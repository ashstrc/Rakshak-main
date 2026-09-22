#pragma once

#include "ESP_I2S.h"

#define I2S_PIN_BCK   15
#define I2S_PIN_WS    2
#define I2S_PIN_DOUT  -1
#define I2S_PIN_DIN   39

void MIC_Init(void);

void MIC_SetEnabled(bool enabled);

bool MIC_IsEnabled();