/*
 * zork_ui.c
 *
 * Master layout for the Zork LVGL UI.
 * Arranges all widgets according to the active display profile.
 *
 * Display profiles (compile-time):
 *   DISPLAY_RT1170  - 720x1280, portrait, keyboard always visible
 *   DISPLAY_RT1050  - 480x272, landscape, keyboard hidden
 *   DISPLAY_DESKTOP - 800x480 (default), portrait, keyboard hidden
 */

#include "zork_ui.h"
#include "zork_theme.h"
#include "zork_output.h"
#include "zork_status.h"
#include "zork_input.h"
#include "zork_compass.h"

#if defined(DISPLAY_RT1170)
    #define STATUS_HEIGHT       48
    #define INPUT_HEIGHT        56
    #define COMPASS_SIZE        64
    #define COMPASS_MARGIN      8
    #define KEYBOARD_VISIBLE    1
#elif defined(DISPLAY_RT1050)
    #define STATUS_HEIGHT       24
    #define INPUT_HEIGHT        32
    #define COMPASS_SIZE        64
    #define COMPASS_MARGIN      4
    #define KEYBOARD_VISIBLE    0
#else /* DISPLAY_DESKTOP */
    #define STATUS_HEIGHT       32
    #define INPUT_HEIGHT        40
    #define COMPASS_SIZE        64
    #define COMPASS_MARGIN      6
    #define KEYBOARD_VISIBLE    0
#endif

static lv_obj_t *s_keyboard;

static void layout_portrait(lv_obj_t *scr);
static void layout_landscape(lv_obj_t *scr);
static void setup_input_group(void);

void zork_ui_init(void)
{
    zork_theme_init();

    lv_obj_t *scr = lv_screen_active();

#if defined(DISPLAY_RT1050)
    layout_landscape(scr);
#else
    layout_portrait(scr);
#endif

    setup_input_group();
}

/*
 * Set up an LVGL input group so that keyboard/encoder indevs
 * route events to the input textarea. Without this, SDL keyboard
 * events have no target.
 */
static void setup_input_group(void)
{
    lv_group_t *grp = lv_group_create();
    lv_obj_t *ta = zork_input_get_textarea();
    if (ta) {
        lv_group_add_obj(grp, ta);
        lv_group_focus_obj(ta);
    }

    /* Assign the group to all keyboard-type indevs */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        lv_indev_type_t type = lv_indev_get_type(indev);
        if (type == LV_INDEV_TYPE_KEYPAD || type == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, grp);
        }
        indev = lv_indev_get_next(indev);
    }

    lv_group_set_default(grp);
}

/*
 * Portrait layout (RT1170 / Desktop):
 *   status -> output (with compass overlay at bottom-right) -> input -> [keyboard]
 *
 * The compass rose is positioned as an overlay on the output area,
 * matching the QUL layout where it floats at bottom-right with 0.7 opacity.
 */
static void layout_portrait(lv_obj_t *scr)
{
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_gap(scr, 0, 0);

    lv_obj_t *status = zork_status_create(scr);
    lv_obj_set_size(status, lv_pct(100), STATUS_HEIGHT);

    lv_obj_t *output_ta = zork_output_create(scr);
    lv_obj_set_width(output_ta, lv_pct(100));
    lv_obj_set_flex_grow(output_ta, 1);

    lv_obj_t *input = zork_input_create(scr);
    lv_obj_set_size(input, lv_pct(100), INPUT_HEIGHT);

    s_keyboard = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(s_keyboard, zork_input_get_textarea());
#if KEYBOARD_VISIBLE
    lv_obj_set_width(s_keyboard, lv_pct(100));
#else
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
#endif

    /* Compass rose: floating overlay on the screen, positioned just above
     * the input bar at the right edge. FLOATING excludes it from flex. */
    lv_obj_t *compass = zork_compass_create(scr);
    lv_obj_add_flag(compass, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_opa(compass, LV_OPA_70, 0);

    /* Scale the image down to COMPASS_SIZE. lv_image uses its native size,
     * so we compute a scale factor (256 = 1:1 in LVGL v9). */
    lv_obj_update_layout(scr);
    int32_t native_w = lv_obj_get_width(compass);
    if (native_w > 0 && native_w != COMPASS_SIZE) {
        uint32_t scale = (uint32_t)COMPASS_SIZE * 256 / (uint32_t)native_w;
        lv_image_set_scale(compass, scale);
    }

    /* Position: right edge, just above the input bar */
    lv_obj_align(compass, LV_ALIGN_BOTTOM_RIGHT,
                 -COMPASS_MARGIN, -(INPUT_HEIGHT + COMPASS_MARGIN));
    lv_obj_move_foreground(compass);
}

/*
 * Landscape layout (RT1050):
 *   Row 1: status bar (full width)
 *   Row 2: output (with compass overlay at bottom-right)
 *   Row 3: input field (full width)
 */
static void layout_landscape(lv_obj_t *scr)
{
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_gap(scr, 0, 0);

    lv_obj_t *status = zork_status_create(scr);
    lv_obj_set_size(status, lv_pct(100), STATUS_HEIGHT);

    lv_obj_t *output_ta = zork_output_create(scr);
    lv_obj_set_width(output_ta, lv_pct(100));
    lv_obj_set_flex_grow(output_ta, 1);

    lv_obj_t *input = zork_input_create(scr);
    lv_obj_set_size(input, lv_pct(100), INPUT_HEIGHT);

    /* Keyboard hidden by default on RT1050 */
    s_keyboard = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(s_keyboard, zork_input_get_textarea());
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);

    /* Compass rose: floating overlay on the screen */
    lv_obj_t *compass = zork_compass_create(scr);
    lv_obj_add_flag(compass, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_opa(compass, LV_OPA_70, 0);

    lv_obj_update_layout(scr);
    int32_t native_w = lv_obj_get_width(compass);
    if (native_w > 0 && native_w != COMPASS_SIZE) {
        uint32_t scale = (uint32_t)COMPASS_SIZE * 256 / (uint32_t)native_w;
        lv_image_set_scale(compass, scale);
    }

    lv_obj_align(compass, LV_ALIGN_BOTTOM_RIGHT,
                 -COMPASS_MARGIN, -(INPUT_HEIGHT + COMPASS_MARGIN));
    lv_obj_move_foreground(compass);
}
