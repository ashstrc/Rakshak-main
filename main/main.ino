#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"
#include <lvgl.h>
#include <Wire.h>

#include "power.h"
#include "Display_SPD2010.h"
#include "Touch_SPD2010.h"
#include "LVGL_Driver.h"

#include "sensors.h"
#include "gps.h"
#include "voice.h"

#include "speech.h"
#include "number_speech.h"
#include "Audio_PCM5101.h"

#include "LVGL_Example.h"

#include "SD_Card.h"
#include "TCA9554PWR.h"

#define SDA_PIN 11
#define SCL_PIN 10

/* ---------- PAGE TRACK (SYNC WITH UI) ---------- */
extern PageType currentPage;  // from LVGL_Example.cpp

/* ---------- RADAR STORAGE ---------- */
RadarData radarTargets[10];
int radarCount = 0;


/* ------------------------------------------------ */
/* SETUP                                            */
/* ------------------------------------------------ */

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("RAKSHAK BOOT");
  Serial.println("STEP 1: BOOT OK");

  Power_Init();

  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  delay(100);

  TCA9554PWR_Init();
  SD_Init();

  Audio_Init();
  Speech_Init();

  LCD_Init();
  Backlight_Init();
  Set_Backlight(100);

  delay(100);
  Touch_Init();

  Lvgl_Init();

  /* 🔥 START WITH RADAR PAGE */
  Radar_UI();

  Sensors_Init();
  GPS_Init();
  Voice_Init();
}

/* ------------------------------------------------ */
/* LOOP                                             */
/* ------------------------------------------------ */

void loop() {
  Power_Update();

  /* 🔥 TOUCH FIRST (FAST RESPONSE) */
  Touch_Loop();

  /* 🔥 GPS PRIORITY */
  for (int i = 0; i < 10; i++)
    GPS_Update();

  Sensors_Update();
  update_status_data();
  Voice_Update();
  Speech_Update();

  Lvgl_Loop();

  /* ------------------------------------------------ */
  /* UART → RADAR DATA                                */
  /* ------------------------------------------------ */

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("RADAR")) {
      line = line.substring(6);

      int end = line.indexOf(",END");
      if (end != -1) line = line.substring(0, end);

      String t[3];
      int i = 0;

      while (line.length() && i < 3) {
        int c = line.indexOf(',');
        if (c == -1) {
          t[i++] = line;
          break;
        }

        t[i++] = line.substring(0, c);
        line = line.substring(c + 1);
      }

      if (i == 3) {
        uint8_t id = t[0].toInt();
        float heading = t[1].toFloat();
        float dist = t[2].toFloat();

        bool found = false;

        for (int j = 0; j < radarCount; j++) {
          if (radarTargets[j].id == id) {
            radarTargets[j].heading = heading;
            radarTargets[j].distance = dist;
            found = true;
            break;
          }
        }

        if (!found && radarCount < 10) {
          radarTargets[radarCount++] = { id, heading, dist };
        }
      }
    }
  }

  /* ------------------------------------------------ */
  /* RADAR UPDATE (ONLY WHEN ON RADAR PAGE)           */
  /* ------------------------------------------------ */

  static uint32_t radarTimer = 0;

  if (millis() - radarTimer > 100)  // 🔥 smoother update
  {
    radarTimer = millis();

    if (currentPage == PAGE_RADAR)  // PAGE_RADAR
    {
      Radar_Update(radarTargets, radarCount, Sensors_GetYaw());
    }
  }

  /* ------------------------------------------------ */
  /* SEND WATCH DATA                                  */
  /* ------------------------------------------------ */

  static uint32_t txTimer = 0;

  if (millis() - txTimer > 1000) {
    txTimer = millis();

    float gx, gy, gz;
    Sensors_GetGyro(gx, gy, gz);

    Serial.print("PKT,");

    Serial.print(1);
    Serial.print(",");
    Serial.print(Sensors_GetYaw(), 1);
    Serial.print(",");

    Serial.print(gx, 3);
    Serial.print(",");
    Serial.print(gy, 3);
    Serial.print(",");
    Serial.print(gz, 3);
    Serial.print(",");

    Serial.print(Sensors_GetHeartRate(), 1);
    Serial.print(",");
    Serial.print(Sensors_GetHealthState());
    Serial.print(",");

    Serial.print(GPS_Fix());
    Serial.print(",");
    Serial.print(GPS_Sats());
    Serial.print(",");
    Serial.print(GPS_HDOP(), 2);
    Serial.print(",");

    Serial.print(GPS_Lat(), 6);
    Serial.print(",");
    Serial.print(GPS_Lon(), 6);

    Serial.println(",END");
  }
}