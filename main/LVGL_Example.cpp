#include "LVGL_Example.h"
#include <math.h>
#include "sensors.h"

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



/* =========================================================
   PAGE SYSTEM
   ========================================================= */

static lv_obj_t *pages[3];

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


/* =========================================================
   OTHER PAGE OBJECTS
   ========================================================= */

static lv_obj_t *msg_label;
static lv_obj_t *status_container;


#define PAGE_ANIMATION_TIME 180


/* Forward declarations for page builders */
static void build_radar_page(lv_obj_t *parent);
static void build_messages_page(lv_obj_t *parent);
static void build_status_page(lv_obj_t *parent);
static void page_manager_init();



/* =========================================================
   PAGE POSITIONING
   ========================================================= */

static void place_all_pages_normal()
{
    if(!pages_ready)
        return;

    for(int i = 0; i < 3; i++)
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
    }
}


/* =========================================================
   STOP PAGE ANIMATIONS
   ========================================================= */

static void stop_page_animations()
{
    if(!pages_ready)
        return;

    for(int i = 0; i < 3; i++)
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

/* =========================================================
   MESSAGES PAGE
   ========================================================= */

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


    msg_label = lv_label_create(parent);

    lv_label_set_text(
        msg_label,
        "NO MESSAGES"
    );

    lv_obj_set_style_text_color(
        msg_label,
        lv_color_hex(0x00FF00),
        0
    );

    lv_obj_center(msg_label);
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

    for(int i = 0; i < 3; i++)
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


    /* =====================================================
       INITIAL STATE
       ===================================================== */

    currentPage =
        PAGE_RADAR;

    pages_ready =
        true;

    place_all_pages_normal();
}
