#include "MIC_MSM.h"
#include "voice.h"
#include <math.h>
#include <esp_heap_caps.h>

#include <RAKSHAK_Voice_Commands_inferencing.h>

/* =========================================================
   EDGE IMPULSE PSRAM ALLOCATOR
   ========================================================= */

void *ei_calloc(size_t nitems, size_t size)
{
    return heap_caps_calloc(
        nitems,
        size,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
}

/* =========================================================
   I2S
   ========================================================= */

I2SClass i2s;

/* =========================================================
   MIC STATE
   ========================================================= */

static volatile bool micEnabled = false;
static volatile bool micReady = false;

/* =========================================================
   EDGE IMPULSE AUDIO BUFFER
   ========================================================= */

static int16_t *eiAudioBuffer = nullptr;
static size_t eiAudioIndex = 0;

static bool eiInferenceRunning = false;

/* =========================================================
   EDGE IMPULSE SIGNAL CALLBACK
   ========================================================= */

static int ei_get_data(
    size_t offset,
    size_t length,
    float *out_ptr
)
{
    if(eiAudioBuffer == nullptr)
        return -1;

    numpy::int16_to_float(
        &eiAudioBuffer[offset],
        out_ptr,
        length
    );

    return 0;
}

/* =========================================================
   EDGE IMPULSE CLASSIFICATION
   ========================================================= */

static void EI_RunInference()
{
    if(eiAudioBuffer == nullptr)
        return;

    Serial.println();
    Serial.println("================================");
    Serial.println("EDGE IMPULSE CLASSIFICATION");
    Serial.println("================================");

    signal_t signal;

    signal.total_length =
        EI_CLASSIFIER_RAW_SAMPLE_COUNT;

    signal.get_data =
        &ei_get_data;

    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR res =
        run_classifier(
            &signal,
            &result,
            false
        );

    if(res != EI_IMPULSE_OK)
    {
        Serial.print("EI CLASSIFIER ERROR: ");
        Serial.println((int)res);

        return;
    }

    Serial.println("PREDICTIONS:");

    for(size_t i = 0;
        i < EI_CLASSIFIER_LABEL_COUNT;
        i++)
    {
        Serial.print("  ");
        Serial.print(
            result.classification[i].label
        );

        Serial.print(" : ");

        Serial.println(
            result.classification[i].value,
            4
        );
    }

        /* =========================================================
       RAKSHAK VOICE COMMAND BRIDGE
       ========================================================= */

    size_t bestIndex = 0;
    float bestScore = 0.0f;

    for(size_t i = 0;
        i < EI_CLASSIFIER_LABEL_COUNT;
        i++)
    {
        if(result.classification[i].value > bestScore)
        {
            bestScore =
                result.classification[i].value;

            bestIndex = i;
        }
    }

    const char *label =
        result.classification[bestIndex].label;

    Serial.print("BEST LABEL: ");
    Serial.print(label);

    Serial.print(" | SCORE: ");
    Serial.println(bestScore, 4);

    if(bestScore >= 0.70f)
    {
        if(strcmp(label, "help") == 0)
        {
            Voice_HandleCommand(
                "ZORO SEND HELP"
            );
        }
        else if(strcmp(label, "enemy") == 0)
        {
            Voice_HandleCommand(
                "ZORO SEND ENEMY"
            );
        }
        else if(strcmp(label, "fallback") == 0)
        {
            Voice_HandleCommand(
                "ZORO SEND FALLBACK"
            );
        }
        else if(strcmp(label, "ambush") == 0)
        {
            Voice_HandleCommand(
                "ZORO SEND AMBUSH"
            );
        }
        else if(strcmp(label, "status") == 0)
        {
            Voice_HandleCommand(
                "ZORO STATUS"
            );
        }
        else if(strcmp(label, "Read") == 0)
        {
            Voice_HandleCommand(
                "ZORO READ"
            );
        }
    }
    else
    {
        Serial.println(
            "VOICE: confidence too low - ignored"
        );
    }

    Serial.print("DSP: ");
    Serial.print(result.timing.dsp);

    Serial.print(" ms | CLASSIFICATION: ");
    Serial.print(result.timing.classification);

    Serial.println(" ms");

    Serial.println("================================");
}

/* =========================================================
   MIC INITIALIZATION
   ========================================================= */

void _MIC_Init()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("MIC INIT");
    Serial.println("================================");

    i2s.setPins(
        I2S_PIN_BCK,
        I2S_PIN_WS,
        I2S_PIN_DOUT,
        I2S_PIN_DIN
    );

    i2s.setTimeout(1000);

    /*
       Waveshare microphone:

       BCLK : GPIO 15
       WS   : GPIO 2
       DATA : GPIO 39

       Edge Impulse expects:
       16 kHz
       16-bit
       mono PCM

       ESP32 Arduino 3.3.11 provides
       explicit mono + RIGHT slot handling.
    */

    bool ok = i2s.begin(
        I2S_MODE_STD,
        16000,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO,
        I2S_STD_SLOT_RIGHT
    );

    if(!ok)
    {
        Serial.println("MIC I2S INIT FAILED");

        micReady = false;

        return;
    }

    micReady = true;

    Serial.println("MIC I2S READY");
    Serial.println("MIC BCLK : GPIO 15");
    Serial.println("MIC WS   : GPIO 2");
    Serial.println("MIC DATA : GPIO 39");

    Serial.println("MIC CAPTURE READY");

    Serial.print("EI SAMPLE COUNT : ");
    Serial.println(
        EI_CLASSIFIER_RAW_SAMPLE_COUNT
    );

    Serial.println("EDGE IMPULSE READY");
}

/* =========================================================
   MIC TASK
   ========================================================= */

void MICTask(void *parameter)
{
    (void)parameter;

    _MIC_Init();

    int16_t buffer[256];

    uint32_t lastReport = 0;

    while(true)
    {
        if(!micEnabled || !micReady)
        {
            vTaskDelay(
                pdMS_TO_TICKS(20)
            );

            continue;
        }

        size_t bytesRead =
            i2s.readBytes(
                (char *)buffer,
                sizeof(buffer)
            );

        if(bytesRead == 0)
        {
            vTaskDelay(
                pdMS_TO_TICKS(5)
            );

            continue;
        }

        size_t samples =
            bytesRead /
            sizeof(int16_t);

        /* =================================================
           RMS
           ================================================= */

        double sumSquares = 0;

        for(size_t i = 0;
            i < samples;
            i++)
        {
            double sample =
                buffer[i];

            sumSquares +=
                sample * sample;
        }

        double rms = 0;

        if(samples > 0)
        {
            rms =
                sqrt(
                    sumSquares /
                    samples
                );
        }

        /* =================================================
           EDGE IMPULSE BUFFER
           ================================================= */

        if(eiAudioBuffer == nullptr)
        {
            eiAudioBuffer =
                (int16_t *)malloc(
                    EI_CLASSIFIER_RAW_SAMPLE_COUNT *
                    sizeof(int16_t)
                );

            if(eiAudioBuffer == nullptr)
            {
                Serial.println(
                    "EI ERROR: AUDIO BUFFER ALLOCATION FAILED"
                );

                micEnabled = false;

                continue;
            }

            eiAudioIndex = 0;

            Serial.print(
                "EI AUDIO BUFFER ALLOCATED: "
            );

            Serial.println(
                EI_CLASSIFIER_RAW_SAMPLE_COUNT
            );
        }

        /*
           IMPORTANT:

           Do NOT multiply the microphone samples here.

           The previous x8 experiment changed the
           classifier distribution but did not produce
           reliable keyword recognition.

           The Edge Impulse signal receives the native
           16-bit PCM samples from the Waveshare mic.
        */

        for(size_t i = 0; i < samples; i++)
    {
        if(eiAudioIndex < EI_CLASSIFIER_RAW_SAMPLE_COUNT)
        {
            int32_t amplified = (int32_t)buffer[i];

        // Prevent int16 overflow/clipping
            if(amplified > 32767)
                amplified = 32767;

            if(amplified < -32768)
                amplified = -32768;

        eiAudioBuffer[eiAudioIndex++] = (int16_t)amplified;
        }
    }

        /* =================================================
           ONE SECOND COMPLETE
           ================================================= */

        if(eiAudioIndex >=
           EI_CLASSIFIER_RAW_SAMPLE_COUNT)
        {
            Serial.println("EI RAW SAMPLES:");

            for(int i = 0; i < 20; i++)
            {
                Serial.print(
                    eiAudioBuffer[i]
                );

                Serial.print(" ");
            }

            Serial.println();

            if(!eiInferenceRunning)
            {
                eiInferenceRunning = true;

                Serial.println();
                Serial.println(
                    "EI: 16000 SAMPLES CAPTURED"
                );

                Serial.println(
                    "EI: RUNNING INFERENCE..."
                );

                EI_RunInference();

                eiAudioIndex = 0;

                eiInferenceRunning = false;
            }
        }

        /* =================================================
           RMS REPORT
           ================================================= */

        uint32_t now = millis();

        if(now - lastReport >= 250)
        {
            lastReport = now;

            Serial.print(
                "MIC ACTIVE | BYTES="
            );

            Serial.print(
                bytesRead
            );

            Serial.print(
                " | RMS="
            );

            Serial.println(
                rms,
                1
            );
        }
    }

    vTaskDelete(NULL);
}

/* =========================================================
   PUBLIC MIC CONTROL
   ========================================================= */

void MIC_Init()
{
    xTaskCreatePinnedToCore(
        MICTask,
        "MICTask",
        4096,
        NULL,
        5,
        NULL,
        0
    );
}

void MIC_SetEnabled(bool enabled)
{
    if(!micReady)
    {
        Serial.println(
            "MIC NOT READY"
        );

        return;
    }

    micEnabled = enabled;

    /*
       Start a fresh one-second inference window
       whenever the microphone is enabled.
    */

    if(micEnabled)
    {
        eiAudioIndex = 0;

        Serial.println();
        Serial.println(
            "=============================="
        );

        Serial.println(
            "MIC ENABLED"
        );

        Serial.println(
            "LISTENING..."
        );

        Serial.println(
            "=============================="
        );
    }
    else
    {
        eiAudioIndex = 0;

        Serial.println();
        Serial.println(
            "=============================="
        );

        Serial.println(
            "MIC DISABLED"
        );

        Serial.println(
            "MIC OFF"
        );

        Serial.println(
            "=============================="
        );
    }
}

bool MIC_IsEnabled()
{
    return micEnabled;
}