#pragma once
#include <Arduino.h>

void Video_Begin();
void Video_Update();
void Video_Stop();

void Video_RequestStart();
void Video_RequestStop();
bool Video_IsActive();