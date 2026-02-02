/*
 * FizmoTextBuffer.c
 *
 * Toolkit-agnostic text buffer management.
 * Pure C, no framework dependencies.
 */

#include "FizmoTextBuffer.h"
#include <string.h>
#include <stdlib.h>

/*
 * Count newline-delimited lines in a text buffer.
 */
static int count_lines(const char *text, int length)
{
    int lines = 0;
    for (int i = 0; i < length; i++) {
        if (text[i] == '\n') {
            lines++;
        }
    }
    if (length > 0 && text[length - 1] != '\n') {
        lines++;
    }
    return lines;
}

/*
 * Trim oldest lines from the output buffer, keeping at least
 * MIN_SCROLLBACK_LINES or the entire current story response.
 */
static void trim_output(FizmoTextBuffer *buf)
{
    if (buf->outputLength == 0) return;

    int current_output_length = buf->outputLength - buf->currentOutputStart;
    int current_output_lines = count_lines(buf->output + buf->currentOutputStart,
                                           current_output_length);

    int keep_lines = (current_output_lines > FIZMO_MIN_SCROLLBACK_LINES)
                     ? current_output_lines
                     : FIZMO_MIN_SCROLLBACK_LINES;

    int total_lines = count_lines(buf->output, buf->outputLength);
    if (total_lines <= keep_lines) return;

    int lines_to_trim = total_lines - keep_lines;
    int trim_pos = 0;
    int lines_found = 0;

    for (int i = 0; i < buf->outputLength && lines_found < lines_to_trim; i++) {
        if (buf->output[i] == '\n') {
            lines_found++;
            if (lines_found == lines_to_trim) {
                trim_pos = i + 1;
                break;
            }
        }
    }

    if (trim_pos > 0 && trim_pos < buf->outputLength) {
        int new_length = buf->outputLength - trim_pos;
        memmove(buf->output, buf->output + trim_pos, new_length);
        buf->output[new_length] = '\0';
        buf->outputLength = new_length;

        int new_current_start = (buf->outputLength >= current_output_length)
                                ? (buf->outputLength - current_output_length)
                                : 0;
        buf->currentOutputStart = new_current_start;
    }
}

void fizmo_text_buffer_init(FizmoTextBuffer *buf)
{
    buf->output[0] = '\0';
    buf->outputLength = 0;
    buf->currentOutputStart = 0;
    buf->statusRoom[0] = '\0';
    buf->statusScore[0] = '\0';
    buf->command[0] = '\0';
    buf->commandLength = 0;
}

int fizmo_text_buffer_append_output(FizmoTextBuffer *buf, const char *text)
{
    if (text == NULL || text[0] == '\0') return 0;

    int textLen = (int)strlen(text);
    int available = FIZMO_OUTPUT_BUFFER_SIZE - buf->outputLength - 1;

    if (textLen > available) {
        /* Buffer full — discard oldest half on a newline boundary */
        int discardTarget = FIZMO_OUTPUT_BUFFER_SIZE / 2;
        int discardAt = discardTarget;

        for (int i = discardTarget;
             i < buf->outputLength && i < discardTarget + 200; i++) {
            if (buf->output[i] == '\n') {
                discardAt = i + 1;
                break;
            }
        }

        int keepLen = buf->outputLength - discardAt;
        if (keepLen > 0) {
            memmove(buf->output, buf->output + discardAt, keepLen);
            buf->outputLength = keepLen;
        } else {
            buf->outputLength = 0;
        }
        buf->output[buf->outputLength] = '\0';
        available = FIZMO_OUTPUT_BUFFER_SIZE - buf->outputLength - 1;
    }

    int toCopy = (textLen < available) ? textLen : available;
    memcpy(buf->output + buf->outputLength, text, toCopy);
    buf->outputLength += toCopy;
    buf->output[buf->outputLength] = '\0';

    trim_output(buf);
    return 1;
}

void fizmo_text_buffer_clear_output(FizmoTextBuffer *buf)
{
    buf->outputLength = 0;
    buf->output[0] = '\0';
    buf->currentOutputStart = 0;
}

const char *fizmo_text_buffer_get_output(const FizmoTextBuffer *buf)
{
    return buf->output;
}

int fizmo_text_buffer_set_status(FizmoTextBuffer *buf,
                                 const char *room, const char *score)
{
    int changed = 0;

    if (strcmp(room, buf->statusRoom) != 0) {
        strncpy(buf->statusRoom, room, FIZMO_STATUS_ROOM_SIZE - 1);
        buf->statusRoom[FIZMO_STATUS_ROOM_SIZE - 1] = '\0';
        changed = 1;
    }
    if (strcmp(score, buf->statusScore) != 0) {
        strncpy(buf->statusScore, score, FIZMO_STATUS_SCORE_SIZE - 1);
        buf->statusScore[FIZMO_STATUS_SCORE_SIZE - 1] = '\0';
        changed = 1;
    }
    return changed;
}

const char *fizmo_text_buffer_get_status_room(const FizmoTextBuffer *buf)
{
    return buf->statusRoom;
}

const char *fizmo_text_buffer_get_status_score(const FizmoTextBuffer *buf)
{
    return buf->statusScore;
}

void fizmo_text_buffer_append_command_char(FizmoTextBuffer *buf,
                                           const char *ch, int len)
{
    int available = FIZMO_COMMAND_BUFFER_SIZE - buf->commandLength - 1;
    if (len > available) len = available;
    if (len <= 0) return;

    memcpy(buf->command + buf->commandLength, ch, len);
    buf->commandLength += len;
    buf->command[buf->commandLength] = '\0';
}

void fizmo_text_buffer_command_backspace(FizmoTextBuffer *buf)
{
    if (buf->commandLength > 0) {
        buf->commandLength--;
        buf->command[buf->commandLength] = '\0';
    }
}

void fizmo_text_buffer_clear_command(FizmoTextBuffer *buf)
{
    buf->commandLength = 0;
    buf->command[0] = '\0';
}

const char *fizmo_text_buffer_get_command(const FizmoTextBuffer *buf)
{
    return buf->command;
}

void fizmo_text_buffer_mark_output_start(FizmoTextBuffer *buf)
{
    buf->currentOutputStart = buf->outputLength;
}

int fizmo_utf32_to_utf8(char *dst, size_t dst_size,
                        const uint32_t *src, size_t count)
{
    int len = 0;
    int max = (int)dst_size - 1;  /* Reserve space for null terminator */

    for (size_t i = 0; i < count && len < max - 3; i++) {
        uint32_t ch = src[i];

        if (ch < 0x80) {
            dst[len++] = (char)ch;
        } else if (ch < 0x800) {
            dst[len++] = (char)(0xC0 | (ch >> 6));
            dst[len++] = (char)(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            dst[len++] = (char)(0xE0 | (ch >> 12));
            dst[len++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            dst[len++] = (char)(0x80 | (ch & 0x3F));
        } else {
            dst[len++] = (char)(0xF0 | (ch >> 18));
            dst[len++] = (char)(0x80 | ((ch >> 12) & 0x3F));
            dst[len++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            dst[len++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    dst[len] = '\0';
    return len;
}
