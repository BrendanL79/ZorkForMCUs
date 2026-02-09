/*
 * main_desktop.c
 *
 * Desktop entry point for the Zork LVGL UI.
 * Creates an SDL window, initializes the UI, starts the fizmo interpreter,
 * and runs the LVGL event loop.
 */

#include "lvgl.h"
#include "app/zork_ui.h"
#include "adapter/fizmo_lvgl_adapter.h"
#include "fizmo_bridge.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(DISPLAY_RT1170)
#define HOR_RES 720
#define VER_RES 1280
#elif defined(DISPLAY_RT1050)
#define HOR_RES 480
#define VER_RES 272
#else
#define HOR_RES 800
#define VER_RES 480
#endif

#ifndef ZORK_STORY_PATH
#define ZORK_STORY_PATH "../../zork1.z3"
#endif

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    lv_init();

    /* Create SDL window with display and input devices */
    lv_display_t *disp = lv_sdl_window_create(HOR_RES, VER_RES);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_t *kb = lv_sdl_keyboard_create();
    (void)disp;
    (void)mouse;
    (void)kb;

    /* Build the UI widget tree */
    zork_ui_init();

    /* Determine story file path */
    const char *story_path = ZORK_STORY_PATH;
    const char *env_path = getenv("ZORK_STORY_PATH");
    if (env_path && env_path[0]) {
        story_path = env_path;
    }

    if (fizmo_bridge_init(story_path) != 0) {
        fprintf(stderr, "Failed to init fizmo bridge with story: %s\n",
                story_path);
        return 1;
    }

    /* Register the polling timer */
    fizmo_lvgl_adapter_init();

    /* Start the interpreter background thread */
    fizmo_start_interpreter();

    /* Main event loop */
    while (!fizmo_has_exited()) {
        uint32_t delay = lv_timer_handler();
#ifdef _WIN32
        Sleep(delay);
#else
        usleep(delay * 1000);
#endif
    }

    fizmo_bridge_shutdown();
    return 0;
}
