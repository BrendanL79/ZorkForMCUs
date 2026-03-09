/*
 * zork_output.c
 *
 * Read-only scrollable output text area for Zork game output.
 * Supports auto-scroll to bottom and scrollback trimming.
 */

#include "zork_output.h"
#include "lvgl.h"
#include <string.h>

#if defined(DISPLAY_RT1050)
    #define MAX_OUTPUT_CHARS    4096
#else
    #define MAX_OUTPUT_CHARS    16384
#endif

static lv_obj_t *output_ta;

static void trim_scrollback(void)
{
    const char *text = lv_textarea_get_text(output_ta);
    size_t len = strlen(text);

    if (len <= MAX_OUTPUT_CHARS) {
        return;
    }

    size_t trim_target = len / 4;
    size_t trim_pos = 0;

    for (size_t i = 0; i < trim_target && i < len; i++) {
        if (text[i] == '\n') {
            trim_pos = i + 1;
        }
    }

    if (trim_pos == 0) {
        for (size_t i = trim_target; i < len; i++) {
            if (text[i] == '\n') {
                trim_pos = i + 1;
                break;
            }
        }
    }

    if (trim_pos > 0 && trim_pos < len) {
        lv_textarea_set_text(output_ta, text + trim_pos);
    }
}

lv_obj_t *zork_output_create(lv_obj_t *parent)
{
    output_ta = lv_textarea_create(parent);

    lv_textarea_set_cursor_click_pos(output_ta, false);
    lv_obj_remove_flag(output_ta, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_textarea_set_max_length(output_ta, MAX_OUTPUT_CHARS + 1024);
    lv_textarea_set_text(output_ta, "");

    /* Padding matches QUL: margin on all sides, extra right for compass overlay */
    lv_obj_set_style_pad_left(output_ta, 6, 0);
    lv_obj_set_style_pad_top(output_ta, 6, 0);
    lv_obj_set_style_pad_bottom(output_ta, 6, 0);
    lv_obj_set_style_pad_right(output_ta, 70, 0);

    return output_ta;
}

void zork_output_append(const char *text)
{
    if (output_ta == NULL || text == NULL) {
        return;
    }

    lv_textarea_add_text(output_ta, text);
    trim_scrollback();
    lv_obj_scroll_to_y(output_ta, LV_COORD_MAX, LV_ANIM_OFF);
}

void zork_output_clear(void)
{
    if (output_ta == NULL) {
        return;
    }
    lv_textarea_set_text(output_ta, "");
}
