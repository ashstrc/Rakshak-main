#include "Video.h"

#include <WiFi.h>
#include <JPEGDEC.h>
#include <esp_heap_caps.h>

#include "Display_SPD2010.h"


/* ============================================================
 * RAKSHAK WATCH 1
 * VIDEO RECEIVER
 *
 * XIAO ESP32-S3 Sense
 *       |
 *       | HTTP MJPEG /stream
 *       v
 * Watch1 Wi-Fi
 *       |
 *       +--> MJPEG parser
 *       |
 *       +--> JPEGDEC
 *       |
 *       +--> 640x480
 *       |      center crop
 *       |      480x480
 *       |
 *       +--> scale
 *       |      480x480 -> 412x412
 *       |
 *       +--> RGB565
 *       |
 *       v
 * SPD2010
 *
 * DISPLAY TRANSFER:
 *       cropBuffer (PSRAM)
 *              |
 *              v
 *       2-row internal DMA buffer
 *              |
 *              v
 *       LCD_addWindow()
 *
 * ============================================================ */


/* ============================================================
 * CAMERA CONFIGURATION
 * ============================================================ */

static const char *VIDEO_HOST = "10.206.84.121";
static const uint16_t VIDEO_PORT = 80;
static const char *VIDEO_PATH = "/stream";


/* ============================================================
 * IMAGE CONFIGURATION
 * ============================================================ */

static const int SOURCE_WIDTH  = 640;
static const int SOURCE_HEIGHT = 480;

static const int CROP_X = 80;
static const int CROP_Y = 0;

static const int CROP_WIDTH  = 480;
static const int CROP_HEIGHT = 480;

static const int DISPLAY_WIDTH  = 412;
static const int DISPLAY_HEIGHT = 412;


/*
 * Maximum expected JPEG frame size.
 *
 * XIAO:
 * VGA 640x480
 * JPEG quality 15
 */
static const size_t MAX_JPEG_SIZE = 256 * 1024;


/* ============================================================
 * LCD DMA TRANSFER CONFIGURATION
 * ============================================================ */

/*
 * Display_SPD2010.cpp has:
 *
 * max_transfer_sz = 2048
 *
 * 412 pixels * 2 rows * 2 bytes = 1648 bytes
 *
 * Therefore 2 rows safely fit inside the configured
 * maximum transfer size.
 */
static const int LCD_TRANSFER_LINES = 8;

static const size_t LCD_TRANSFER_BUFFER_SIZE =
    DISPLAY_WIDTH *
    LCD_TRANSFER_LINES *
    sizeof(uint16_t);


/*
 * Small internal RAM + DMA-capable buffer.
 *
 * IMPORTANT:
 *
 * This buffer is NOT in PSRAM.
 *
 * It is reused only after a short guard delay so the LCD
 * DMA has time to finish reading it.
 */
static uint16_t *lcdTransferBuffer = nullptr;


/* ============================================================
 * NETWORK
 * ============================================================ */

static WiFiClient videoClient;

static bool videoStarted = false;
static bool httpHeadersDone = false;


/* ============================================================
 * JPEG FRAME BUFFER
 * ============================================================ */

static uint8_t *jpegBuffer = nullptr;
static size_t jpegLength = 0;


/* ============================================================
 * DECODED CROP BUFFER
 *
 * 480 x 480 x 2 bytes
 * = 460,800 bytes
 *
 * Stored in PSRAM.
 * ============================================================ */

static uint16_t *cropBuffer = nullptr;


/* ============================================================
 * JPEGDEC
 * ============================================================ */

static JPEGDEC jpeg;


/*
 * JPEGDEC requires a draw callback.
 *
 * Framebuffer mode writes directly into cropBuffer.
 */
static int JPEG_Draw_Callback(JPEGDRAW *pDraw)
{
    return 1;
}


/* ============================================================
 * JPEG PARSER STATE
 * ============================================================ */

static bool receivingJPEG = false;
static bool previousWasFF = false;


/* ============================================================
 * HTTP HEADER STATE
 * ============================================================ */

static char httpHeaderBuffer[512];
static size_t httpHeaderLength = 0;


/* ============================================================
 * MEMORY CLEANUP
 * ============================================================ */

static void Video_FreeBuffers()
{
    if (jpegBuffer)
    {
        free(jpegBuffer);
        jpegBuffer = nullptr;
    }

    if (cropBuffer)
    {
        free(cropBuffer);
        cropBuffer = nullptr;
    }

    if (lcdTransferBuffer)
    {
        heap_caps_free(lcdTransferBuffer);
        lcdTransferBuffer = nullptr;
    }

    jpegLength = 0;
}


/* ============================================================
 * MEMORY ALLOCATION
 * ============================================================ */

static bool Video_AllocateBuffers()
{
    if (!psramFound())
    {
        Serial.println("[VIDEO] ERROR: PSRAM not found");
        return false;
    }

    Serial.println("[VIDEO] PSRAM detected");


    /*
     * --------------------------------------------------------
     * JPEG compressed frame buffer
     * --------------------------------------------------------
     */

    jpegBuffer = (uint8_t *)ps_malloc(MAX_JPEG_SIZE);

    if (!jpegBuffer)
    {
        Serial.println("[VIDEO] ERROR: JPEG buffer allocation failed");

        Video_FreeBuffers();
        return false;
    }


    /*
     * --------------------------------------------------------
     * 480x480 RGB565 crop buffer
     * --------------------------------------------------------
     */

    cropBuffer = (uint16_t *)ps_malloc(
        CROP_WIDTH *
        CROP_HEIGHT *
        sizeof(uint16_t)
    );

    if (!cropBuffer)
    {
        Serial.println("[VIDEO] ERROR: crop buffer allocation failed");

        Video_FreeBuffers();
        return false;
    }


    /*
     * --------------------------------------------------------
     * Small LCD DMA buffer
     *
     * MUST be internal + DMA capable.
     * --------------------------------------------------------
     */

    lcdTransferBuffer =
        (uint16_t *)heap_caps_malloc(
            LCD_TRANSFER_BUFFER_SIZE,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA
        );

    if (!lcdTransferBuffer)
    {
        Serial.println(
            "[VIDEO] ERROR: LCD DMA buffer allocation failed"
        );

        Video_FreeBuffers();
        return false;
    }


    /*
     * Clear transfer buffer.
     */

    memset(
        lcdTransferBuffer,
        0,
        LCD_TRANSFER_BUFFER_SIZE
    );


    Serial.println("[VIDEO] Buffers allocated");

    Serial.print("[VIDEO] JPEG buffer: ");
    Serial.print(MAX_JPEG_SIZE / 1024);
    Serial.println(" KB");

    Serial.print("[VIDEO] Crop buffer: ");
    Serial.print(
        (CROP_WIDTH *
         CROP_HEIGHT *
         sizeof(uint16_t)) / 1024
    );
    Serial.println(" KB");

    Serial.print("[VIDEO] LCD DMA buffer: ");
    Serial.print(LCD_TRANSFER_BUFFER_SIZE);
    Serial.println(" bytes");

    return true;
}


/* ============================================================
 * HTTP CONNECTION
 * ============================================================ */

static bool Video_Connect()
{
    if (videoClient.connected())
        return true;


    Serial.println("[VIDEO] Connecting to XIAO...");


    if (!videoClient.connect(
            VIDEO_HOST,
            VIDEO_PORT))
    {
        Serial.println(
            "[VIDEO] ERROR: connection failed"
        );

        return false;
    }


    /*
     * Reset HTTP parser state.
     */

    httpHeadersDone = false;
    httpHeaderLength = 0;

    receivingJPEG = false;
    previousWasFF = false;
    jpegLength = 0;


    /*
     * HTTP MJPEG request.
     */

    videoClient.print(
        "GET "
    );

    videoClient.print(
        VIDEO_PATH
    );

    videoClient.print(
        " HTTP/1.1\r\n"
        "Host: "
    );

    videoClient.print(
        VIDEO_HOST
    );

    videoClient.print(
        "\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n"
    );


    videoClient.setTimeout(100);


    Serial.println(
        "[VIDEO] HTTP request sent"
    );

    return true;
}


/* ============================================================
 * HTTP HEADER PARSER
 * ============================================================ */

static bool Video_ProcessHTTPHeaders()
{
    while (videoClient.available())
    {
        char c = (char)videoClient.read();


        /*
         * Prevent abnormal headers from consuming memory.
         */

        if (httpHeaderLength <
            sizeof(httpHeaderBuffer) - 1)
        {
            httpHeaderBuffer[
                httpHeaderLength++
            ] = c;

            httpHeaderBuffer[
                httpHeaderLength
            ] = '\0';
        }
        else
        {
            httpHeaderLength = 0;
        }


        /*
         * Detect:
         *
         * \r\n
         * \r\n
         */

        if (httpHeaderLength >= 4 &&
            httpHeaderBuffer[
                httpHeaderLength - 4
            ] == '\r' &&
            httpHeaderBuffer[
                httpHeaderLength - 3
            ] == '\n' &&
            httpHeaderBuffer[
                httpHeaderLength - 2
            ] == '\r' &&
            httpHeaderBuffer[
                httpHeaderLength - 1
            ] == '\n')
        {
            httpHeaderLength = 0;

            httpHeadersDone = true;

            Serial.println(
                "[VIDEO] HTTP headers received"
            );

            return true;
        }
    }

    return false;
}


/* ============================================================
 * JPEG FRAME PARSER
 *
 * SOI = FF D8
 * EOI = FF D9
 * ============================================================ */

static bool Video_ReadJPEGFrame()
{
    while (videoClient.available())
    {
        int value = videoClient.read();

        if (value < 0)
            break;


        uint8_t byte = (uint8_t)value;


        /* ----------------------------------------------------
         * WAITING FOR JPEG START
         * ---------------------------------------------------- */

        if (!receivingJPEG)
        {
            if (previousWasFF &&
                byte == 0xD8)
            {
                jpegBuffer[0] = 0xFF;
                jpegBuffer[1] = 0xD8;

                jpegLength = 2;

                receivingJPEG = true;
                previousWasFF = false;

                continue;
            }


            previousWasFF =
                (byte == 0xFF);

            continue;
        }


        /* ----------------------------------------------------
         * RECEIVING JPEG
         * ---------------------------------------------------- */

        if (jpegLength >= MAX_JPEG_SIZE)
        {
            Serial.println(
                "[VIDEO] JPEG frame too large"
            );

            receivingJPEG = false;
            previousWasFF = false;
            jpegLength = 0;

            continue;
        }


        jpegBuffer[
            jpegLength++
        ] = byte;


        /* ----------------------------------------------------
         * JPEG END
         * ---------------------------------------------------- */

        if (previousWasFF &&
            byte == 0xD9)
        {
            receivingJPEG = false;
            previousWasFF = false;

            return true;
        }


        previousWasFF =
            (byte == 0xFF);
    }


    return false;
}


/* ============================================================
 * DECODE JPEG
 *
 * 640x480
 *     |
 *     +--> crop x=80,y=0,w=480,h=480
 *     |
 *     v
 * 480x480 RGB565
 * ============================================================ */

static bool Video_DecodeJPEG()
{
    if (jpegLength < 4)
        return false;


    if (!jpeg.openRAM(
            jpegBuffer,
            jpegLength,
            JPEG_Draw_Callback))
    {
        Serial.print(
            "[VIDEO] JPEG open failed, error="
        );

        Serial.println(
            jpeg.getLastError()
        );

        return false;
    }


    int width  = jpeg.getWidth();
    int height = jpeg.getHeight();


    /*
     * The XIAO is expected to send VGA.
     */

    if (width != SOURCE_WIDTH ||
        height != SOURCE_HEIGHT)
    {
        Serial.print(
            "[VIDEO] Unexpected JPEG size: "
        );

        Serial.print(width);
        Serial.print("x");
        Serial.println(height);

        jpeg.close();

        return false;
    }


    /*
     * --------------------------------------------------------
     * CENTER CROP
     *
     * 640 - 480 = 160
     * 160 / 2 = 80
     * --------------------------------------------------------
     */

    jpeg.setCropArea(
        CROP_X,
        CROP_Y,
        CROP_WIDTH,
        CROP_HEIGHT
    );


    /*
     * Verify JPEGDEC accepted the requested crop.
     */

    int actualX;
    int actualY;
    int actualW;
    int actualH;

    jpeg.getCropArea(
        &actualX,
        &actualY,
        &actualW,
        &actualH
    );


    if (actualX != CROP_X ||
        actualY != CROP_Y ||
        actualW != CROP_WIDTH ||
        actualH != CROP_HEIGHT)
    {
        Serial.print(
            "[VIDEO] Crop adjusted to: "
        );

        Serial.print(actualX);
        Serial.print(",");
        Serial.print(actualY);

        Serial.print(" ");

        Serial.print(actualW);
        Serial.print("x");
        Serial.println(actualH);

        jpeg.close();

        return false;
    }


    /*
     * RGB565.
     */

   jpeg.setPixelType(RGB565_BIG_ENDIAN);


    /*
     * Decode directly into PSRAM crop buffer.
     */

    jpeg.setFramebuffer(
        cropBuffer
    );


    bool decoded = jpeg.decode(
        0,
        0,
        0
    );


    int decodeError =
        jpeg.getLastError();


    jpeg.close();


    if (!decoded)
    {
        Serial.print(
            "[VIDEO] JPEG decode failed, error="
        );

        Serial.println(
            decodeError
        );

        return false;
    }


    return true;
}


/* ============================================================
 * SCALE + DISPLAY
 *
 * We intentionally DO NOT create a 412x412 PSRAM display
 * framebuffer anymore.
 *
 * Instead:
 *
 * cropBuffer
 *     |
 *     | nearest-neighbour scale
 *     v
 * 2 rows
 *     |
 *     v
 * internal DMA buffer
 *     |
 *     v
 * LCD
 *
 * This means the LCD DMA never reads from a large PSRAM
 * framebuffer that is being reused for the next frame.
 * ============================================================ */

static void Video_DisplayFrame()
{
    if (!lcdTransferBuffer)
        return;


    /*
     * Send 2 rows at a time.
     *
     * 412 x 2 x 2 bytes = 1648 bytes
     *
     * Display driver maximum:
     * 2048 bytes
     */

    for (
        int displayY = 0;
        displayY < DISPLAY_HEIGHT;
        displayY += LCD_TRANSFER_LINES
    )
    {
        int lines =
            DISPLAY_HEIGHT - displayY;

        if (lines > LCD_TRANSFER_LINES)
            lines = LCD_TRANSFER_LINES;


        /*
         * ----------------------------------------------------
         * SCALE CURRENT ROWS DIRECTLY INTO INTERNAL DMA BUFFER
         * ----------------------------------------------------
         */

        for (
            int localY = 0;
            localY < lines;
            localY++
        )
        {
            int y =
                displayY + localY;


            /*
             * Nearest-neighbour Y mapping.
             */

            int sourceY =
                (y * CROP_HEIGHT) /
                DISPLAY_HEIGHT;


            const uint16_t *sourceRow =
                cropBuffer +
                (sourceY * CROP_WIDTH);


            uint16_t *destRow =
                lcdTransferBuffer +
                (localY * DISPLAY_WIDTH);


            /*
             * Nearest-neighbour X mapping.
             */

            for (
                int x = 0;
                x < DISPLAY_WIDTH;
                x++
            )
            {
                int sourceX =
                    (x * CROP_WIDTH) /
                    DISPLAY_WIDTH;


                destRow[x] =
                    sourceRow[sourceX];
            }
        }


        /*
         * ----------------------------------------------------
         * SEND ONLY THIS SMALL CHUNK
         * ----------------------------------------------------
         */

        LCD_addWindow(
            0,
            displayY,
            DISPLAY_WIDTH - 1,
            displayY + lines - 1,
            lcdTransferBuffer
        );


        /*
         * ----------------------------------------------------
         * IMPORTANT
         *
         * LCD_addWindow() ultimately queues the LCD transfer.
         *
         * We must NOT immediately overwrite
         * lcdTransferBuffer.
         *
         * The current Display_SPD2010 driver does not expose
         * an on_color_trans_done callback, so this conservative
         * guard gives the DMA time to consume the chunk.
         *
         * 1 ms is intentionally used here as the initial
         * conservative value.
         * ----------------------------------------------------
         */

        delay(1);
    }
}


/* ============================================================
 * PUBLIC API
 * ============================================================ */

void Video_Begin()
{
    if (videoStarted)
        return;


    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "RAKSHAK VIDEO INIT"
    );

    Serial.println(
        "================================"
    );


    /*
     * Wi-Fi must already be connected.
     */

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "[VIDEO] WiFi is not connected"
        );

        Serial.println(
            "================================"
        );

        return;
    }


    /*
     * Allocate buffers.
     */

    if (!Video_AllocateBuffers())
    {
        Serial.println(
            "[VIDEO] Buffer allocation failed"
        );

        Serial.println(
            "================================"
        );

        return;
    }


    /*
     * Connect to XIAO.
     */

    if (!Video_Connect())
    {
        Serial.println(
            "[VIDEO] Camera connection failed"
        );

        Video_FreeBuffers();

        Serial.println(
            "================================"
        );

        return;
    }


    videoStarted = true;


    Serial.println(
        "[VIDEO] READY"
    );

    Serial.print(
        "[VIDEO] Source: http://"
    );

    Serial.print(
        VIDEO_HOST
    );

    Serial.println(
        "/stream"
    );

    Serial.println(
        "================================"
    );
}


/* ============================================================
 * VIDEO UPDATE
 * ============================================================ */

void Video_Update()
{
    if (!videoStarted)
        return;


    /*
     * --------------------------------------------------------
     * CAMERA DISCONNECTED
     * --------------------------------------------------------
     */

    if (!videoClient.connected())
    {
        Serial.println(
            "[VIDEO] Camera disconnected"
        );


        videoClient.stop();


        httpHeadersDone = false;
        httpHeaderLength = 0;

        receivingJPEG = false;
        previousWasFF = false;

        jpegLength = 0;


        Video_Connect();

        return;
    }


    /*
     * --------------------------------------------------------
     * HTTP HEADERS
     * --------------------------------------------------------
     */

    if (!httpHeadersDone)
    {
        Video_ProcessHTTPHeaders();

        if (!httpHeadersDone)
            return;
    }


    /*
     * --------------------------------------------------------
     * RECEIVE COMPLETE JPEG
     * --------------------------------------------------------
     */

    if (!Video_ReadJPEGFrame())
        return;


    /*
     * --------------------------------------------------------
     * DECODE
     *
     * 640x480
     *     ->
     * 480x480 crop
     * --------------------------------------------------------
     */

    if (!Video_DecodeJPEG())
    {
        jpegLength = 0;
        return;
    }


    /*
     * --------------------------------------------------------
     * SCALE + DISPLAY
     *
     * 480x480
     *     ->
     * 412x412
     * --------------------------------------------------------
     */

    Video_DisplayFrame();


    /*
     * --------------------------------------------------------
     * FRAME COMPLETE
     * --------------------------------------------------------
     */

    jpegLength = 0;
}


/* ============================================================
 * STOP
 * ============================================================ */

void Video_Stop()
{
    /*
     * Stop the network stream even if the public video state
     * has already been cleared.
     */

    videoClient.stop();


    videoStarted = false;

    httpHeadersDone = false;
    httpHeaderLength = 0;

    receivingJPEG = false;
    previousWasFF = false;

    jpegLength = 0;


    Video_FreeBuffers();


    Serial.println(
        "[VIDEO] Stopped"
    );
}