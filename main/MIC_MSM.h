#pragma once

#include "ESP_I2S.h"

// --------------------------------------------------
// Waveshare ESP32-S3-Touch-LCD-1.46
// Onboard Microphone I2S Pins
// --------------------------------------------------

#define I2S_PIN_BCK   15
#define I2S_PIN_WS    2
#define I2S_PIN_DOUT  -1
#define I2S_PIN_DIN   39

// --------------------------------------------------
// Microphone initialization
// --------------------------------------------------

void MIC_Init(void);