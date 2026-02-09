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
    #define COMPASS_SIZE        200
    #define KEYBOARD_VISIBLE    1
#elif defined(DISPLAY_RT1050)
    #define STATUS_HEIGHT       24
    #define INPUT_HEIGHT        32
    #define COMPASS_SIZE        100
    #define KEYBOARD_VISIBLE    0
#else /* DISPLAY_DESKTOP */
    #define STATUS_HEIGHT       32
    #define INPUT_HEIGHT        40
    #define COMPASS_SIZE        150
    #define KEYBOARD_VISIBLE    0
#endif

static void layout_portrait(lv_obj_t *scr);
static void layout_landscape(lv_obj_t *scr);

void zork_ui_init(void)
{
    zork_theme_init();

    lv_obj_t *scr = lv_screen_active();

#if defined(DISPLAY_RT1050)
    layout_landscape(scr);
#else
    layout_portrait(scr);
#endif
}

/*
 * Portrait layout (RT1170 / Desktop):
 *   status -> output -> input -> compass -> [keyboard]
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

    lv_obj_t *compass = zork_compass_create(scr);
    lv_obj_set_size(compass, COMPASS_SIZE, COMPASS_SIZE);
    lv_obj_set_style_align(compass, LV_ALIGN_CENTER, 0);

    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, zork_input_get_textarea());
#if KEYBOARD_VISIBLE
    lv_obj_set_width(kb, lv_pct(100));
#else
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
#endif
}

/*
 * Landscape layout (RT1050):
 *   Row 1: status bar (full width)
 *   Row 2: output (flex grow) + compass (fixed) side-by-side
 *   Row 3: input field (full width)
 */
static void layout_landscape(lv_obj_t *scr)
{
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_gap(scr, 0, 0);

    lv_obj_t *status = zork_status_create(scr);
    lv_obj_set_size(status, lv_pct(100), STATUS_HEIGHT);

    /* Middle row: output + compass side by side */
    lv_obj_t *mid_row = lv_obj_create(scr);
    lv_obj_remove_style_all(mid_row);
    lv_obj_set_width(mid_row, lv_pct(100));
    lv_obj_set_flex_grow(mid_row, 1);
    lv_obj_set_flex_flow(mid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(mid_row, 0, 0);
    lv_obj_set_style_pad_gap(mid_row, 0, 0);

    lv_obj_t *output_ta = zork_output_create(mid_row);
    lv_obj_set_height(output_ta, lv_pct(100));
    lv_obj_set_flex_grow(output_ta, 1);

    lv_obj_t *compass = zork_compass_create(mid_row);
    lv_obj_set_size(compass, COMPASS_SIZE, COMPASS_SIZE);

    lv_obj_t *input = zork_input_create(scr);
    lv_obj_set_size(input, lv_pct(100), INPUT_HEIGHT);

    /* Keyboard hidden by default on RT1050 */
    lv_obj_t *kb = lv_keyboard_create(scr);
    lv_keyboard_set_textarea(kb, zork_input_get_textarea());
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}
