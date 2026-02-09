#ifndef FIZMO_LVGL_ADAPTER_H
#define FIZMO_LVGL_ADAPTER_H

#include "lvgl.h"

/* Initialize the fizmo<->LVGL adapter.
 * Registers an lv_timer for periodic output polling.
 * Must be called after zork_ui_init() and lv_init(). */
void fizmo_lvgl_adapter_init(void);

/* Submit a command string to the fizmo interpreter. */
void fizmo_lvgl_submit_command(const char *cmd);

/* Submit a single character (for [MORE] prompt handling). */
void fizmo_lvgl_submit_char(char c);

#endif /* FIZMO_LVGL_ADAPTER_H */
