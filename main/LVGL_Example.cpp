#include "LVGL_Example.h"
#include <math.h>
#include "sensors.h"
#include "voice.h"
#include <string.h>
#include "MIC_MSM.h"
#include "Video.h"

/* =========================================================
   CONFIG
   ========================================================= */

#define SCREEN_W 412
#define SCREEN_H 412

#define MAX_TARGETS 10

#define CENTER_X 206
#define CENTER_Y 206

#define RADAR_RADIUS 180
#define MAX_RANGE_M 30.0f
#define MAX_RECENT_MESSAGES 4



/* =========================================================
   PAGE SYSTEM
   ========================================================= */

static lv_obj_t *pages[4];

static bool pages_ready = false;
static bool page_animating = false;

PageType currentPage = PAGE_RADAR;


/* =========================================================
   RADAR OBJECTS
   ========================================================= */

static lv_obj_t *radar;
static lv_obj_t *center_dot;

static lv_obj_t *targets[MAX_TARGETS];
static lv_obj_t *pulse[MAX_TARGETS];

static lv_obj_t *rings[3];

static lv_obj_t *sweep_line;


static float sweep_angle = 0;

static float pulse_radius[MAX_TARGETS] = {0};

static String recent_messages[MAX_RECENT_MESSAGES];
static int recent_message_count = 0;


/* =========================================================
   OTHER PAGE OBJECTS
   ========================================================= */

static lv_obj_t *msg_label;
static lv_obj_t *status_container;
static lv_obj_t *videoPage = NULL;
static bool videoPageVisible = false;
static lv_obj_t *video_toggle;
static lv_obj_t *video_status_label;

/* =========================================================
   MIC PAGE OBJECTS
   ========================================================= */

static lv_obj_t *mic_button;
static lv_obj_t *mic_status_label;




#define PAGE_ANIMATION_TIME 180


/* Forward declarations for page builders */
static void build_radar_page(lv_obj_t *parent);
static void build_messages_page(lv_obj_t *parent);
static void build_status_page(lv_obj_t *parent);
static void build_mic_page(lv_obj_t *parent);
static void page_manager_init();
static void video_toggle_event(lv_event_t *e);



/* =========================================================
   PAGE POSITIONING
   ========================================================= */

static void place_all_pages_normal()
{
    if(!pages_ready)
        return;

    for(int i = 0; i < 4; i++)
    {
        if(i == currentPage)
        {
            lv_obj_set_x(pages[i], 0);
        }
        else if(i < currentPage)
        {
            lv_obj_set_x(pages[i], -SCREEN_W);
        }
        else
        {
            lv_obj_set_x(pages[i], SCREEN_W);
        }

        lv_obj_set_y(
            pages[i],
            0
        );
    }

    // ============================================================
// VIDEO TRANSITION PAGE
// ============================================================

videoPage = lv_obj_create(lv_scr_act());

lv_obj_set_size(
    videoPage,
    SCREEN_W,
    SCREEN_H
);

lv_obj_set_pos(
    videoPage,
    0,
    -SCREEN_H
);

lv_obj_set_style_bg_color(
    videoPage,
    lv_color_hex(0x000000),
    0
);

lv_obj_set_style_bg_opa(
    videoPage,
    LV_OPA_COVER,
    0
);

lv_obj_set_style_border_width(
    videoPage,
    0,
    0
);

lv_obj_set_style_radius(
    videoPage,
    0,
    0
);

lv_obj_add_flag(
    videoPage,
    LV_OBJ_FLAG_HIDDEN
);

videoPageVisible = false;

// ============================================================
// VIDEO MODE TITLE
// ============================================================

lv_obj_t *video_title = lv_label_create(videoPage);

lv_label_set_text(
    video_title,
    "VIDEO MODE"
);

lv_obj_set_style_text_color(
    video_title,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_set_style_text_font(
    video_title,
    &lv_font_montserrat_16,
    0
);

lv_obj_align(
    video_title,
    LV_ALIGN_TOP_MID,
    0,
    55
);


// ============================================================
// VIDEO TOGGLE
// ============================================================

video_toggle = lv_btn_create(videoPage);

lv_obj_set_size(
    video_toggle,
    170,
    80
);

lv_obj_align(
    video_toggle,
    LV_ALIGN_CENTER,
    0,
    -10
);

lv_obj_set_style_radius(
    video_toggle,
    18,
    0
);

lv_obj_set_style_bg_color(
    video_toggle,
    lv_color_hex(0x07130D),
    0
);

lv_obj_set_style_bg_opa(
    video_toggle,
    LV_OPA_COVER,
    0
);

lv_obj_set_style_border_width(
    video_toggle,
    2,
    0
);

lv_obj_set_style_border_color(
    video_toggle,
    lv_color_hex(0x00FF66),
    0
);


// ============================================================
// TOGGLE LABEL
// ============================================================

lv_obj_t *video_toggle_label =
    lv_label_create(video_toggle);

lv_label_set_text(
    video_toggle_label,
    "VIDEO OFF"
);

lv_obj_set_style_text_color(
    video_toggle_label,
    lv_color_hex(0x777777),
    0
);

lv_obj_set_style_text_font(
    video_toggle_label,
    &lv_font_montserrat_16,
    0
);

lv_obj_center(video_toggle_label);


// ============================================================
// VIDEO STATUS
// ============================================================

video_status_label =
    lv_label_create(videoPage);

lv_label_set_text(
    video_status_label,
    "VIDEO STANDBY"
);

lv_obj_set_style_text_color(
    video_status_label,
    lv_color_hex(0x777777),
    0
);

lv_obj_set_style_text_font(
    video_status_label,
    &lv_font_montserrat_14,
    0
);

lv_obj_align(
    video_status_label,
    LV_ALIGN_CENTER,
    0,
    85
);

lv_obj_add_event_cb(
    video_toggle,
    video_toggle_event,
    LV_EVENT_CLICKED,
    NULL
);

}




/* =========================================================
   STOP PAGE ANIMATIONS
   ========================================================= */

static void stop_page_animations()
{
    if(!pages_ready)
        return;

    for(int i = 0; i < 4; i++)
    {
        lv_anim_del(pages[i], NULL);
    }

    page_animating = false;
}


/* =========================================================
   PAGE ANIMATION
   ========================================================= */

static void page_anim_exec(
    void *obj,
    int32_t value)
{
    lv_obj_set_x(
        (lv_obj_t *)obj,
        value
    );
}


static void page_animation_finished(
    lv_anim_t *anim)
{
    (void)anim;

    page_animating = false;

    place_all_pages_normal();
}


static void animate_page(
    lv_obj_t *page,
    int32_t start_x,
    int32_t target_x,
    bool ready_callback)
{
    if(start_x == target_x)
        return;

    lv_anim_t anim;

    lv_anim_init(&anim);

    lv_anim_set_var(
        &anim,
        page
    );

    lv_anim_set_values(
        &anim,
        start_x,
        target_x
    );

    lv_anim_set_time(
        &anim,
        PAGE_ANIMATION_TIME
    );

    lv_anim_set_path_cb(
        &anim,
        lv_anim_path_ease_out
    );

    lv_anim_set_exec_cb(
        &anim,
        page_anim_exec
    );

    if(ready_callback)
    {
        lv_anim_set_ready_cb(
            &anim,
            page_animation_finished
        );
    }

    lv_anim_start(&anim);
}

/* =========================================================
   MIC VERTICAL PAGE ANIMATION
   ========================================================= */

static void mic_page_anim_exec(
    void *obj,
    int32_t value)
{
    lv_obj_set_y(
        (lv_obj_t *)obj,
        value
    );
}


static void mic_page_animation_finished(
    lv_anim_t *anim)
{
    (void)anim;

    page_animating = false;

    place_all_pages_normal();
}


static void animate_mic_page(
    lv_obj_t *page,
    int32_t start_y,
    int32_t target_y,
    bool ready_callback)
{
    lv_anim_t anim;

    lv_anim_init(&anim);

    lv_anim_set_var(
        &anim,
        page
    );

    lv_anim_set_values(
        &anim,
        start_y,
        target_y
    );

    lv_anim_set_time(
        &anim,
        PAGE_ANIMATION_TIME
    );

    lv_anim_set_path_cb(
        &anim,
        lv_anim_path_ease_out
    );

    lv_anim_set_exec_cb(
        &anim,
        mic_page_anim_exec
    );

    if(ready_callback)
    {
        lv_anim_set_ready_cb(
            &anim,
            mic_page_animation_finished
        );
    }

    lv_anim_start(&anim);
}

/* =========================================================
   VIDEO PAGE VERTICAL ANIMATION
   ========================================================= */

static void video_page_anim_exec(
    void *obj,
    int32_t value)
{
    lv_obj_set_y(
        (lv_obj_t *)obj,
        value
    );
}


static void video_page_animation_finished(
    lv_anim_t *anim)
{
    (void)anim;

    videoPageVisible = true;
    page_animating = false;
}


/* ---------------------------------------------------------
   VIDEO PAGE: TOP -> CENTER
   --------------------------------------------------------- */

static void animate_video_page_in()
{
    if(videoPage == NULL)
        return;

    lv_obj_clear_flag(
        videoPage,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_obj_set_x(
        videoPage,
        0
    );

    lv_obj_set_y(
        videoPage,
        -SCREEN_H
    );

    lv_anim_t anim;

    lv_anim_init(&anim);

    lv_anim_set_var(
        &anim,
        videoPage
    );

    lv_anim_set_values(
        &anim,
        -SCREEN_H,
        0
    );

    lv_anim_set_time(
        &anim,
        PAGE_ANIMATION_TIME
    );

    lv_anim_set_path_cb(
        &anim,
        lv_anim_path_ease_out
    );

    lv_anim_set_exec_cb(
        &anim,
        video_page_anim_exec
    );

    lv_anim_set_ready_cb(
        &anim,
        video_page_animation_finished
    );

    page_animating = true;

    lv_anim_start(&anim);
}

/* ---------------------------------------------------------
   VIDEO PAGE: CENTER -> BOTTOM
   --------------------------------------------------------- */

static void animate_video_page_out()
{
    if(videoPage == NULL)
        return;

    lv_anim_t anim;

    lv_anim_init(&anim);

    lv_anim_set_var(
        &anim,
        videoPage
    );

    lv_anim_set_values(
        &anim,
        0,
        SCREEN_H
    );

    lv_anim_set_time(
        &anim,
        PAGE_ANIMATION_TIME
    );

    lv_anim_set_path_cb(
        &anim,
        lv_anim_path_ease_out
    );

    lv_anim_set_exec_cb(
        &anim,
        video_page_anim_exec
    );

    lv_anim_set_ready_cb(
        &anim,
        [](lv_anim_t *anim)
        {
            (void)anim;

            lv_obj_add_flag(
                videoPage,
                LV_OBJ_FLAG_HIDDEN
            );

            lv_obj_set_y(
                videoPage,
                -SCREEN_H
            );

            videoPageVisible = false;
            page_animating = false;
        }
    );

    page_animating = true;

    lv_anim_start(&anim);
}

/* =========================================================
   CHANGE PAGE
   ========================================================= */

static void change_page(
    PageType targetPage,
    int direction)
{
    if(!pages_ready)
        return;

    if(page_animating)
        return;

    if(targetPage == currentPage)
        return;

    PageType oldPage = currentPage;

    int target_start_x =
        direction > 0
            ? SCREEN_W
            : -SCREEN_W;

    int old_target_x =
        direction > 0
            ? -SCREEN_W
            : SCREEN_W;

    lv_obj_set_x(
        pages[targetPage],
        target_start_x
    );

    page_animating = true;

    animate_page(
        pages[oldPage],
        0,
        old_target_x,
        false
    );

    currentPage = targetPage;

    animate_page(
        pages[targetPage],
        target_start_x,
        0,
        true
    );
}


/* =========================================================
   TOP TOUCH
   ========================================================= */

void Page_Touch_Top()
{
    if(page_animating)
        return;

    // ============================================================
    // VIDEO -> RADAR
    // ============================================================

    if(videoPageVisible)
    {
        animate_video_page_out();
        return;
    }

    // ============================================================
    // RADAR -> VIDEO
    // ============================================================

    if(currentPage == PAGE_RADAR)
    {
        animate_video_page_in();
        return;
    }

    // ============================================================
    // EXISTING MIC -> RADAR
    // ============================================================

    if(currentPage != PAGE_MIC)
        return;

    /*
     * Leaving MIC page always turns the microphone OFF.
     */
    if(MIC_IsEnabled())
    {
        MIC_SetEnabled(false);

        if(mic_status_label != NULL)
        {
            lv_label_set_text(
                mic_status_label,
                "MIC OFF"
            );

            lv_obj_set_style_text_color(
                mic_status_label,
                lv_color_hex(0x777777),
                0
            );
        }
    }

    lv_obj_t *micPage = pages[PAGE_MIC];
    lv_obj_t *radarPage = pages[PAGE_RADAR];

    /*
     * Radar starts above the display.
     */
    lv_obj_set_y(
        radarPage,
        -SCREEN_H
    );

    /*
     * Keep X positions stable.
     */
    lv_obj_set_x(
        radarPage,
        0
    );

    page_animating = true;

    /*
     * MIC moves upward and leaves.
     */
    animate_mic_page(
        micPage,
        0,
        -SCREEN_H,
        false
    );

    /*
     * Radar enters from the top.
     */
    animate_mic_page(
        radarPage,
        -SCREEN_H,
        0,
        true
    );

    currentPage = PAGE_RADAR;
}


/* =========================================================
   BOTTOM TOUCH
   ========================================================= */

void Page_Touch_Bottom()
{

        // LIVE VIDEO -> VIDEO MODE
    if(Video_IsActive())
    {
        Video_RequestStop();
        return;
    }

        // VIDEO MODE -> RADAR
    if(videoPageVisible)
    {
        animate_video_page_out();
        return;
    }

    if(page_animating)
        return;

    if(currentPage != PAGE_RADAR)
        return;

    lv_obj_t *oldPage = pages[PAGE_RADAR];
    lv_obj_t *micPage = pages[PAGE_MIC];

    /*
     * MIC starts below the display.
     */
    lv_obj_set_y(
        micPage,
        SCREEN_H
    );

    /*
     * Keep X positions stable.
     */
    lv_obj_set_x(
        micPage,
        0
    );

    page_animating = true;

    /*
     * Radar moves upward and leaves screen.
     */
    animate_mic_page(
        oldPage,
        0,
        -SCREEN_H,
        false
    );

    /*
     * MIC comes from bottom to center.
     */
    animate_mic_page(
        micPage,
        SCREEN_H,
        0,
        true
    );

    currentPage = PAGE_MIC;
}

/* =========================================================
   LEFT TOUCH
   ========================================================= */

void Page_Touch_Left()
{
    if(page_animating)
        return;

    if(currentPage == PAGE_RADAR)
    {
        change_page(PAGE_MESSAGES, -1);
    }
    else if(currentPage == PAGE_STATUS)
    {
        change_page(PAGE_RADAR, -1);
    }
}


/* =========================================================
   RIGHT TOUCH
   ========================================================= */

void Page_Touch_Right()
{
    if(page_animating)
        return;

    if(currentPage == PAGE_RADAR)
    {
        change_page(PAGE_STATUS, 1);
    }
    else if(currentPage == PAGE_MESSAGES)
    {
        change_page(PAGE_RADAR, 1);
    }
}


/* =========================================================
   RADAR UI
   ========================================================= */

void Radar_UI()
{
    page_manager_init();

    stop_page_animations();

    currentPage =
        PAGE_RADAR;


    place_all_pages_normal();
}


/* =========================================================
   RADAR UPDATE
   ========================================================= */

void Radar_Update(
    RadarData *targets_data,
    int count,
    float myHeading)
{
    if(!pages_ready)
        return;

    if(currentPage != PAGE_RADAR)
        return;


    for(int i = 0; i < MAX_TARGETS; i++)
    {
        if(i >= count)
        {
            lv_obj_add_flag(
                targets[i],
                LV_OBJ_FLAG_HIDDEN
            );

            lv_obj_add_flag(
                pulse[i],
                LV_OBJ_FLAG_HIDDEN
            );

            continue;
        }


        float rel =
            targets_data[i].heading -
            myHeading;


        while(rel < -180)
            rel += 360;

        while(rel > 180)
            rel -= 360;


        float rad =
            rel *
            3.14159f /
            180.0f;


        float dist =
            targets_data[i].distance;


        float scale =
            dist /
            MAX_RANGE_M;


        if(scale > 1.0f)
            scale = 1.0f;


        int r =
            scale *
            RADAR_RADIUS;


        int x =
            r *
            sin(rad);

        int y =
            -r *
            cos(rad);


        lv_obj_clear_flag(
            targets[i],
            LV_OBJ_FLAG_HIDDEN
        );


        lv_obj_align(
            targets[i],
            LV_ALIGN_CENTER,
            x,
            y
        );


        /* -----------------------------------------
           PULSE
        ----------------------------------------- */

        pulse_radius[i] += 3;

        if(pulse_radius[i] > 30)
            pulse_radius[i] = 0;


        int size =
            10 +
            pulse_radius[i];


        lv_obj_clear_flag(
            pulse[i],
            LV_OBJ_FLAG_HIDDEN
        );


        lv_obj_set_size(
            pulse[i],
            size,
            size
        );


        lv_obj_align(
            pulse[i],
            LV_ALIGN_CENTER,
            x,
            y
        );


        int opa =
            120 -
            (pulse_radius[i] * 4);


        if(opa < 0)
            opa = 0;


        lv_obj_set_style_bg_opa(
            pulse[i],
            opa,
            0
        );
    }


    /* ---------------------------------------------
   RADAR SWEEP
--------------------------------------------- */

sweep_angle += 4;

if(sweep_angle >= 360)
    sweep_angle = 0;

float sweep_rad =
    sweep_angle * 3.14159f / 180.0f;

static lv_point_t sweep_points[2];

sweep_points[0].x = CENTER_X;
sweep_points[0].y = CENTER_Y;

sweep_points[1].x =
    CENTER_X + cos(sweep_rad) * RADAR_RADIUS;

sweep_points[1].y =
    CENTER_Y + sin(sweep_rad) * RADAR_RADIUS;

lv_line_set_points(
    sweep_line,
    sweep_points,
    2
);
}

/* =========================================================
   MESSAGE UI
   ========================================================= */

void Messages_UI()
{
    page_manager_init();

    stop_page_animations();

    currentPage =
        PAGE_MESSAGES;


    place_all_pages_normal();
}


/* =========================================================
   STATUS UI
   ========================================================= */

void Status_UI()
{
    page_manager_init();

    stop_page_animations();

    currentPage =
        PAGE_STATUS;


    place_all_pages_normal();
}


/* =========================================================
   LEGACY PAGE SWITCH
   ========================================================= */

void Switch_Page(int dir)
{
    if(dir > 0)
    {
        if(currentPage == PAGE_RADAR)
            change_page(PAGE_STATUS, 1);
        else if(currentPage == PAGE_MESSAGES)
            change_page(PAGE_RADAR, 1);
    }
    else if(dir < 0)
    {
        if(currentPage == PAGE_RADAR)
            change_page(PAGE_MESSAGES, -1);
        else if(currentPage == PAGE_STATUS)
            change_page(PAGE_RADAR, -1);
    }
}

/* =========================================================
   RADAR PAGE BUILDER
   ========================================================= */

static void build_radar_page(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(
        parent,
        lv_color_hex(0x000000),
        0
    );

    lv_obj_set_style_border_width(
        parent,
        0,
        0
    );

    lv_obj_clear_flag(
        parent,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /* =====================================================
       RADAR CONTAINER
       ===================================================== */

    radar = lv_obj_create(parent);

    lv_obj_set_size(
        radar,
        SCREEN_W,
        SCREEN_H
    );

    lv_obj_center(radar);

    lv_obj_set_style_bg_opa(
        radar,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        radar,
        0,
        0
    );

    lv_obj_set_style_pad_all(
        radar,
        0,
        0
    );

    lv_obj_clear_flag(
        radar,
        LV_OBJ_FLAG_SCROLLABLE
    );


   


    /* =====================================================
       RADAR RINGS
       ===================================================== */

    int ring_sizes[3] = {
        120,
        240,
        360
    };

    for(int i = 0; i < 3; i++)
    {
        rings[i] = lv_obj_create(radar);

        lv_obj_set_size(
            rings[i],
            ring_sizes[i],
            ring_sizes[i]
        );

        lv_obj_center(rings[i]);

        lv_obj_set_style_radius(
            rings[i],
            LV_RADIUS_CIRCLE,
            0
        );

        lv_obj_set_style_bg_opa(
            rings[i],
            LV_OPA_TRANSP,
            0
        );

        lv_obj_set_style_border_width(
            rings[i],
            1,
            0
        );

        lv_obj_set_style_border_color(
            rings[i],
            lv_color_hex(0x00FF00),
            0
        );

        lv_obj_set_style_border_opa(
            rings[i],
            LV_OPA_30,
            0
        );
    }


    /* =====================================================
       CENTER DOT
       ===================================================== */

    center_dot = lv_obj_create(radar);

    lv_obj_set_size(
        center_dot,
        12,
        12
    );

    lv_obj_center(center_dot);

    lv_obj_set_style_radius(
        center_dot,
        LV_RADIUS_CIRCLE,
        0
    );

    lv_obj_set_style_bg_color(
        center_dot,
        lv_color_hex(0x00FF00),
        0
    );

    lv_obj_set_style_bg_opa(
        center_dot,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        center_dot,
        0,
        0
    );


    /* =====================================================
       TARGET DOTS
       ===================================================== */

    for(int i = 0; i < MAX_TARGETS; i++)
    {
        targets[i] = lv_obj_create(radar);

        lv_obj_set_size(
            targets[i],
            10,
            10
        );

        lv_obj_set_style_radius(
            targets[i],
            LV_RADIUS_CIRCLE,
            0
        );

        lv_obj_set_style_bg_color(
            targets[i],
            lv_color_hex(0xFF0000),
            0
        );

        lv_obj_set_style_bg_opa(
            targets[i],
            LV_OPA_COVER,
            0
        );

        lv_obj_set_style_border_width(
            targets[i],
            0,
            0
        );

        lv_obj_add_flag(
            targets[i],
            LV_OBJ_FLAG_HIDDEN
        );


        /* ---------------------------------------------
           TARGET PULSE
           --------------------------------------------- */

        pulse[i] = lv_obj_create(radar);

        lv_obj_set_size(
            pulse[i],
            10,
            10
        );

        lv_obj_set_style_radius(
            pulse[i],
            LV_RADIUS_CIRCLE,
            0
        );

        lv_obj_set_style_bg_color(
            pulse[i],
            lv_color_hex(0xFF0000),
            0
        );

        lv_obj_set_style_bg_opa(
            pulse[i],
            LV_OPA_30,
            0
        );

        lv_obj_set_style_border_width(
            pulse[i],
            1,
            0
        );

        lv_obj_set_style_border_color(
            pulse[i],
            lv_color_hex(0xFF0000),
            0
        );

        lv_obj_add_flag(
            pulse[i],
            LV_OBJ_FLAG_HIDDEN
        );

        pulse_radius[i] = 0;
    }


    /* =====================================================
       SWEEP OBJECT
       ===================================================== */

    /* =====================================================
   RADAR SWEEP
   ===================================================== */

sweep_line = lv_line_create(radar);

static lv_point_t sweep_points[2];

sweep_points[0].x = CENTER_X;
sweep_points[0].y = CENTER_Y;

sweep_points[1].x = CENTER_X;
sweep_points[1].y = CENTER_Y - RADAR_RADIUS;

lv_line_set_points(
    sweep_line,
    sweep_points,
    2
);

lv_obj_set_style_line_color(
    sweep_line,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_set_style_line_width(
    sweep_line,
    3,
    0
);

lv_obj_set_style_line_opa(
    sweep_line,
    LV_OPA_70,
    0
);

sweep_angle = 0;
}


// ============================================
// MESSAGE HISTORY
// ============================================

static void Messages_AddRecent(const char *message)
{
    if(message == NULL)
        return;


    /*
     * Shift older messages down.
     */
    for(int i = MAX_RECENT_MESSAGES - 1; i > 0; i--)
    {
        recent_messages[i] =
            recent_messages[i - 1];
    }


    /*
     * Add newest message at the top.
     */
    recent_messages[0] =
        String("> ") + message;


    if(recent_message_count < MAX_RECENT_MESSAGES)
    {
        recent_message_count++;
    }


    /*
     * Update the display.
     */
    if(msg_label == NULL)
        return;


    String text = "";


    for(int i = 0; i < recent_message_count; i++)
    {
        text += recent_messages[i];


        if(i < recent_message_count - 1)
        {
            text += "\n";
        }
    }


    lv_label_set_text(
        msg_label,
        text.c_str()
    );


    lv_obj_set_style_text_color(
        msg_label,
        lv_color_hex(0xFFFFFF),
        0
    );
}

/* =========================================================
   MESSAGES PAGE
   ========================================================= */

static void message_button_event(lv_event_t *e)
{
    const char *command =
        (const char *)lv_event_get_user_data(e);


    if(command == NULL)
        return;


    Serial.println();
    Serial.println("==============================");
    Serial.println("MESSAGE BUTTON CLICKED");
    Serial.print("COMMAND = ");
    Serial.println(command);


    /*
     * Add to recent sent history.
     */

    if(strcmp(command, "ZORO SEND HELP") == 0)
    {
        Messages_AddRecent("HELP");
    }
    else if(strcmp(command, "ZORO SEND ENEMY") == 0)
    {
        Messages_AddRecent("ENEMY");
    }
    else if(strcmp(command, "ZORO SEND FALLBACK") == 0)
    {
        Messages_AddRecent("FALLBACK");
    }
    else if(strcmp(command, "ZORO SEND AMBUSH") == 0)
    {
        Messages_AddRecent("AMBUSH");
    }


    Serial.println("==============================");


    /*
     * Existing Rakshak voice/message system.
     */

    Voice_HandleCommand(command);
}


static void build_messages_page(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(
        parent,
        lv_color_hex(0x000000),
        0
    );

    lv_obj_set_style_border_width(
        parent,
        0,
        0
    );

    lv_obj_clear_flag(
        parent,
        LV_OBJ_FLAG_SCROLLABLE
    );


    // ==============================
    // TITLE
    // ==============================

    lv_obj_t *title = lv_label_create(parent);

    lv_label_set_text(
        title,
        "MESSAGES"
    );

    lv_obj_set_style_text_color(
        title,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        28
    );


    // ==============================
    // MESSAGE BUTTONS
    // ==============================

    lv_obj_t *help_btn = lv_btn_create(parent);

    lv_obj_set_size(
        help_btn,
        110,
        55
    );

    lv_obj_align(
        help_btn,
        LV_ALIGN_TOP_LEFT,
        55,
        75
    );

    lv_obj_add_event_cb(
        help_btn,
        message_button_event,
        LV_EVENT_CLICKED,
        (void *)"ZORO SEND HELP"
    );

    lv_obj_t *help_label = lv_label_create(help_btn);

    lv_label_set_text(
        help_label,
        "HELP"
    );

    lv_obj_center(help_label);


    lv_obj_t *enemy_btn = lv_btn_create(parent);

    lv_obj_set_size(
        enemy_btn,
        110,
        55
    );

    lv_obj_align(
        enemy_btn,
        LV_ALIGN_TOP_RIGHT,
        -55,
        75
    );

    lv_obj_add_event_cb(
        enemy_btn,
        message_button_event,
        LV_EVENT_CLICKED,
        (void *)"ZORO SEND ENEMY"
    );

    lv_obj_t *enemy_label = lv_label_create(enemy_btn);

    lv_label_set_text(
        enemy_label,
        "ENEMY"
    );

    lv_obj_center(enemy_label);


    lv_obj_t *fallback_btn = lv_btn_create(parent);

    lv_obj_set_size(
        fallback_btn,
        110,
        55
    );

    lv_obj_align(
        fallback_btn,
        LV_ALIGN_TOP_LEFT,
        55,
        145
    );

    lv_obj_add_event_cb(
        fallback_btn,
        message_button_event,
        LV_EVENT_CLICKED,
        (void *)"ZORO SEND FALLBACK"
    );

    lv_obj_t *fallback_label = lv_label_create(fallback_btn);

    lv_label_set_text(
        fallback_label,
        "FALLBACK"
    );

    lv_obj_center(fallback_label);


    lv_obj_t *ambush_btn = lv_btn_create(parent);

    lv_obj_set_size(
        ambush_btn,
        110,
        55
    );

    lv_obj_align(
        ambush_btn,
        LV_ALIGN_TOP_RIGHT,
        -55,
        145
    );

    lv_obj_add_event_cb(
        ambush_btn,
        message_button_event,
        LV_EVENT_CLICKED,
        (void *)"ZORO SEND AMBUSH"
    );

    lv_obj_t *ambush_label = lv_label_create(ambush_btn);

    lv_label_set_text(
        ambush_label,
        "AMBUSH"
    );

    lv_obj_center(ambush_label);


    // ==============================
    // SECTION DIVIDER
    // ==============================

    lv_obj_t *divider = lv_obj_create(parent);

    lv_obj_set_size(
        divider,
        300,
        1
    );

    lv_obj_align(
        divider,
        LV_ALIGN_TOP_MID,
        0,
        220
    );

    lv_obj_set_style_bg_color(
        divider,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_set_style_border_width(
        divider,
        0,
        0
    );


    // ==============================
    // RECENT SENT TITLE
    // ==============================

    lv_obj_t *recent_title = lv_label_create(parent);

    lv_label_set_text(
        recent_title,
        "RECENT SENT"
    );

    lv_obj_set_style_text_color(
        recent_title,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_align(
        recent_title,
        LV_ALIGN_TOP_MID,
        0,
        235
    );


    // ==============================
    // RECENT MESSAGE AREA
    // ==============================

    msg_label = lv_label_create(parent);

    lv_label_set_text(
        msg_label,
        "NO MESSAGES SENT"
    );

    lv_obj_set_style_text_color(
        msg_label,
        lv_color_hex(0x777777),
        0
    );

    lv_obj_align(
        msg_label,
        LV_ALIGN_TOP_MID,
        0,
        275
    );
}


/* =========================================================
   STATUS PAGE
   ========================================================= */

static void build_status_page(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(
        parent,
        lv_color_hex(0x000000),
        0
    );

    lv_obj_set_style_bg_opa(
        parent,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        parent,
        0,
        0
    );

    lv_obj_clear_flag(
        parent,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /* =====================================================
       TITLE
       ===================================================== */

    lv_obj_t *title =
        lv_label_create(parent);

    lv_label_set_text(
        title,
        "SELF STATUS"
    );

    lv_obj_set_style_text_color(
        title,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_16,
        0
    );

    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        45
    );


    /* =====================================================
       STATUS PANEL
       ===================================================== */

    status_container =
        lv_obj_create(parent);

    lv_obj_set_size(
        status_container,
        330,
        210
    );

    lv_obj_align(
        status_container,
        LV_ALIGN_CENTER,
        0,
        20
    );

    lv_obj_set_style_bg_color(
        status_container,
        lv_color_hex(0x07130D),
        0
    );

    lv_obj_set_style_bg_opa(
        status_container,
        LV_OPA_80,
        0
    );

    lv_obj_set_style_border_width(
        status_container,
        2,
        0
    );

    lv_obj_set_style_border_color(
        status_container,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_set_style_radius(
        status_container,
        18,
        0
    );

    lv_obj_set_style_pad_all(
        status_container,
        18,
        0
    );

    lv_obj_clear_flag(
        status_container,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /* =====================================================
       SELF INFORMATION
       ===================================================== */

    lv_obj_t *self =
        lv_label_create(status_container);

    lv_label_set_text(
    self,
    "HEART RATE     -- BPM\n"
    "HEALTH         --\n"
    "LATITUDE       22.82\n"
    "LONGITUDE      75.94"
);

    lv_obj_set_style_text_color(
        self,
        lv_color_hex(0xFFFFFF),
        0
    );

    lv_obj_set_style_text_font(
        self,
        &lv_font_montserrat_16,
        0
    );

    lv_obj_set_style_text_line_space(
        self,
        10,
        0
    );

    lv_obj_align(
        self,
        LV_ALIGN_CENTER,
        0,
        0
    );
}

void update_status_data()
{
    if(!pages_ready)
        return;

    if(currentPage != PAGE_STATUS)
        return;

    if(status_container == NULL)
        return;

    float heartRate = Sensors_GetHeartRate();
    String health = Sensors_GetHealthState();

    char status_text[160];

    snprintf(
        status_text,
        sizeof(status_text),
        "HEART RATE     %.0f BPM\n"
        "HEALTH         %s\n"
        "LATITUDE       22.82\n"
        "LONGITUDE      75.94",
        heartRate,
        health.c_str()
    );

    // Find the label inside the status container
    lv_obj_t *self = lv_obj_get_child(status_container, 0);

    if(self != NULL)
    {
        lv_label_set_text(self, status_text);
    }
}


/* =========================================================
   PAGE MANAGER INITIALIZATION
   ========================================================= */

/* =========================================================
   MIC PAGE
   ========================================================= */

static void mic_button_event(lv_event_t *e)
{
    (void)e;

    /*
     * Ignore any delayed/overlapping click event
     * if we are no longer on the MIC page.
     */
    if(currentPage != PAGE_MIC)
        return;

    /*
     * Ignore clicks while a page transition is running.
     */
    if(page_animating)
        return;


    bool newState =
        !MIC_IsEnabled();

    MIC_SetEnabled(newState);


    if(MIC_IsEnabled())
    {
        lv_label_set_text(
            mic_status_label,
            "MIC LISTENING"
        );

        lv_obj_set_style_text_color(
            mic_status_label,
            lv_color_hex(0x00FF66),
            0
        );
    }
    else
    {
        lv_label_set_text(
            mic_status_label,
            "MIC OFF"
        );

        lv_obj_set_style_text_color(
            mic_status_label,
            lv_color_hex(0x777777),
            0
        );
    }
}

static void video_toggle_event(lv_event_t *e)
{
    (void)e;

    if(page_animating)
        return;

    if(!videoPageVisible)
        return;

    Serial.println("[VIDEO] Toggle pressed");

    /*
     * DO NOT hide videoPage here.
     *
     * It must remain visible until Video.cpp
     * takes over the physical LCD.
     */
    Video_RequestStart();
}   


static void build_mic_page(lv_obj_t *parent)
{
    /* =====================================================
       PAGE BACKGROUND
       ===================================================== */

    lv_obj_set_style_bg_color(
        parent,
        lv_color_hex(0x000000),
        0
    );

    lv_obj_set_style_bg_opa(
        parent,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        parent,
        0,
        0
    );

    lv_obj_clear_flag(
        parent,
        LV_OBJ_FLAG_SCROLLABLE
    );


    /* =====================================================
       TITLE
       ===================================================== */

    lv_obj_t *title =
        lv_label_create(parent);

    lv_label_set_text(
        title,
        "MIC"
    );

    lv_obj_set_style_text_color(
        title,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_set_style_text_font(
        title,
        &lv_font_montserrat_16,
        0
    );

    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        42
    );


    /* =====================================================
       MIC BUTTON
       ===================================================== */

    mic_button =
        lv_btn_create(parent);

    lv_obj_set_size(
        mic_button,
        150,
        150
    );

    lv_obj_align(
        mic_button,
        LV_ALIGN_CENTER,
        0,
        -5
    );

    lv_obj_set_style_radius(
        mic_button,
        LV_RADIUS_CIRCLE,
        0
    );

    lv_obj_set_style_bg_color(
        mic_button,
        lv_color_hex(0x07130D),
        0
    );

    lv_obj_set_style_bg_opa(
        mic_button,
        LV_OPA_COVER,
        0
    );

    lv_obj_set_style_border_width(
        mic_button,
        3,
        0
    );

    lv_obj_set_style_border_color(
        mic_button,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_set_style_shadow_width(
        mic_button,
        12,
        0
    );

    lv_obj_set_style_shadow_color(
        mic_button,
        lv_color_hex(0x00FF66),
        0
    );

    lv_obj_add_event_cb(
        mic_button,
        mic_button_event,
        LV_EVENT_CLICKED,
        NULL
    );


    /* =====================================================
   MIC SYMBOL
   ===================================================== */

/* -----------------------------------------------------
   MIC BODY
   ----------------------------------------------------- */

lv_obj_t *mic_body =
    lv_obj_create(mic_button);

lv_obj_set_size(
    mic_body,
    38,
    62
);

lv_obj_align(
    mic_body,
    LV_ALIGN_CENTER,
    0,
    -12
);

lv_obj_set_style_radius(
    mic_body,
    19,
    0
);

lv_obj_set_style_bg_color(
    mic_body,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_set_style_bg_opa(
    mic_body,
    LV_OPA_COVER,
    0
);

lv_obj_set_style_border_width(
    mic_body,
    0,
    0
);

lv_obj_clear_flag(
    mic_body,
    LV_OBJ_FLAG_CLICKABLE
);


/* -----------------------------------------------------
   MIC U-SHAPE
   ----------------------------------------------------- */

lv_obj_t *mic_arc =
    lv_obj_create(mic_button);

lv_obj_set_size(
    mic_arc,
    76,
    72
);

lv_obj_align(
    mic_arc,
    LV_ALIGN_CENTER,
    0,
    -5
);

lv_obj_set_style_radius(
    mic_arc,
    LV_RADIUS_CIRCLE,
    0
);

lv_obj_set_style_bg_opa(
    mic_arc,
    LV_OPA_TRANSP,
    0
);

lv_obj_set_style_border_width(
    mic_arc,
    4,
    0
);

lv_obj_set_style_border_color(
    mic_arc,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_clear_flag(
    mic_arc,
    LV_OBJ_FLAG_CLICKABLE
);


/* -----------------------------------------------------
   MIC STEM
   ----------------------------------------------------- */

lv_obj_t *mic_stand =
    lv_obj_create(mic_button);

lv_obj_set_size(
    mic_stand,
    5,
    24
);

lv_obj_align(
    mic_stand,
    LV_ALIGN_CENTER,
    0,
    44
);

lv_obj_set_style_radius(
    mic_stand,
    2,
    0
);

lv_obj_set_style_bg_color(
    mic_stand,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_set_style_bg_opa(
    mic_stand,
    LV_OPA_COVER,
    0
);

lv_obj_set_style_border_width(
    mic_stand,
    0,
    0
);

lv_obj_clear_flag(
    mic_stand,
    LV_OBJ_FLAG_CLICKABLE
);


/* -----------------------------------------------------
   MIC BASE
   ----------------------------------------------------- */

lv_obj_t *mic_base =
    lv_obj_create(mic_button);

lv_obj_set_size(
    mic_base,
    42,
    5
);

lv_obj_align(
    mic_base,
    LV_ALIGN_CENTER,
    0,
    58
);

lv_obj_set_style_radius(
    mic_base,
    3,
    0
);

lv_obj_set_style_bg_color(
    mic_base,
    lv_color_hex(0x00FF66),
    0
);

lv_obj_set_style_bg_opa(
    mic_base,
    LV_OPA_COVER,
    0
);

lv_obj_set_style_border_width(
    mic_base,
    0,
    0
);

lv_obj_clear_flag(
    mic_base,
    LV_OBJ_FLAG_CLICKABLE
);


    /* =====================================================
       STATUS
       ===================================================== */

    mic_status_label =
        lv_label_create(parent);

    lv_label_set_text(
        mic_status_label,
        "MIC OFF"
    );

    lv_obj_set_style_text_color(
        mic_status_label,
        lv_color_hex(0x777777),
        0
    );

    lv_obj_set_style_text_font(
        mic_status_label,
        &lv_font_montserrat_14,
        0
    );

    lv_obj_align(
        mic_status_label,
        LV_ALIGN_CENTER,
        0,
        105
    );
}

static void page_manager_init()
{
    if(pages_ready)
        return;


    lv_obj_t *root =
        lv_scr_act();


    lv_obj_clean(root);


    lv_obj_set_style_bg_color(
        root,
        lv_color_hex(0x000000),
        0
    );

    lv_obj_set_style_pad_all(
        root,
        0,
        0
    );


    /* =====================================================
       CREATE ALL PAGES
       ===================================================== */

    for(int i = 0; i < 4; i++)
    {
        pages[i] =
            lv_obj_create(root);

        lv_obj_set_size(
            pages[i],
            SCREEN_W,
            SCREEN_H
        );

        lv_obj_set_style_radius(
            pages[i],
            0,
            0
        );

        lv_obj_set_style_border_width(
            pages[i],
            0,
            0
        );

        lv_obj_set_style_pad_all(
            pages[i],
            0,
            0
        );

        lv_obj_clear_flag(
            pages[i],
            LV_OBJ_FLAG_SCROLLABLE
        );

        
    }


    /* =====================================================
       BUILD EACH PAGE
       ===================================================== */

    build_radar_page(
        pages[PAGE_RADAR]
    );

    build_messages_page(
        pages[PAGE_MESSAGES]
    );

    build_status_page(
        pages[PAGE_STATUS]
    );

    build_mic_page(
        pages[PAGE_MIC]
    );


    /* =====================================================
       INITIAL STATE
       ===================================================== */

    currentPage =
        PAGE_RADAR;

    pages_ready =
        true;

    place_all_pages_normal();
}