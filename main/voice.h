#pragma once
#include <Arduino.h>

void Voice_Init();
void Voice_Update();

void Voice_HandleCommand(const char* cmd);