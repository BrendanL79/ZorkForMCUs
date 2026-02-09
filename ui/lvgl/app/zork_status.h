#ifndef ZORK_STATUS_H
#define ZORK_STATUS_H

#include "lvgl.h"

/* Create the status bar. Returns the container widget. */
lv_obj_t *zork_status_create(lv_obj_t *parent);

/* Update status bar text. */
void zork_status_update(const char *room, const char *score);

#endif /* ZORK_STATUS_H */
