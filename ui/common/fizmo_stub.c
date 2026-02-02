/*
 * fizmo_stub.c
 *
 * Desktop stub implementation for testing UI without the fizmo interpreter.
 * Provides the same C API as fizmo_bridge.h / fizmo_rtos_bridge.h with
 * canned demo output.
 *
 * Shared by all UI implementations for their desktop-stub build mode.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

static const char *s_demoText =
    "ZORK I: The Great Underground Empire\n"
    "Copyright (c) 1981, 1982, 1983 Infocom, Inc.\n"
    "All rights reserved.\n\n"
    "West of House\n"
    "You are standing in an open field west of a white house, "
    "with a boarded front door.\n"
    "There is a small mailbox here.\n\n";

static bool s_demoOutputSent = false;
static bool s_waitingInput = false;

size_t fizmo_output_available(void)
{
    if (!s_demoOutputSent) return strlen(s_demoText);
    return 0;
}

size_t fizmo_output_read(uint32_t *buffer, size_t max_chars)
{
    if (s_demoOutputSent) return 0;
    s_demoOutputSent = true;
    size_t len = strlen(s_demoText);
    if (len > max_chars) len = max_chars;
    for (size_t i = 0; i < len; i++) {
        buffer[i] = (uint32_t)s_demoText[i];
    }
    s_waitingInput = true;
    return len;
}

bool fizmo_waiting_for_input(void) { return s_waitingInput; }
bool fizmo_waiting_for_char(void) { return false; }
bool fizmo_has_exited(void) { return false; }

bool fizmo_get_status_line(char *room, size_t room_size,
                           char *score_or_time, size_t score_size)
{
    strncpy(room, "West of House", room_size - 1);
    room[room_size - 1] = '\0';
    strncpy(score_or_time, "Score: 0  Moves: 0", score_size - 1);
    score_or_time[score_size - 1] = '\0';
    return true;
}

void fizmo_submit_line(const char *line)
{
    (void)line;
    s_waitingInput = true;
}

void fizmo_submit_char(uint32_t ch) { (void)ch; }

int fizmo_bridge_init(const char *story_path)
{
    (void)story_path;
    return 0;
}

void fizmo_start_interpreter(void) {}
void fizmo_bridge_shutdown(void) {}
