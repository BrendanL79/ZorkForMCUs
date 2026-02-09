#ifndef ZORK_INPUT_H
#define ZORK_INPUT_H

#include "lvgl.h"

typedef void (*zork_input_submit_cb_t)(const char *cmd);
typedef void (*zork_input_char_cb_t)(char c);

/* Create input field (prompt label + textarea). Returns container. */
lv_obj_t *zork_input_create(lv_obj_t *parent);

/* Get the textarea widget (for linking to a keyboard). */
lv_obj_t *zork_input_get_textarea(void);

/* Set callback for command submission (Enter pressed). */
void zork_input_set_submit_cb(zork_input_submit_cb_t cb);

/* Set callback for single character input. */
void zork_input_set_char_cb(zork_input_char_cb_t cb);

/* Enable/disable the input field. */
void zork_input_set_enabled(bool enabled);

/* Clear the input field text. */
void zork_input_clear(void);

#endif /* ZORK_INPUT_H */
