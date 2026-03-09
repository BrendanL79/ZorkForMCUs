/*
 * zork_status.c
 *
 * Status bar with room name (left) and score/moves (right).
 */

#include "zork_status.h"
#include "zork_fonts.h"
#include "lvgl.h"

#if defined(DISPLAY_RT1170)
    #define STATUS_MARGIN   8
#elif defined(DISPLAY_RT1050)
    #define STATUS_MARGIN   4
#elif defined(DISPLAY_RT1170_SCALED)
    #define STATUS_MARGIN   6
#else
    #error "No display profile defined"
#endif

static lv_style_t style_status_bar;
static lv_style_t style_status_text;
static lv_style_t style_score_text;
static bool styles_initialized;

static lv_obj_t *room_label_ref;
static lv_obj_t *score_label_ref;

static void status_styles_init(void)
{
    if (styles_initialized) {
        return;
    }

    lv_style_init(&style_status_bar);
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
    lv_style_set_bg_color(&style_status_bar, lv_color_hex(0x16213e));
    lv_style_set_border_width(&style_status_bar, 0);
    lv_style_set_radius(&style_status_bar, 0);
    lv_style_set_pad_left(&style_status_bar, STATUS_MARGIN * 2);
    lv_style_set_pad_right(&style_status_bar, STATUS_MARGIN * 2);
    lv_style_set_pad_top(&style_status_bar, 0);
    lv_style_set_pad_bottom(&style_status_bar, 0);

    lv_style_init(&style_status_text);
    lv_style_set_text_color(&style_status_text, lv_color_hex(0xe8e8e8));
    lv_style_set_text_font(&style_status_text, ZORK_FONT);

    lv_style_init(&style_score_text);
    lv_style_set_text_color(&style_score_text, lv_color_hex(0xa0a0a0));
    lv_style_set_text_font(&style_score_text, ZORK_FONT);

    styles_initialized = true;
}

lv_obj_t *zork_status_create(lv_obj_t *parent)
{
    status_styles_init();

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_add_style(cont, &style_status_bar, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    room_label_ref = lv_label_create(cont);
    lv_obj_add_style(room_label_ref, &style_status_text, 0);
    lv_label_set_text(room_label_ref, "");

    score_label_ref = lv_label_create(cont);
    lv_obj_add_style(score_label_ref, &style_score_text, 0);
    lv_label_set_text(score_label_ref, "");

    return cont;
}

void zork_status_update(const char *room, const char *score)
{
    if (room_label_ref != NULL && room != NULL) {
        lv_label_set_text(room_label_ref, room);
    }
    if (score_label_ref != NULL && score != NULL) {
        lv_label_set_text(score_label_ref, score);
    }
}
