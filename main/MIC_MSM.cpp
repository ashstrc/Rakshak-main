#include "MIC_MSM.h"

I2SClass i2s;

// --------------------------------------------------
// Microphone / ESP-SR initialization
// --------------------------------------------------

void _MIC_Init()
{
    Serial.println();
    Serial.println("================================");
    Serial.println("MIC INIT");
    Serial.println("================================");

    // Onboard microphone:
    // BCLK -> GPIO 15
    // WS   -> GPIO 2
    // DIN  -> GPIO 39
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

    if (!ok)
    {
        Serial.println("MIC I2S INIT FAILED");
        return;
    }

    Serial.println("MIC I2S READY");
    Serial.println("MIC BCLK : GPIO 15");
    Serial.println("MIC WS   : GPIO 2");
    Serial.println("MIC DATA : GPIO 39");

    // ESP-SR will use the microphone stream.
   Serial.println("MIC I2S READY");
   Serial.println("ESP-SR DISABLED FOR MIC TEST");
    Serial.println("================================");
}


// --------------------------------------------------
// Microphone task
// --------------------------------------------------

void MICTask(void *parameter)
{
    _MIC_Init();

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    vTaskDelete(NULL);
}


// --------------------------------------------------
// Public initialization
// --------------------------------------------------

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