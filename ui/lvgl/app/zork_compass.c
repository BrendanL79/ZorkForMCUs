/*
 * zork_compass.c
 *
 * Compass rose with atan2-based 8-way direction detection.
 * Loads compass-rose.png at runtime via LVGL's LodePNG decoder.
 * Falls back to a text placeholder if the image fails to load.
 * 15% center dead zone ignores taps near the middle.
 */

#include "zork_compass.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>

static zork_compass_dir_cb_t s_dir_cb;
static lv_obj_t *s_compass_obj;

static const char *directions[] = {
    "n", "ne", "e", "se", "s", "sw", "w", "nw"
};

static void compass_click_cb(lv_event_t *e)
{
    (void)e;
    if (s_dir_cb == NULL || s_compass_obj == NULL) return;

    lv_point_t point;
    lv_indev_get_point(lv_indev_active(), &point);

    /* Get widget screen coordinates */
    lv_area_t area;
    lv_obj_get_coords(s_compass_obj, &area);

    float w = (float)lv_area_get_width(&area);
    float h = (float)lv_area_get_height(&area);
    float cx = (float)area.x1 + w / 2.0f;
    float cy = (float)area.y1 + h / 2.0f;
    float dx = (float)point.x - cx;
    float dy = (float)point.y - cy;

    float radius = (w < h) ? w / 2.0f : h / 2.0f;
    float dist = sqrtf(dx * dx + dy * dy);

    /* 15% dead zone in center */
    if (dist < radius * 0.15f) return;

    /* Convert to clockwise angle from North (0=N, 90=E, etc.) */
    float angle = atan2f(-dy, dx);             /* radians, CCW from East */
    float degrees = 90.0f - angle * 180.0f / 3.14159265f;
    if (degrees < 0.0f) degrees += 360.0f;

    int sector = ((int)(degrees + 22.5f) % 360) / 45;
    if (sector >= 0 && sector < 8) {
        s_dir_cb(directions[sector]);
    }
}

/* Path to the compass rose PNG — set by CMake via -DCOMPASS_ROSE_PATH=... */
#ifndef COMPASS_ROSE_PATH
#define COMPASS_ROSE_PATH "A:compass-rose.png"
#endif

lv_obj_t *zork_compass_create(lv_obj_t *parent)
{
    /* Try loading the compass rose PNG directly as an lv_image.
     * No wrapper container — avoids clipping issues. */
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, COMPASS_ROSE_PATH);

    lv_obj_update_layout(parent);
    int32_t img_w = lv_obj_get_width(img);

    if (img_w > 0) {
        fprintf(stderr, "[compass] Loaded compass rose image (%d px wide)\n", (int)img_w);
        s_compass_obj = img;
    } else {
        /* Image failed — replace with a placeholder container */
        fprintf(stderr, "[compass] Failed to load '%s', using placeholder\n",
                COMPASS_ROSE_PATH);
        lv_obj_delete(img);

        s_compass_obj = lv_obj_create(parent);
        lv_obj_remove_flag(s_compass_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(s_compass_obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(s_compass_obj, lv_color_hex(0x16213e), 0);
        lv_obj_set_style_radius(s_compass_obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_compass_obj, 1, 0);
        lv_obj_set_style_border_color(s_compass_obj, lv_color_hex(0x304060), 0);

        lv_obj_t *lbl = lv_label_create(s_compass_obj);
        lv_label_set_text(lbl, LV_SYMBOL_GPS);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xe8e8e8), 0);
    }

    /* Clickable for direction detection */
    lv_obj_add_flag(s_compass_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_compass_obj, compass_click_cb,
                        LV_EVENT_CLICKED, NULL);

    return s_compass_obj;
}

void zork_compass_set_dir_cb(zork_compass_dir_cb_t cb)
{
    s_dir_cb = cb;
}
