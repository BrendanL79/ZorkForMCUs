/*
 * zork_input.c
 *
 * Input field with ">" prompt and single-line textarea.
 */

#include "zork_input.h"
#include "lvgl.h"
#include <string.h>

static lv_obj_t *s_input_ta;
static zork_input_submit_cb_t s_submit_cb;
static zork_input_char_cb_t s_char_cb;

static void input_ready_cb(lv_event_t *e)
{
    (void)e;
    if (s_input_ta == NULL) return;

    const char *txt = lv_textarea_get_text(s_input_ta);
    if (s_submit_cb && txt && txt[0] != '\0') {
        s_submit_cb(txt);
    }
    lv_textarea_set_text(s_input_ta, "");
}

lv_obj_t *zork_input_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_set_style_pad_gap(cont, 4, 0);

    lv_obj_t *prompt = lv_label_create(cont);
    lv_label_set_text(prompt, ">");
    lv_obj_set_style_text_color(prompt, lv_color_hex(0x00FF00), 0);

    s_input_ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(s_input_ta, true);
    lv_textarea_set_text(s_input_ta, "");
    lv_obj_set_flex_grow(s_input_ta, 1);
    lv_obj_set_height(s_input_ta, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(s_input_ta, input_ready_cb, LV_EVENT_READY, NULL);

    return cont;
}

lv_obj_t *zork_input_get_textarea(void)
{
    return s_input_ta;
}

void zork_input_set_submit_cb(zork_input_submit_cb_t cb)
{
    s_submit_cb = cb;
}

void zork_input_set_char_cb(zork_input_char_cb_t cb)
{
    s_char_cb = cb;
}

void zork_input_set_enabled(bool enabled)
{
    if (s_input_ta == NULL) return;

    if (enabled) {
        lv_obj_clear_state(s_input_ta, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_input_ta, LV_STATE_DISABLED);
    }
}

void zork_input_clear(void)
{
    if (s_input_ta == NULL) return;
    lv_textarea_set_text(s_input_ta, "");
}
