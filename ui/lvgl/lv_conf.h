/**
 * @file lv_conf.h
 * LVGL v9.4.x configuration for ZorkForMCUs
 *
 * Desktop-first SDL2 build. MCU targets override specific settings
 * (color depth, memory, OS, drivers) via their own build definitions.
 */

#if 1 /* Set to 0 to disable the entire LVGL configuration */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*=======================
 * COLOR SETTINGS
 *=======================*/

/* 32-bit ARGB8888 for desktop SDL. MCU builds override to 16. */
#define LV_COLOR_DEPTH 32

/*=======================
 * MEMORY SETTINGS
 *=======================*/

/* 128 KB - generous for desktop; MCU builds may reduce this. */
#define LV_MEM_SIZE (128 * 1024)

/*=======================
 * OPERATING SYSTEM
 *=======================*/

/* No OS for the desktop SDL build.
 * MCU builds with FreeRTOS should define LV_USE_OS LV_OS_FREERTOS. */
#define LV_USE_OS LV_OS_NONE

/*=======================
 * DISPLAY SETTINGS
 *=======================*/

/* ~30 FPS refresh rate */
#define LV_DEF_REFR_PERIOD 33

/*=======================
 * DRAW BUFFER SETTINGS
 *=======================*/

#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN        4

/*=======================
 * DRAWING ENGINE
 *=======================*/

/* Software rendering; SDL GPU draw is not used. */
#define LV_USE_DRAW_SDL 0

/* Disable ARM Helium/NEON assembly -- desktop build, not MCU. */
#define LV_USE_DRAW_SW_ASM          LV_DRAW_SW_ASM_NONE
#define LV_USE_NATIVE_HELIUM_ASM    0

/*=======================
 * LOGGING
 *=======================*/

#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN

/*=======================
 * ASSERTS
 *=======================*/

#define LV_USE_ASSERT_NULL   1
#define LV_USE_ASSERT_MALLOC 1

/*=======================
 * FONTS
 *=======================*/

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*=======================
 * DRIVERS
 *=======================*/

/* SDL display and input driver */
#define LV_USE_SDL 1

/* SDL2 built from source has headers at include/SDL.h (no SDL2/ prefix) */
#define LV_SDL_INCLUDE_PATH <SDL.h>

/*=======================
 * WIDGETS
 *=======================*/

#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_LABEL    1
#define LV_USE_IMAGE    1
#define LV_USE_BTN      1

#endif /* LV_CONF_H */
#endif /* #if 1 */
