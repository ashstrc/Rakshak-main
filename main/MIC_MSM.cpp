#include "MIC_MSM.h"
#include <math.h>

I2SClass i2s;

static volatile bool micEnabled = false;
static volatile bool micReady = false;


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

    bool ok = i2s.begin(
        I2S_MODE_STD,
        16000,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO
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
    Serial.println("ESP-SR DISABLED");
}


/* =========================================================
   MIC TASK
   ========================================================= */

void MICTask(void *parameter)
{
    (void)parameter;

    _MIC_Init();

    /*
     * Stereo 16-bit:
     *
     * 2 bytes per sample
     * 2 channels
     *
     * 512 bytes = 256 int16 values
     */

    int16_t buffer[256];

    uint32_t lastReport = 0;

    while(true)
    {
        /*
         * MIC OFF
         */
        if(!micEnabled || !micReady)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }


        /*
         * Read microphone samples
         */
        size_t bytesRead =
            i2s.readBytes(
                (char *)buffer,
                sizeof(buffer)
            );

        if(bytesRead == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }


        /*
         * Calculate simple RMS level.
         */

        size_t samples =
            bytesRead / sizeof(int16_t);

        double sumSquares = 0;

        for(size_t i = 0; i < samples; i++)
        {
            double sample = buffer[i];

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


        /*
         * Print level every 250 ms.
         */

        uint32_t now = millis();

        if(now - lastReport >= 250)
        {
            lastReport = now;

            Serial.print("MIC ACTIVE | BYTES=");
            Serial.print(bytesRead);

            Serial.print(" | RMS=");
            Serial.println(rms, 1);
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
        Serial.println("MIC NOT READY");
        return;
    }

    micEnabled = enabled;

    if(micEnabled)
    {
        Serial.println();
        Serial.println("==============================");
        Serial.println("MIC ENABLED");
        Serial.println("LISTENING...");
        Serial.println("==============================");
    }
    else
    {
        Serial.println();
        Serial.println("==============================");
        Serial.println("MIC DISABLED");
        Serial.println("MIC OFF");
        Serial.println("==============================");
    }
}


bool MIC_IsEnabled()
{
    return micEnabled;
}