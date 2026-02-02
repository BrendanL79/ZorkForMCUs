/*
 * FizmoTextBuffer.h
 *
 * Toolkit-agnostic text buffer management for the fizmo interpreter UI.
 * Handles output accumulation, trimming, UTF-32 to UTF-8 conversion,
 * status line storage, and command input buffering.
 *
 * Shared by all UI implementations (Qt for MCUs, LVGL, Slint, etc.).
 */

#ifndef FIZMO_TEXT_BUFFER_H
#define FIZMO_TEXT_BUFFER_H

#include "DisplayConfig.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Output buffer sizes — tuned per display profile.
 * RT1050: small display, keep buffer small to save RAM.
 * Others: more scrollback for larger displays.
 */
#if defined(DISPLAY_RT1050)
#define FIZMO_OUTPUT_BUFFER_SIZE  4096
#define FIZMO_MIN_SCROLLBACK_LINES 10
#else
#define FIZMO_OUTPUT_BUFFER_SIZE  16384
#define FIZMO_MIN_SCROLLBACK_LINES 20
#endif

#define FIZMO_STATUS_ROOM_SIZE    64
#define FIZMO_STATUS_SCORE_SIZE   32
#define FIZMO_COMMAND_BUFFER_SIZE  256

/*
 * FizmoTextBuffer — holds all text state for the UI.
 *
 * Designed to be embedded in any UI backend (Qt singleton, LVGL app data,
 * Slint backend, etc.) as a plain C struct.
 */
typedef struct {
    /* Main output text (accumulated game output, UTF-8) */
    char output[FIZMO_OUTPUT_BUFFER_SIZE];
    int  outputLength;
    int  currentOutputStart;  /* Marks where current story response began */

    /* Status line */
    char statusRoom[FIZMO_STATUS_ROOM_SIZE];
    char statusScore[FIZMO_STATUS_SCORE_SIZE];

    /* Command input buffer (built character-by-character on constrained UIs) */
    char command[FIZMO_COMMAND_BUFFER_SIZE];
    int  commandLength;
} FizmoTextBuffer;

/*
 * Initialize all buffers to empty.
 */
void fizmo_text_buffer_init(FizmoTextBuffer *buf);

/*
 * Append UTF-8 text to the output buffer.
 * Automatically trims old content when the buffer is full.
 * Returns 1 if text was appended (buffer changed), 0 if text was empty/null.
 */
int fizmo_text_buffer_append_output(FizmoTextBuffer *buf, const char *text);

/*
 * Clear the output buffer.
 */
void fizmo_text_buffer_clear_output(FizmoTextBuffer *buf);

/*
 * Get pointer to the output text (null-terminated UTF-8).
 */
const char *fizmo_text_buffer_get_output(const FizmoTextBuffer *buf);

/*
 * Update the status line buffers.
 * Returns 1 if either field changed, 0 if unchanged.
 */
int fizmo_text_buffer_set_status(FizmoTextBuffer *buf,
                                 const char *room, const char *score);

const char *fizmo_text_buffer_get_status_room(const FizmoTextBuffer *buf);
const char *fizmo_text_buffer_get_status_score(const FizmoTextBuffer *buf);

/*
 * Command buffer helpers (for on-screen keyboard UIs).
 */
void fizmo_text_buffer_append_command_char(FizmoTextBuffer *buf,
                                           const char *ch, int len);
void fizmo_text_buffer_command_backspace(FizmoTextBuffer *buf);
void fizmo_text_buffer_clear_command(FizmoTextBuffer *buf);
const char *fizmo_text_buffer_get_command(const FizmoTextBuffer *buf);

/*
 * Mark the start of a new story response (for scrollback trimming).
 */
void fizmo_text_buffer_mark_output_start(FizmoTextBuffer *buf);

/*
 * UTF-32 to UTF-8 conversion.
 * Converts an array of UTF-32 code points to a null-terminated UTF-8 string.
 * Returns the number of bytes written (excluding null terminator).
 * dst must be at least (count * 4 + 1) bytes.
 */
int fizmo_utf32_to_utf8(char *dst, size_t dst_size,
                        const uint32_t *src, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* FIZMO_TEXT_BUFFER_H */
