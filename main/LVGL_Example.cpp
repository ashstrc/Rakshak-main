#include "LVGL_Example.h"
#include <math.h>

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

#define SWIPE_THRESHOLD 55
#define SWIPE_ANIMATION_TIME 280


/* =========================================================
   PAGE SYSTEM
   ========================================================= */

static lv_obj_t *pages[3];

static bool pages_ready = false;
static bool page_dragging = false;
static bool page_animating = false;

static PageType swipe_target_page;
static int swipe_direction = 0;

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

static lv_point_t sweep_points[2];

static float sweep_angle = 0;

static float pulse_radius[MAX_TARGETS] = {0};


/* =========================================================
   OTHER PAGE OBJECTS
   ========================================================= */

static lv_obj_t *msg_label;
static lv_obj_t *status_container;


/* =========================================================
   PAGE INDEX HELPERS
   ========================================================= */

static bool valid_page(int page)
{
    return page >= PAGE_RADAR && page <= PAGE_STATUS;
}


/* =========================================================
   PAGE POSITIONING
   ========================================================= */

/*
   Only the current page and the swipe target are involved
   during a gesture.

   direction:
      +1 = finger moving right
      -1 = finger moving left

   Example:

   RADAR -> STATUS

       RADAR | STATUS
          0      +412

   finger moves left:

       RADAR ------> STATUS
       dx < 0

   Example:

   RADAR -> MESSAGES

       MESSAGES | RADAR
          -412      0

   finger moves right:

       MESSAGES <------ RADAR
       dx > 0
*/

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
        else
        {
            /*
             * Keep non-current pages outside the visible area.
             */
            lv_obj_set_x(
                pages[i],
                (i < currentPage)
                    ? -SCREEN_W
                    : SCREEN_W
            );
        }
    }
}


/* =========================================================
   DETERMINE SWIPE TARGET
   ========================================================= */

static bool get_swipe_target(
    int16_t dx,
    PageType *target,
    int *direction)
{
    if(dx == 0)
        return false;

    /*
     * Finger moved RIGHT.
     */
    if(dx > 0)
    {
        if(currentPage == PAGE_RADAR)
        {
            *target = PAGE_MESSAGES;
            *direction = 1;
            return true;
        }

        if(currentPage == PAGE_MESSAGES)
        {
            *target = PAGE_RADAR;
            *direction = 1;
            return true;
        }

        /*
         * STATUS has no page to the right.
         */
        return false;
    }


    /*
     * Finger moved LEFT.
     */
    if(dx < 0)
    {
        if(currentPage == PAGE_RADAR)
        {
            *target = PAGE_STATUS;
            *direction = -1;
            return true;
        }

        if(currentPage == PAGE_STATUS)
        {
            *target = PAGE_RADAR;
            *direction = -1;
            return true;
        }

        /*
         * MESSAGES has no page to the left.
         */
        return false;
    }

    return false;
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
        lv_anim_del(
            pages[i],
            NULL
        );
    }

    page_animating = false;
}


/* =========================================================
   ANIMATION CALLBACK
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


/* =========================================================
   ANIMATION FINISHED
   ========================================================= */

static void page_animation_finished(
    lv_anim_t *anim)
{
    (void)anim;

    page_animating = false;

    /*
     * The current page is already updated when a
     * committed swipe finishes.
     *
     * Put everything into its clean resting position.
     */
    place_all_pages_normal();

    swipe_direction = 0;
}


/* =========================================================
   ANIMATE SINGLE PAGE
   ========================================================= */

static void animate_page_to(
    lv_obj_t *page,
    int32_t target_x,
    bool ready_callback)
{
    int32_t start_x = lv_obj_get_x(page);

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
        SWIPE_ANIMATION_TIME
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
   CANCEL / SNAP BACK
   ========================================================= */

static void cancel_swipe()
{
    if(!pages_ready)
        return;

    stop_page_animations();

    page_animating = true;

    /*
     * Current page returns to center.
     */
    animate_page_to(
        pages[currentPage],
        0,
        true
    );

    /*
     * Put all other pages back outside the screen.
     */
    for(int i = 0; i < 3; i++)
    {
        if(i == currentPage)
            continue;

        int target_x =
            (i < currentPage)
                ? -SCREEN_W
                : SCREEN_W;

        animate_page_to(
            pages[i],
            target_x,
            false
        );
    }
}


/* =========================================================
   COMMIT SWIPE
   ========================================================= */

static void commit_swipe(
    PageType targetPage,
    int direction)
{
    if(!pages_ready)
        return;

    stop_page_animations();

    swipe_target_page = targetPage;
    swipe_direction = direction;

    /*
     * Target page is already positioned at:
     *
     * RIGHT swipe:
     * target = -SCREEN_W
     *
     * LEFT swipe:
     * target = +SCREEN_W
     */
    lv_obj_set_x(
        pages[targetPage],
        direction > 0
            ? -SCREEN_W
            : SCREEN_W
    );

    /*
     * Current page leaves in the same direction
     * the finger moved.
     */
    int current_target =
        direction > 0
            ? SCREEN_W
            : -SCREEN_W;

    /*
     * IMPORTANT:
     *
     * currentPage is changed NOW.
     *
     * This means the new page becomes the logical page
     * immediately, while the animation visually completes.
     */
    currentPage = targetPage;

    page_animating = true;

    /*
     * Animate old page.
     *
     * We need to find the page that was previously current.
     */
    PageType oldPage;

    if(direction > 0)
    {
        oldPage =
            (targetPage == PAGE_MESSAGES)
                ? PAGE_RADAR
                : PAGE_MESSAGES;
    }
    else
    {
        oldPage =
            (targetPage == PAGE_STATUS)
                ? PAGE_RADAR
                : PAGE_STATUS;
    }

    /*
     * Animate old page out.
     */
    animate_page_to(
        pages[oldPage],
        current_target,
        false
    );

    /*
     * Animate new page to center.
     *
     * It owns the ready callback.
     */
    animate_page_to(
        pages[targetPage],
        0,
        true
    );
}


/* =========================================================
   RADAR PAGE
   ========================================================= */

static void build_radar_page(
    lv_obj_t *parent)
{
    radar = parent;

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


    /* ---------------------------------------------
       CENTER DOT
    --------------------------------------------- */

    center_dot = lv_obj_create(parent);

    lv_obj_set_size(
        center_dot,
        14,
        14
    );

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

    lv_obj_set_style_border_width(
        center_dot,
        0,
        0
    );

    lv_obj_align(
        center_dot,
        LV_ALIGN_CENTER,
        0,
        0
    );


    /* ---------------------------------------------
       RADAR RINGS
    --------------------------------------------- */

    for(int i = 0; i < 3; i++)
    {
        rings[i] =
            lv_obj_create(parent);

        int size =
            120 + i * 100;

        lv_obj_set_size(
            rings[i],
            size,
            size
        );

        lv_obj_center(
            rings[i]
        );

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

        lv_obj_set_style_border_color(
            rings[i],
            lv_color_hex(0x003300),
            0
        );

        lv_obj_set_style_border_width(
            rings[i],
            1,
            0
        );
    }


    /* ---------------------------------------------
       TARGETS
    --------------------------------------------- */

    for(int i = 0; i < MAX_TARGETS; i++)
    {
        targets[i] =
            lv_obj_create(parent);

        lv_obj_set_size(
            targets[i],
            14,
            14
        );

        lv_obj_set_style_radius(
            targets[i],
            LV_RADIUS_CIRCLE,
            0
        );

        lv_obj_set_style_bg_color(
            targets[i],
            lv_color_hex(0xFFFFFF),
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


        pulse[i] =
            lv_obj_create(parent);

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
            lv_color_hex(0xFFFFFF),
            0
        );

        lv_obj_set_style_border_width(
            pulse[i],
            0,
            0
        );

        lv_obj_set_style_bg_opa(
            pulse[i],
            LV_OPA_30,
            0
        );

        lv_obj_add_flag(
            pulse[i],
            LV_OBJ_FLAG_HIDDEN
        );
    }


    /* ---------------------------------------------
       SWEEP LINE
    --------------------------------------------- */

    sweep_line =
        lv_line_create(parent);

    lv_obj_set_size(
        sweep_line,
        SCREEN_W,
        SCREEN_H
    );

    lv_obj_center(
        sweep_line
    );

    lv_obj_set_style_line_width(
        sweep_line,
        2,
        0
    );

    lv_obj_set_style_line_color(
        sweep_line,
        lv_color_hex(0x00FF00),
        0
    );

    lv_obj_set_style_line_opa(
        sweep_line,
        LV_OPA_70,
        0
    );

    sweep_points[0].x = CENTER_X;
    sweep_points[0].y = CENTER_Y;

    sweep_points[1].x = CENTER_X;
    sweep_points[1].y = CENTER_Y;

    lv_line_set_points(
        sweep_line,
        sweep_points,
        2
    );
}


/* =========================================================
   MESSAGES PAGE
   ========================================================= */

static void build_messages_page(
    lv_obj_t *parent)
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

    msg_label =
        lv_label_create(parent);

    lv_label_set_text(
        msg_label,
        "NO MESSAGES"
    );

    lv_obj_center(
        msg_label
    );
}


/* =========================================================
   STATUS PAGE
   ========================================================= */

static void build_status_page(
    lv_obj_t *parent)
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


    status_container =
        lv_obj_create(parent);

    lv_obj_set_size(
        status_container,
        400,
        380
    );

    lv_obj_center(
        status_container
    );

    lv_obj_set_style_bg_opa(
        status_container,
        LV_OPA_TRANSP,
        0
    );

    lv_obj_set_style_border_width(
        status_container,
        0,
        0
    );

    lv_obj_set_flex_flow(
        status_container,
        LV_FLEX_FLOW_COLUMN
    );


    lv_obj_t *title =
        lv_label_create(
            status_container
        );

    lv_label_set_text(
        title,
        "SELF STATUS"
    );


    lv_obj_t *self =
        lv_label_create(
            status_container
        );

    lv_label_set_text(
        self,
        "HR: 0\n"
        "HEALTH: DEAD\n"
        "LAT: 22.82\n"
        "LON: 75.94"
    );


    lv_obj_t *others =
        lv_label_create(
            status_container
        );

    lv_label_set_text(
        others,
        "\nOTHERS:\n"
        "No data"
    );
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


    /*
     * Create all three pages.
     */

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


    /*
     * Build pages.
     */

    build_radar_page(
        pages[PAGE_RADAR]
    );

    build_messages_page(
        pages[PAGE_MESSAGES]
    );

    build_status_page(
        pages[PAGE_STATUS]
    );


    currentPage =
        PAGE_RADAR;

    pages_ready = true;

    place_all_pages_normal();
}


/* =========================================================
   SWIPE BEGIN
   ========================================================= */

bool Page_Swipe_Begin(
    int16_t start_x,
    int16_t start_y)
{
    (void)start_x;
    (void)start_y;

    if(!pages_ready)
        return false;

    if(page_animating)
        stop_page_animations();

    page_dragging = true;

    swipe_direction = 0;

    swipe_target_page =
        currentPage;

    /*
     * Make absolutely sure the current page is centered
     * before beginning a new drag.
     */
    lv_obj_set_x(
        pages[currentPage],
        0
    );

    return true;
}


/* =========================================================
   SWIPE UPDATE
   ========================================================= */

void Page_Swipe_Update(
    int16_t dx)
{
    if(!pages_ready)
        return;

    if(!page_dragging)
        return;


    /*
     * Determine which page is being dragged toward.
     */
    PageType target;
    int direction;

    bool valid =
        get_swipe_target(
            dx,
            &target,
            &direction
        );


    /*
     * No valid page in that direction.
     *
     * Give the current page a small rubber-band effect.
     */
    if(!valid)
    {
        swipe_direction = 0;

        int16_t resisted =
            dx / 4;

        if(resisted > 50)
            resisted = 50;

        if(resisted < -50)
            resisted = -50;

        lv_obj_set_x(
            pages[currentPage],
            resisted
        );

        return;
    }


    /*
     * First horizontal movement.
     *
     * Put target page directly beside current page.
     */
    if(swipe_direction != direction)
    {
        swipe_direction = direction;
        swipe_target_page = target;

        /*
         * Right:
         *
         * MESSAGES | RADAR
         *   -412       0
         *
         * Left:
         *
         * RADAR | STATUS
         *   0       +412
         */
        lv_obj_set_x(
            pages[target],
            direction > 0
                ? -SCREEN_W
                : SCREEN_W
        );
    }


    /*
     * Keep drag bounded to one screen.
     */
    if(dx > SCREEN_W)
        dx = SCREEN_W;

    if(dx < -SCREEN_W)
        dx = -SCREEN_W;


    /*
     * Current page follows finger exactly.
     */
    lv_obj_set_x(
        pages[currentPage],
        dx
    );


    /*
     * Target page follows at exactly one screen
     * distance from the current page.
     */
    lv_obj_set_x(
        pages[swipe_target_page],
        direction > 0
            ? dx - SCREEN_W
            : dx + SCREEN_W
    );
}


/* =========================================================
   SWIPE END
   ========================================================= */

void Page_Swipe_End(
    int16_t dx)
{
    if(!pages_ready)
        return;

    if(!page_dragging)
        return;

    page_dragging = false;

    /*
     * A zero dx means:
     *
     * - tap
     * - vertical gesture
     * - cancelled gesture
     *
     * Never navigate in this case.
     */
    if(dx == 0)
    {
        cancel_swipe();
        return;
    }


    /*
     * Determine target page.
     */
    PageType target;
    int direction;

    bool valid =
        get_swipe_target(
            dx,
            &target,
            &direction
        );


    /*
     * No page in this direction.
     */
    if(!valid)
    {
        cancel_swipe();
        return;
    }


    /*
     * Commit only after the finger has travelled
     * a modest distance.
     */
    int abs_dx =
        dx < 0
            ? -dx
            : dx;

    if(abs_dx < SWIPE_THRESHOLD)
    {
        cancel_swipe();
        return;
    }


    /*
     * Commit.
     */
    commit_swipe(
        target,
        direction
    );
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

    page_dragging = false;

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

    sweep_angle += 8;

    if(sweep_angle > 360)
        sweep_angle = 0;


    float rad =
        sweep_angle *
        3.14159f /
        180.0f;


    sweep_points[0].x =
        CENTER_X;

    sweep_points[0].y =
        CENTER_Y;


    sweep_points[1].x =
        CENTER_X +
        cos(rad) *
        RADAR_RADIUS;

    sweep_points[1].y =
        CENTER_Y +
        sin(rad) *
        RADAR_RADIUS;


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

    page_dragging = false;

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

    page_dragging = false;

    place_all_pages_normal();
}


/* =========================================================
   LEGACY PAGE SWITCH
   ========================================================= */

void Switch_Page(int dir)
{
    if(!pages_ready)
        return;

    if(page_animating)
        stop_page_animations();


    PageType target;
    int direction;


    /*
     * Preserve compatibility with the old
     * Switch_Page(int dir) API.
     */

    if(dir > 0)
    {
        if(currentPage == PAGE_RADAR)
        {
            target = PAGE_STATUS;
            direction = -1;
        }
        else if(currentPage == PAGE_MESSAGES)
        {
            target = PAGE_RADAR;
            direction = 1;
        }
        else
        {
            return;
        }
    }
    else if(dir < 0)
    {
        if(currentPage == PAGE_RADAR)
        {
            target = PAGE_MESSAGES;
            direction = 1;
        }
        else if(currentPage == PAGE_STATUS)
        {
            target = PAGE_RADAR;
            direction = -1;
        }
        else
        {
            return;
        }
    }
    else
    {
        return;
    }


    /*
     * Prepare target.
     */
    lv_obj_set_x(
        pages[target],
        direction > 0
            ? -SCREEN_W
            : SCREEN_W
    );


    /*
     * Animate exactly like a normal committed swipe.
     */
    commit_swipe(
        target,
        direction
    );
}