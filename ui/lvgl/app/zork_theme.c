/*
 * zork_theme.c
 *
 * Custom LVGL v9 theme: retro green-on-black terminal aesthetic.
 */

#include "zork_theme.h"
#include "lvgl.h"
#include "src/themes/lv_theme_private.h"
#include <string.h>

/* Color palette — matches the Qt/QUL Zork UI */
#define ZORK_COLOR_BG           lv_color_hex(0x1a1a2e)  /* Dark blue-black */
#define ZORK_COLOR_TEXT         lv_color_hex(0x00ff88)  /* Classic green terminal */
#define ZORK_COLOR_INPUT_BG    lv_color_hex(0x0f3460)  /* Deep blue input area */
#define ZORK_COLOR_INPUT_TEXT  lv_color_hex(0xffffff)  /* White typed text */
#define ZORK_COLOR_STATUS_BG   lv_color_hex(0x16213e)  /* Dark navy status bar */
#define ZORK_COLOR_STATUS_TEXT lv_color_hex(0xe8e8e8)  /* Light grey room name */
#define ZORK_COLOR_STATUS_DIM  lv_color_hex(0xa0a0a0)  /* Dimmer score text */
#define ZORK_COLOR_KB_BG       lv_color_hex(0x1a1a2e)  /* Match app background */
#define ZORK_COLOR_KB_KEY_BG   lv_color_hex(0x263238)  /* Slate grey keys */
#define ZORK_COLOR_KB_KEY_TEXT lv_color_hex(0xffffff)  /* White key labels */
#define ZORK_COLOR_KB_SPECIAL  lv_color_hex(0x01579b)  /* Blue special keys */
#define ZORK_COLOR_SCROLLBAR   lv_color_hex(0x004400)

static lv_style_t style_screen;
static lv_style_t style_scrollbar;
static lv_style_t style_textarea;
static lv_style_t style_textarea_cursor;
static lv_style_t style_label;
static lv_style_t style_keyboard;
static lv_style_t style_keyboard_items;
static lv_style_t style_keyboard_items_checked;
static lv_style_t style_container;

static lv_theme_t *zork_theme;

static void styles_init(void)
{
    /* Screen background: black */
    lv_style_init(&style_screen);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_bg_color(&style_screen, ZORK_COLOR_BG);
    lv_style_set_text_color(&style_screen, ZORK_COLOR_TEXT);
    lv_style_set_text_font(&style_screen, LV_FONT_DEFAULT);

    /* Scrollbar: dark green, thin */
    lv_style_init(&style_scrollbar);
    lv_style_set_bg_opa(&style_scrollbar, LV_OPA_COVER);
    lv_style_set_bg_color(&style_scrollbar, ZORK_COLOR_SCROLLBAR);
    lv_style_set_width(&style_scrollbar, 4);
    lv_style_set_pad_right(&style_scrollbar, 2);

    /* Textarea: green text on black, no border */
    lv_style_init(&style_textarea);
    lv_style_set_bg_opa(&style_textarea, LV_OPA_COVER);
    lv_style_set_bg_color(&style_textarea, ZORK_COLOR_BG);
    lv_style_set_text_color(&style_textarea, ZORK_COLOR_TEXT);
    lv_style_set_border_width(&style_textarea, 0);
    lv_style_set_radius(&style_textarea, 0);
    lv_style_set_pad_all(&style_textarea, 4);

    /* Textarea cursor: green line */
    lv_style_init(&style_textarea_cursor);
    lv_style_set_border_side(&style_textarea_cursor, LV_BORDER_SIDE_LEFT);
    lv_style_set_border_color(&style_textarea_cursor, ZORK_COLOR_TEXT);
    lv_style_set_border_width(&style_textarea_cursor, 2);
    lv_style_set_bg_opa(&style_textarea_cursor, LV_OPA_TRANSP);
    lv_style_set_anim_duration(&style_textarea_cursor, 500);

    /* Label: green text */
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, ZORK_COLOR_TEXT);

    /* Keyboard background: dark grey */
    lv_style_init(&style_keyboard);
    lv_style_set_bg_opa(&style_keyboard, LV_OPA_COVER);
    lv_style_set_bg_color(&style_keyboard, ZORK_COLOR_KB_BG);
    lv_style_set_border_width(&style_keyboard, 0);
    lv_style_set_pad_all(&style_keyboard, 2);
    lv_style_set_pad_gap(&style_keyboard, 2);

    /* Keyboard key items: grey bg, green text */
    lv_style_init(&style_keyboard_items);
    lv_style_set_bg_opa(&style_keyboard_items, LV_OPA_COVER);
    lv_style_set_bg_color(&style_keyboard_items, ZORK_COLOR_KB_KEY_BG);
    lv_style_set_text_color(&style_keyboard_items, ZORK_COLOR_KB_KEY_TEXT);
    lv_style_set_border_width(&style_keyboard_items, 0);
    lv_style_set_radius(&style_keyboard_items, 4);

    /* Keyboard checked/special keys */
    lv_style_init(&style_keyboard_items_checked);
    lv_style_set_bg_color(&style_keyboard_items_checked, ZORK_COLOR_KB_SPECIAL);
    lv_style_set_text_color(&style_keyboard_items_checked, ZORK_COLOR_KB_KEY_TEXT);

    /* Generic container: transparent bg, no border */
    lv_style_init(&style_container);
    lv_style_set_bg_opa(&style_container, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_container, 0);
    lv_style_set_pad_all(&style_container, 0);
    lv_style_set_radius(&style_container, 0);
}

static void theme_apply_cb(lv_theme_t *th, lv_obj_t *obj)
{
    LV_UNUSED(th);

    lv_obj_t *parent = lv_obj_get_parent(obj);

    if (parent == NULL) {
        lv_obj_add_style(obj, &style_screen, 0);
        lv_obj_add_style(obj, &style_scrollbar, LV_PART_SCROLLBAR);
        return;
    }

    if (lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_add_style(obj, &style_textarea, 0);
        lv_obj_add_style(obj, &style_scrollbar, LV_PART_SCROLLBAR);
        lv_obj_add_style(obj, &style_textarea_cursor,
                         LV_PART_CURSOR | LV_STATE_FOCUSED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_keyboard_class)) {
        lv_obj_add_style(obj, &style_keyboard, 0);
        lv_obj_add_style(obj, &style_keyboard_items, LV_PART_ITEMS);
        lv_obj_add_style(obj, &style_keyboard_items_checked,
                         LV_PART_ITEMS | LV_STATE_CHECKED);
        return;
    }

    if (lv_obj_check_type(obj, &lv_label_class)) {
        lv_obj_add_style(obj, &style_label, 0);
        return;
    }

    if (lv_obj_check_type(obj, &lv_obj_class)) {
        lv_obj_add_style(obj, &style_container, 0);
        lv_obj_add_style(obj, &style_scrollbar, LV_PART_SCROLLBAR);
        return;
    }
}

void zork_theme_init(void)
{
    styles_init();

    zork_theme = lv_malloc_zeroed(sizeof(lv_theme_t));
    lv_theme_set_apply_cb(zork_theme, theme_apply_cb);

    lv_display_t *disp = lv_display_get_default();
    lv_display_set_theme(disp, zork_theme);
}
