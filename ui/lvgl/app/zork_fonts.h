/*
 * zork_fonts.h
 *
 * Profile-dependent CascadiaMono font selection.
 * Include after lvgl.h (needs lv_font_t to be defined).
 */

#ifndef ZORK_FONTS_H
#define ZORK_FONTS_H

#include "lvgl.h"

LV_ATTRIBUTE_EXTERN_DATA extern const lv_font_t cascadia_mono_14;
LV_ATTRIBUTE_EXTERN_DATA extern const lv_font_t cascadia_mono_18;
LV_ATTRIBUTE_EXTERN_DATA extern const lv_font_t cascadia_mono_24;

#if defined(DISPLAY_RT1170)
    #define ZORK_FONT       &cascadia_mono_24
#elif defined(DISPLAY_RT1050)
    #define ZORK_FONT       &cascadia_mono_14
#elif defined(DISPLAY_RT1170_SCALED)
    #define ZORK_FONT       &cascadia_mono_18
#else
    #define ZORK_FONT       &cascadia_mono_18
#endif

#endif /* ZORK_FONTS_H */
