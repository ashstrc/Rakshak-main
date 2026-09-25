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

#include "MIC_MSM.h"

#include "Video.h"
#include "ESPNow.h"
#include "network_fix.h"

#include "esp_heap_caps.h"

#define VIDEO_BOTTOM_TOUCH_ZONE 342

/* =========================================================
   WIFI
   ========================================================= */

const char *WIFI_SSID = "Mohit";
const char *WIFI_PASSWORD = "mohit0604";


void WiFi_Init()
{
  Serial.println();
  Serial.println("==============================");
  Serial.println("WIFI INIT");
  Serial.println("==============================");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  Serial.print("[WIFI] Connecting");

  uint32_t start = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - start < 15000
  )
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();


  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("[WIFI] Connected");

    Serial.print("[WIFI] IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("[WIFI] RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
  else
  {
    Serial.println("[WIFI] Connection failed");

    Serial.print("[WIFI] Status: ");
    Serial.println(WiFi.status());
  }
}

/* =========================================================
   I2C
   ========================================================= */

#define SDA_PIN 11
#define SCL_PIN 10


/* =========================================================
   PAGE TRACK
   ========================================================= */

extern PageType currentPage;


/* =========================================================
   RADAR STORAGE
   ========================================================= */

RadarData radarTargets[10];
int radarCount = 0;


/* =========================================================
   VIDEO STATE
   ========================================================= */

bool videoActive = false;
bool videoStarted = false;


void Video_RequestStart()
{
  Serial.println("[VIDEO STATE] REQUEST START");
  videoActive = true;
  Serial.println("[VIDEO STATE] videoActive = TRUE");
}

void Video_RequestStop()
{
  Serial.println("[VIDEO STATE] REQUEST STOP");
  videoActive = false;
  Serial.println("[VIDEO STATE] videoActive = FALSE");
}

bool Video_IsActive()
{
  return videoActive;
}


/* =========================================================
   SETUP
   ========================================================= */

void setup()
{
  Serial.begin(115200);

  delay(2000);

  Serial.println("RAKSHAK BOOT");
  Serial.println("STEP 1: BOOT OK");


  /* -----------------------------------------------------
     POWER
     ----------------------------------------------------- */

  Power_Init();


  /* -----------------------------------------------------
     I2C
     ----------------------------------------------------- */

  Wire.begin(
    SDA_PIN,
    SCL_PIN,
    100000
  );

  delay(100);

   /* -----------------------------------------------------
     POWER CONTROLLER
     ----------------------------------------------------- */

  TCA9554PWR_Init();


  /* -----------------------------------------------------
     SD CARD
     ----------------------------------------------------- */

  SD_Init();


  /* -----------------------------------------------------
     AUDIO
     ----------------------------------------------------- */

  Audio_Init();
  Speech_Init();


  /* -----------------------------------------------------
     DISPLAY
     ----------------------------------------------------- */

  LCD_Init();

  Backlight_Init();

  Set_Backlight(100);


/* -----------------------------------------------------
     TOUCH
     ----------------------------------------------------- */

  delay(100);

  Touch_Init();


  /* -----------------------------------------------------
     LVGL
     ----------------------------------------------------- */

  Lvgl_Init();


  /* -----------------------------------------------------
     WIFI
     ----------------------------------------------------- */

  WiFi_Init();

  Serial.print("[WIFI] Channel: ");
  Serial.println(WiFi.channel());

  Serial.print("[WIFI] MAC: ");
  Serial.println(WiFi.macAddress());

  ESPNow_Init();

  /* -----------------------------------------------------
     START WITH RADAR PAGE
     ----------------------------------------------------- */

  Radar_UI();


  /* -----------------------------------------------------
     SENSORS
     ----------------------------------------------------- */

  Sensors_Init();

  GPS_Init();

  Voice_Init();

  MIC_Init();

  /* -----------------------------------------------------
     PSRAM DEBUG
     ----------------------------------------------------- */

  Serial.print("PSRAM: ");
  Serial.println(
    ESP.getPsramSize()
  );

  Serial.print("FREE PSRAM: ");
  Serial.println(
    ESP.getFreePsram()
  );
}


/* =========================================================
   LOOP
   ========================================================= */

void loop()
{
  /* -----------------------------------------------------
     POWER
     ----------------------------------------------------- */

  Power_Update();


  /* -----------------------------------------------------
     TOUCH
     
     LVGL owns the touch controller.
     Do not call Touch_Loop() here because it would
     read the SPD2010 a second time.
     ----------------------------------------------------- */

  // Touch_Loop();


  /* -----------------------------------------------------
     GPS PRIORITY
     ----------------------------------------------------- */

  for (int i = 0; i < 10; i++)
  {
    GPS_Update();
  }


  /* -----------------------------------------------------
     SENSORS
     ----------------------------------------------------- */

  Sensors_Update();

  update_status_data();


  /* -----------------------------------------------------
     VOICE
     ----------------------------------------------------- */

  Voice_Update();

  Speech_Update();

  ESPNow_Update();


  /* =====================================================
   VIDEO / LVGL DISPLAY MODE
   ===================================================== */

if (videoActive)
{
  /*
   * =====================================================
   * ENTER LIVE VIDEO
   * =====================================================
   */

  if (!videoStarted)
{
    Serial.println("[VIDEO] Starting live video...");

    // Pause ESP-NOW so the Wi-Fi radio is dedicated to camera streaming.
    ESPNow_Stop();

    delay(100);

    Video_Begin();
    videoStarted = true;
} 


  /*
   * =====================================================
   * LIVE VIDEO EXIT TOUCH
   * =====================================================
   *
   * LVGL is intentionally NOT running here.
   * Video.cpp owns the LCD.
   */

  static bool videoTouching = false;

  uint16_t videoTouchX = 0;
  uint16_t videoTouchY = 0;
  uint16_t videoTouchZ = 0;

  uint8_t videoTouchPoints = 0;

  bool videoPressed = Touch_Get_xy(
    &videoTouchX,
    &videoTouchY,
    &videoTouchZ,
    &videoTouchPoints,
    CONFIG_ESP_LCD_TOUCH_MAX_POINTS
  );


  if (videoPressed && videoTouchPoints > 0)
  {
    if (!videoTouching)
    {
      videoTouching = true;

      Serial.print("[VIDEO TOUCH] X=");
      Serial.print(videoTouchX);

      Serial.print(" Y=");
      Serial.println(videoTouchY);


      /*
       * Bottom edge = exit live video.
       */
      if (videoTouchY > VIDEO_BOTTOM_TOUCH_ZONE)
      {
        Serial.println(
          "[VIDEO] Bottom edge -> stopping video"
        );

        Video_RequestStop();
      }
    }
  }
  else
  {
    videoTouching = false;
  }


  /*
   * =====================================================
   * RENDER CAMERA
   * =====================================================
   */

  if (videoActive)
  {
    Video_Update();
  }
}
else
{
  /*
   * =====================================================
   * RETURN FROM LIVE VIDEO
   * =====================================================
   */

  if (videoStarted)
  {
    Serial.println("[VIDEO] Stopping live video...");

    Video_Stop();
    videoStarted = false;


    /*
     * Show Video Mode again.
     *
     * Radar will NOT appear because the Video Mode
     * page is restored before LVGL renders.
     */


    /*
     * Force LVGL to redraw.
     */
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
  }


  /*
   * Normal LVGL operation.
   */
  Lvgl_Loop();
}


 /* =====================================================
     UART → RADAR DATA
     ===================================================== */

  if (Serial.available())
  {
    String line =
      Serial.readStringUntil('\n');

    line.trim();


    if (line.startsWith("RADAR"))
    {
      line =
        line.substring(6);


      int end =
        line.indexOf(",END");


      if (end != -1)
      {
        line =
          line.substring(0, end);
      }


      String t[3];

      int i = 0;


      while (
        line.length() &&
        i < 3
      )
      {
        int c =
          line.indexOf(',');

           if (c == -1)
        {
          t[i++] = line;

          break;
        }


        t[i++] =
          line.substring(
            0,
            c
          );


        line =
          line.substring(
            c + 1
          );
      }


      if (i == 3)
      {
        uint8_t id =
          t[0].toInt();

        float heading =
          t[1].toFloat();

        float dist =
          t[2].toFloat();


        bool found = false;


        for (
          int j = 0;
          j < radarCount;
          j++
        )
        {
          if (
            radarTargets[j].id == id
            )
          {
            radarTargets[j].heading =
              heading;

            radarTargets[j].distance =
              dist;

            found = true;

            break;
          }
        }


        if (
          !found &&
          radarCount < 10
        )
        {
          radarTargets[radarCount++] =
          {
            id,
            heading,
            dist
          };
        }
      }
    }
  }


/* =====================================================
     RADAR UPDATE
     
     Only update the radar UI when actually on the
     radar page.
     ===================================================== */

  static uint32_t radarTimer = 0;


  if (
    millis() - radarTimer > 100
  )
  {
    radarTimer =
      millis();


    if (
      currentPage == PAGE_RADAR
    )
    {
      Radar_Update(
        radarTargets,
        radarCount,
        Sensors_GetYaw()
      );
    }
  }

/* =====================================================
     SEND WATCH DATA
     ===================================================== */

  static uint32_t txTimer = 0;


  if (
    millis() - txTimer > 1000
  )
  {
    txTimer =
      millis();


    float gx;
    float gy;
    float gz;


    Sensors_GetGyro(
      gx,
      gy,
      gz
    );


    Serial.print("PKT,");


    Serial.print(1);
    Serial.print(",");


    Serial.print(
      Sensors_GetYaw(),
      1
    );

    Serial.print(",");

    Serial.print(
      gx,
      3
    );

    Serial.print(",");


    Serial.print(
      gy,
      3
    );

    Serial.print(",");


    Serial.print(
      gz,
      3
    );

    Serial.print(",");


    Serial.print(
      Sensors_GetHeartRate(),
      1
    );

    Serial.print(",");


    Serial.print(
      Sensors_GetHealthState()
    );

    Serial.print(",");


    Serial.print(
      GPS_Fix()
    );

    Serial.print(",");


    Serial.print(
      GPS_Sats()
    );

    Serial.print(",");


    Serial.print(
      GPS_HDOP(),
      2
    );

    Serial.print(",");


    Serial.print(
      GPS_Lat(),
      6
    );

    Serial.print(",");


    Serial.print(
      GPS_Lon(),
      6
    );


    Serial.println(
      ",END"
    );
  }
}
