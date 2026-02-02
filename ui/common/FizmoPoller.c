/*
 * FizmoPoller.c
 *
 * Toolkit-agnostic polling logic for the fizmo interpreter.
 * Pure C, no framework dependencies.
 */

#include "FizmoPoller.h"
#include <string.h>

/*
 * Include the appropriate bridge header based on build mode.
 * The desktop stub also provides these same function signatures.
 */
#if defined(BUILD_WITH_FREERTOS)
#include "fizmo_rtos_bridge.h"
#elif defined(USE_FIZMO_BRIDGE)
#include "fizmo_bridge.h"
#else
/* Desktop stub — functions are linked from fizmo_stub.c */
extern size_t fizmo_output_available(void);
extern size_t fizmo_output_read(uint32_t *buffer, size_t max_chars);
extern bool fizmo_waiting_for_input(void);
extern bool fizmo_waiting_for_char(void);
extern bool fizmo_has_exited(void);
extern bool fizmo_get_status_line(char *room, size_t room_size,
                                   char *score_or_time, size_t score_size);
extern void fizmo_submit_line(const char *line);
extern void fizmo_submit_char(uint32_t ch);
#endif

/* Temporary buffer for reading UTF-32 output from fizmo */
#define READ_BUFFER_SIZE 256

void fizmo_poller_init(FizmoPollerState *state)
{
    state->waitingForInput = false;
    state->waitingForChar = false;
    state->gameExited = false;
}

int fizmo_poller_poll(FizmoPollerState *state, FizmoTextBuffer *buf)
{
    int changed = 0;
    uint32_t readBuf[READ_BUFFER_SIZE];

    /* Drain output queue */
    size_t available = fizmo_output_available();
    while (available > 0) {
        size_t toRead = (available < READ_BUFFER_SIZE) ? available : READ_BUFFER_SIZE;
        size_t nread = fizmo_output_read(readBuf, toRead);

        if (nread > 0) {
            char utf8[READ_BUFFER_SIZE * 4 + 1];
            fizmo_utf32_to_utf8(utf8, sizeof(utf8), readBuf, nread);
            if (fizmo_text_buffer_append_output(buf, utf8)) {
                changed |= FIZMO_CHANGED_OUTPUT;
            }
        }
        available = fizmo_output_available();
    }

    /* Check input state */
    bool waiting = fizmo_waiting_for_input();
    if (waiting != state->waitingForInput) {
        state->waitingForInput = waiting;
        changed |= FIZMO_CHANGED_INPUT;
    }

    bool waitingCh = fizmo_waiting_for_char();
    if (waitingCh != state->waitingForChar) {
        state->waitingForChar = waitingCh;
        changed |= FIZMO_CHANGED_CHAR;
    }

    /* Update status line */
    char room[FIZMO_STATUS_ROOM_SIZE];
    char score[FIZMO_STATUS_SCORE_SIZE];
    if (fizmo_get_status_line(room, sizeof(room), score, sizeof(score))) {
        if (fizmo_text_buffer_set_status(buf, room, score)) {
            changed |= FIZMO_CHANGED_STATUS;
        }
    }

    /* Check if game exited */
    if (!state->gameExited && fizmo_has_exited()) {
        state->gameExited = true;
        changed |= FIZMO_CHANGED_EXITED;
    }

    return changed;
}

void fizmo_poller_submit_line(FizmoPollerState *state,
                              FizmoTextBuffer *buf,
                              const char *line,
                              bool is_desktop)
{
    (void)state;

    fizmo_text_buffer_mark_output_start(buf);

    int len = (int)strlen(line);
    char echo[FIZMO_COMMAND_BUFFER_SIZE + 4];
    int pos = 0;

    if (is_desktop) {
        /* Desktop: fizmo already printed ">", just add space + text */
        echo[pos++] = ' ';
    } else {
        /* Hardware: we need the full prompt */
        echo[pos++] = '>';
        echo[pos++] = ' ';
    }

    if (len > 0 && pos + len < (int)sizeof(echo) - 2) {
        memcpy(echo + pos, line, len);
        pos += len;
    }
    echo[pos++] = '\n';
    echo[pos] = '\0';

    fizmo_text_buffer_append_output(buf, echo);
    fizmo_submit_line(line);
}

void fizmo_poller_submit_char(FizmoPollerState *state, uint32_t ch)
{
    (void)state;
    fizmo_submit_char(ch);
}
