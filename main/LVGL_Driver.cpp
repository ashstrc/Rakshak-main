/*****************************************************************************
  | File        :   LVGL_Driver.cpp
  | Description :   LVGL display + full-screen phone-style swipe driver
*****************************************************************************/

#include "LVGL_Driver.h"

/* --------------------------------------------------
   PAGE SWIPE API
   -------------------------------------------------- */

extern bool Page_Swipe_Begin(
    int16_t start_x,
    int16_t start_y
);

extern void Page_Swipe_Update(
    int16_t dx
);

extern void Page_Swipe_End(
    int16_t dx
);


/* --------------------------------------------------
   DISPLAY BUFFER
   -------------------------------------------------- */

static lv_disp_draw_buf_t draw_buf;

static lv_color_t buf1[LVGL_BUF_LEN];
static lv_color_t buf2[LVGL_BUF_LEN];


/* --------------------------------------------------
   TOUCH STATE
   -------------------------------------------------- */

static bool touching = false;

static bool gesture_locked = false;

static bool vertical_gesture = false;

static int16_t start_x = 0;

static int16_t start_y = 0;

static int16_t last_x = 0;


/*
 * Number of pixels the finger must move before
 * deciding whether the gesture is horizontal
 * or vertical.
 */
#define TOUCH_SLOP 8


/* --------------------------------------------------
   LVGL PRINT
   -------------------------------------------------- */

void Lvgl_print(const char *buf)
{
    // Serial.printf(buf);
}


/* --------------------------------------------------
   ROUND DISPLAY AREA ALIGNMENT
   -------------------------------------------------- */

void Lvgl_port_rounder_callback(
    struct _lv_disp_drv_t *disp_drv,
    lv_area_t *area)
{
    uint16_t x1 = area->x1;

    uint16_t x2 = area->x2;


    /*
     * SPD2010 LCD requires horizontal alignment
     * to 4-pixel boundaries.
     */

    area->x1 =
        (x1 >> 2) << 2;

    area->x2 =
        ((x2 >> 2) << 2) + 3;
}


/* --------------------------------------------------
   DISPLAY FLUSH
   -------------------------------------------------- */

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


    lv_disp_flush_ready(
        disp_drv
    );
}


/* --------------------------------------------------
   TOUCHPAD READ
   -------------------------------------------------- */

void Lvgl_Touchpad_Read(
    lv_indev_drv_t *indev_drv,
    lv_indev_data_t *data)
{
    uint16_t tp_x = 0;

    uint16_t tp_y = 0;

    uint8_t tp_cnt = 0;


    /* ------------------------------------------------
       READ SPD2010
       ------------------------------------------------ */

    bool pressed =
        Touch_Get_xy(
            &tp_x,
            &tp_y,
            NULL,
            &tp_cnt,
            CONFIG_ESP_LCD_TOUCH_MAX_POINTS
        );


    /* =================================================
       FINGER PRESSED / MOVING
       ================================================= */

    if(pressed && tp_cnt > 0)
    {
        /*
         * SPD2010 coordinates are reversed relative
         * to the LVGL display coordinates.
         */

        int16_t x =
            411 - tp_x;

        int16_t y =
            411 - tp_y;


        /*
         * Give LVGL the current touch position.
         */

        data->point.x =
            x;

        data->point.y =
            y;

        data->state =
            LV_INDEV_STATE_PR;


        /* ---------------------------------------------
           FIRST TOUCH FRAME
           --------------------------------------------- */

        if(!touching)
        {
            touching =
                true;

            gesture_locked =
                false;

            vertical_gesture =
                false;

            start_x =
                x;

            start_y =
                y;

            last_x =
                x;


            /*
             * Start a possible page gesture.
             *
             * The page manager does not change page
             * here. It only prepares for a gesture.
             */

            Page_Swipe_Begin(
                x,
                y
            );


            return;
        }


        /*
         * Always remember the latest X position.
         */

        last_x =
            x;


        /* ---------------------------------------------
           VERTICAL GESTURE ALREADY DETECTED
           --------------------------------------------- */

        if(vertical_gesture)
        {
            return;
        }


        /* ---------------------------------------------
           CALCULATE MOVEMENT
           --------------------------------------------- */

        int16_t dx =
            x - start_x;

        int16_t dy =
            y - start_y;


        int abs_dx =
            dx < 0 ? -dx : dx;

        int abs_dy =
            dy < 0 ? -dy : dy;


        /* ---------------------------------------------
           DETERMINE GESTURE DIRECTION
           --------------------------------------------- */

        if(!gesture_locked)
        {
            /*
             * Wait until the finger has moved enough
             * to classify the gesture.
             */

            if(abs_dx >= TOUCH_SLOP ||
               abs_dy >= TOUCH_SLOP)
            {
                gesture_locked =
                    true;


                /*
                 * More X movement = horizontal swipe.
                 */

                if(abs_dx > abs_dy)
                {
                    vertical_gesture =
                        false;
                }
                else
                {
                    /*
                     * More Y movement = vertical gesture.
                     *
                     * Vertical movement must never
                     * change pages.
                     */

                    vertical_gesture =
                        true;


                    /*
                     * Cancel any page movement.
                     */

                    Page_Swipe_End(
                        0
                    );


                    return;
                }
            }
        }


        /* ---------------------------------------------
           HORIZONTAL SWIPE
           --------------------------------------------- */

        if(gesture_locked &&
           !vertical_gesture)
        {
            /*
             * Move the page directly with the finger.
             */

            Page_Swipe_Update(
                dx
            );
        }
    }


    /* =================================================
       FINGER RELEASED
       ================================================= */

    else
    {
        data->state =
            LV_INDEV_STATE_REL;


        if(touching)
        {
            if(gesture_locked &&
               !vertical_gesture)
            {
                /*
                 * Use the last real touch position.
                 *
                 * Do not rely on the LVGL release
                 * frame for coordinates.
                 */

                int16_t final_dx =
                    last_x - start_x;


                Page_Swipe_End(
                    final_dx
                );
            }
            else
            {
                /*
                 * Tap or vertical gesture.
                 *
                 * It must not navigate.
                 */

                Page_Swipe_End(
                    0
                );
            }
        }


        /* ---------------------------------------------
           RESET TOUCH STATE
           --------------------------------------------- */

        touching =
            false;

        gesture_locked =
            false;

        vertical_gesture =
            false;

        start_x =
            0;

        start_y =
            0;

        last_x =
            0;
    }
}


/* --------------------------------------------------
   LVGL INITIALIZATION
   -------------------------------------------------- */

void Lvgl_Init(void)
{
    lv_init();


    /* =================================================
       DRAW BUFFER
       ================================================= */

    lv_disp_draw_buf_init(
        &draw_buf,
        buf1,
        buf2,
        LVGL_BUF_LEN
    );


    /* =================================================
       DISPLAY DRIVER
       ================================================= */

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


    /* =================================================
       TOUCH DRIVER
       ================================================= */

    static lv_indev_drv_t indev_drv;


    lv_indev_drv_init(
        &indev_drv
    );


    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = Lvgl_Touchpad_Read;

    lv_indev_drv_register(&indev_drv);
}


/* --------------------------------------------------
   LVGL LOOP
   -------------------------------------------------- */

void Lvgl_Loop(void)
{
    static uint32_t lastTick =
        0;


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


    /*
     * Run LVGL timers,
     * animations and rendering.
     */

    lv_timer_handler();


    /*
     * Small yield for ESP32.
     */

    delay(1);
}