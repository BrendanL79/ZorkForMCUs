#ifndef ZORK_COMPASS_H
#define ZORK_COMPASS_H

#include "lvgl.h"

typedef void (*zork_compass_dir_cb_t)(const char *direction);

/* Create the compass rose widget. Returns the container. */
lv_obj_t *zork_compass_create(lv_obj_t *parent);

/* Set callback for direction selection. */
void zork_compass_set_dir_cb(zork_compass_dir_cb_t cb);

#endif /* ZORK_COMPASS_H */
