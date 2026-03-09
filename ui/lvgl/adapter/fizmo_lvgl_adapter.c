/*
 * fizmo_lvgl_adapter.c
 *
 * Bridges the fizmo interpreter to the LVGL UI.
 * Polls fizmo output via an lv_timer, converts UTF-32 to UTF-8,
 * and pushes text to the output textarea.
 */

#include "fizmo_lvgl_adapter.h"
#include "fizmo_bridge.h"
#include "zork_output.h"
#include "zork_input.h"
#include "zork_status.h"
#include "zork_compass.h"

#include <string.h>
#include <stdio.h>

static uint32_t s_readBuf[256];
static char s_prevRoom[128];
static char s_prevScore[64];

static void poll_fizmo_cb(lv_timer_t *timer);
static void on_command_submit(const char *cmd);
static void on_compass_dir(const char *direction);
static void on_char_submit(char c);

void fizmo_lvgl_adapter_init(void)
{
    zork_input_set_submit_cb(on_command_submit);
    zork_input_set_char_cb(on_char_submit);
    zork_compass_set_dir_cb(on_compass_dir);

    lv_timer_create(poll_fizmo_cb, 50, NULL);
}

void fizmo_lvgl_submit_command(const char *cmd)
{
    on_command_submit(cmd);
}

void fizmo_lvgl_submit_char(char c)
{
    on_char_submit(c);
}

static size_t utf32_to_utf8(const uint32_t *src, size_t count,
                            char *dst, size_t dst_size)
{
    size_t written = 0;

    for (size_t i = 0; i < count; i++) {
        uint32_t ch = src[i];

        if (ch < 0x80) {
            if (written + 1 >= dst_size) break;
            dst[written++] = (char)ch;
        } else if (ch < 0x800) {
            if (written + 2 >= dst_size) break;
            dst[written++] = (char)(0xC0 | (ch >> 6));
            dst[written++] = (char)(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            if (written + 3 >= dst_size) break;
            dst[written++] = (char)(0xE0 | (ch >> 12));
            dst[written++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            dst[written++] = (char)(0x80 | (ch & 0x3F));
        } else {
            if (written + 4 >= dst_size) break;
            dst[written++] = (char)(0xF0 | (ch >> 18));
            dst[written++] = (char)(0x80 | ((ch >> 12) & 0x3F));
            dst[written++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            dst[written++] = (char)(0x80 | (ch & 0x3F));
        }
    }

    dst[written] = '\0';
    return written;
}

static void poll_fizmo_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Drain output from fizmo */
    size_t avail = fizmo_output_available();
    while (avail > 0) {
        size_t n = fizmo_output_read(s_readBuf, 256);
        if (n == 0) break;

        char utf8_buf[256 * 4 + 1];
        utf32_to_utf8(s_readBuf, n, utf8_buf, sizeof(utf8_buf));
        zork_output_append(utf8_buf);

        avail = fizmo_output_available();
    }

    /* Update input state */
    if (fizmo_has_exited()) {
        zork_input_set_enabled(false);
        return;
    }

    bool waiting = fizmo_waiting_for_input() || fizmo_waiting_for_char();
    zork_input_set_enabled(waiting);

    /* Update status bar if changed */
    char room[128];
    char score[64];
    if (fizmo_get_status_line(room, sizeof(room), score, sizeof(score))) {
        if (strcmp(room, s_prevRoom) != 0 || strcmp(score, s_prevScore) != 0) {
            zork_status_update(room, score);
            strncpy(s_prevRoom, room, sizeof(s_prevRoom) - 1);
            s_prevRoom[sizeof(s_prevRoom) - 1] = '\0';
            strncpy(s_prevScore, score, sizeof(s_prevScore) - 1);
            s_prevScore[sizeof(s_prevScore) - 1] = '\0';
        }
    }
}

static void on_command_submit(const char *cmd)
{
    if (fizmo_waiting_for_char()) {
        /* In character mode, send just the first character */
        fizmo_submit_char((cmd && cmd[0]) ? (uint32_t)cmd[0] : (uint32_t)' ');
    } else {
        /* Echo the command (fizmo already outputs the ">" prompt) */
        char echo_buf[512];
        snprintf(echo_buf, sizeof(echo_buf), "%s\n", cmd);
        zork_output_append(echo_buf);
        fizmo_submit_line(cmd);
    }
}

static void on_compass_dir(const char *direction)
{
    on_command_submit(direction);
}

static void on_char_submit(char c)
{
    fizmo_submit_char((uint32_t)c);
}
