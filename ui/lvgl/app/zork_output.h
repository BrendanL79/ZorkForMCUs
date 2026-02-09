#ifndef ZORK_OUTPUT_H
#define ZORK_OUTPUT_H

#include "lvgl.h"

/* Create the output text area. Returns the lv_textarea widget. */
lv_obj_t *zork_output_create(lv_obj_t *parent);

/* Append text to the output, auto-scroll to bottom. */
void zork_output_append(const char *text);

/* Clear all output text. */
void zork_output_clear(void);

#endif /* ZORK_OUTPUT_H */
