/*
 * DisplayConfig.h
 *
 * Display profile configuration for different target boards.
 * Selected at build time via -DDISPLAY_PROFILE=RT1050 or RT1170.
 *
 * This file is toolkit-agnostic and shared by all UI implementations
 * (Qt for MCUs, LVGL, Slint, etc.).
 */

#ifndef DISPLAYCONFIG_H
#define DISPLAYCONFIG_H

// Display profile selection - set by CMake
// Defaults to RT1050 if not specified
#if !defined(DISPLAY_RT1050) && !defined(DISPLAY_RT1170) && !defined(DISPLAY_RT1170_SCALED)
#define DISPLAY_RT1050
#endif

/*
 * RT1050 EVK - 480x272 landscape LCD
 */
#ifdef DISPLAY_RT1050
    #define DISPLAY_WIDTH           480
    #define DISPLAY_HEIGHT          272
    #define DISPLAY_ORIENTATION     0       // Landscape
    #define DISPLAY_SHOW_VKEYBOARD  1       // Virtual keyboard enabled
    #define DISPLAY_VKEYBOARD_ALWAYS 0      // Only show when input focused

    // UI sizing
    #define DISPLAY_FONT_SIZE       14
    #define DISPLAY_STATUS_HEIGHT   24
    #define DISPLAY_INPUT_HEIGHT    32
    #define DISPLAY_MARGIN          4
#endif

/*
 * RT1170 EVK-B - 720x1280 portrait LCD (RK055HDMIPI4MA0)
 */
#ifdef DISPLAY_RT1170
    #define DISPLAY_WIDTH           720
    #define DISPLAY_HEIGHT          1280
    #define DISPLAY_ORIENTATION     1       // Portrait
    #define DISPLAY_SHOW_VKEYBOARD  1       // Virtual keyboard enabled
    #define DISPLAY_VKEYBOARD_ALWAYS 1      // Always visible (portrait mode)

    // UI sizing - scaled for larger display
    #define DISPLAY_FONT_SIZE       24
    #define DISPLAY_STATUS_HEIGHT   48
    #define DISPLAY_INPUT_HEIGHT    56
    #define DISPLAY_MARGIN          8
#endif

/*
 * RT1170_SCALED - 75% scale for desktop testing (540x960)
 * Same layout as RT1170 but fits on smaller monitors
 */
#ifdef DISPLAY_RT1170_SCALED
    #define DISPLAY_WIDTH           540
    #define DISPLAY_HEIGHT          960
    #define DISPLAY_ORIENTATION     1       // Portrait
    #define DISPLAY_SHOW_VKEYBOARD  1       // Virtual keyboard enabled
    #define DISPLAY_VKEYBOARD_ALWAYS 1      // Always visible (portrait mode)

    // UI sizing - 75% of RT1170
    #define DISPLAY_FONT_SIZE       18
    #define DISPLAY_STATUS_HEIGHT   36
    #define DISPLAY_INPUT_HEIGHT    42
    #define DISPLAY_MARGIN          6
#endif

#endif // DISPLAYCONFIG_H
