#pragma once
#include <Arduino.h>

void Speech_Init();
void Speech_Update();

void Speech_Play(const char* file);
void Speech_PlayWord(const char* word);