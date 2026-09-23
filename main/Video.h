#pragma once

#include <Arduino.h>

/*
 * RAKSHAK WATCH 1
 * Video receiver interface
 *
 * XIAO ESP32-S3 Sense
 *        ↓ HTTP MJPEG
 * Watch1 Wi-Fi
 *        ↓
 * MJPEG parser
 *        ↓
 * JPEGDEC
 *        ↓
 * 640x480 → center crop 480x480
 *        ↓
 * 480x480 → 412x412
 *        ↓
 * RGB565 → SPD2010
 */

void Video_Begin();
void Video_Update();
void Video_Stop();