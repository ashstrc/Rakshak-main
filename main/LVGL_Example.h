#pragma once

/* ---------- LVGL CORE ---------- */
#include "LVGL_Driver.h"

/* ---------- PAGE ENUM ---------- */
typedef enum
{
    PAGE_RADAR = 0,
    PAGE_MESSAGES,
    PAGE_STATUS

} PageType;

/* ---------- GLOBAL PAGE STATE ---------- */
extern PageType currentPage;

/* ---------- RADAR STRUCT ---------- */
typedef struct
{
    uint8_t id;
    float heading;
    float distance;

} RadarData;

/* ---------- UI FUNCTIONS ---------- */
void Backlight_adjustment_event_cb(lv_event_t * e);

void Lvgl_Example1(void);
void LVGL_Backlight_adjustment(uint8_t Backlight);

/* ---------- RADAR ---------- */
void Radar_UI();
void Radar_Update(RadarData *targets, int count, float myHeading);

/* ---------- PAGE CONTROL ---------- */
void Switch_Page(int dir);