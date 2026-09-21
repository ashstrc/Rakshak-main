/*****************************************************************************
  | File        :   LVGL_Driver.cpp
  | Description :   LVGL display + simple left/right touch navigation
*****************************************************************************/

#include "LVGL_Driver.h"

/*
 * Page navigation functions provided by LVGL_Example.cpp
 */
extern void Page_Touch_Left();
extern void Page_Touch_Right();


/* =========================================================
   LVGL DISPLAY BUFFER
   ========================================================= */

static lv_disp_draw_buf_t draw_buf;

static lv_color_t buf1[LVGL_BUF_LEN];
static lv_color_t buf2[LVGL_BUF_LEN];


/* =========================================================
   TOUCH STATE
   ========================================================= */

static bool touching = false;


/*
 * Touch zones.
 *
 * Screen width = 412 pixels.
 *
 * LEFT:
 *     x < 140
 *
 * CENTER:
 *     140 <= x <= 272
 *
 * RIGHT:
 *     x > 272
 */

#define LEFT_TOUCH_ZONE  140
#define RIGHT_TOUCH_ZONE 272


/* =========================================================
   LVGL PRINT
   ========================================================= */

void Lvgl_print(const char *buf)
{
    /*
     * Keep Serial quiet.
     *
     * Touch polling can happen very frequently,
     * so don't print here.
     */
}


/* =========================================================
   ROUND DISPLAY AREA
   ========================================================= */

void Lvgl_port_rounder_callback(
    struct _lv_disp_drv_t *disp_drv,
    lv_area_t *area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    area->x1 =
        (x1 >> 2) << 2;

    area->x2 =
        ((x2 >> 2) << 2) + 3;
}


/* =========================================================
   DISPLAY FLUSH
   ========================================================= */

void Lvgl_Display_LCD(
    lv_disp_drv_t *disp_drv,
    const lv_area_t *area,
    lv_color_t *color_p)
{
    LCD_addWindow(
        area->x1,
        area->y1,
        area->x2,
        area->y2,
        (uint16_t *)color_p
    );

    lv_disp_flush_ready(disp_drv);
}


/* =========================================================
   TOUCH READ
   ========================================================= */

void Lvgl_Touchpad_Read(
    lv_indev_drv_t *indev_drv,
    lv_indev_data_t *data)
{
    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;

    bool pressed =
        Touch_Get_xy(
            &tp_x,
            &tp_y,
            NULL,
            &tp_cnt,
            CONFIG_ESP_LCD_TOUCH_MAX_POINTS
        );


    /* =====================================================
       FINGER IS TOUCHING
       ===================================================== */

    if(pressed && tp_cnt > 0)
    {
        /*
         * Convert raw SPD2010 coordinates
         * to display coordinates.
         */
        int16_t x =
            411 - tp_x;

        int16_t y =
            411 - tp_y;


        data->point.x = x;
        data->point.y = y;

        data->state =
            LV_INDEV_STATE_PR;


        /*
         * Only react once when the finger
         * initially touches the screen.
         */
        if(!touching)
        {
            touching = true;


            /*
             * LEFT SIDE
             */
            if(x < LEFT_TOUCH_ZONE)
            {
                Page_Touch_Left();
            }


            /*
             * RIGHT SIDE
             */
            else if(x > RIGHT_TOUCH_ZONE)
            {
                Page_Touch_Right();
            }


            /*
             * CENTER
             *
             * Do nothing.
             */
        }

        return;
    }


    /* =====================================================
       FINGER RELEASED
       ===================================================== */

    data->state =
        LV_INDEV_STATE_REL;


    if(touching)
    {
        touching = false;
    }
}


/* =========================================================
   LVGL INITIALIZATION
   ========================================================= */

void Lvgl_Init(void)
{
    lv_init();


    /* -----------------------------------------------------
       DISPLAY BUFFER
       ----------------------------------------------------- */

    lv_disp_draw_buf_init(
        &draw_buf,
        buf1,
        buf2,
        LVGL_BUF_LEN
    );


    /* -----------------------------------------------------
       DISPLAY DRIVER
       ----------------------------------------------------- */

    static lv_disp_drv_t disp_drv;

    lv_disp_drv_init(
        &disp_drv
    );

    disp_drv.hor_res =
        412;

    disp_drv.ver_res =
        412;

    disp_drv.flush_cb =
        Lvgl_Display_LCD;

    disp_drv.rounder_cb =
        Lvgl_port_rounder_callback;

    disp_drv.draw_buf =
        &draw_buf;

    lv_disp_drv_register(
        &disp_drv
    );


    /* -----------------------------------------------------
       TOUCH DRIVER
       ----------------------------------------------------- */

    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(
        &indev_drv
    );

    indev_drv.type =
        LV_INDEV_TYPE_POINTER;

    indev_drv.read_cb =
        Lvgl_Touchpad_Read;

    lv_indev_drv_register(
        &indev_drv
    );
}


/* =========================================================
   LVGL LOOP
   ========================================================= */

void Lvgl_Loop(void)
{
    static uint32_t lastTick = 0;

    uint32_t now =
        millis();

    uint32_t diff =
        now - lastTick;


    if(diff > 0)
    {
        lv_tick_inc(
            diff
        );

        lastTick =
            now;
    }


    lv_timer_handler();

    delay(1);
}