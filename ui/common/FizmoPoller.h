/*
 * FizmoPoller.h
 *
 * Toolkit-agnostic polling logic for the fizmo interpreter.
 * Reads output from fizmo, converts UTF-32 to UTF-8, updates
 * text buffers, and tracks input/game state.
 *
 * Call fizmo_poller_poll() periodically (e.g. every 50ms) from
 * whatever timer mechanism your UI toolkit provides.
 *
 * Shared by all UI implementations.
 */

#ifndef FIZMO_POLLER_H
#define FIZMO_POLLER_H

#include "FizmoTextBuffer.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Flags indicating what changed during a poll cycle.
 * The UI layer can inspect these to decide what to repaint.
 */
#define FIZMO_CHANGED_OUTPUT    (1 << 0)
#define FIZMO_CHANGED_STATUS    (1 << 1)
#define FIZMO_CHANGED_INPUT     (1 << 2)  /* waitingForInput changed */
#define FIZMO_CHANGED_CHAR      (1 << 3)  /* waitingForChar changed */
#define FIZMO_CHANGED_EXITED    (1 << 4)  /* game exited */

/*
 * FizmoPollerState — tracks interpreter state between polls.
 */
typedef struct {
    bool waitingForInput;
    bool waitingForChar;
    bool gameExited;
} FizmoPollerState;

/*
 * Initialize poller state.
 */
void fizmo_poller_init(FizmoPollerState *state);

/*
 * Poll the fizmo interpreter for new output, status, and state changes.
 * Drains the output queue, converts UTF-32→UTF-8, appends to buf,
 * and updates state flags.
 *
 * Returns a bitmask of FIZMO_CHANGED_* flags indicating what changed.
 */
int fizmo_poller_poll(FizmoPollerState *state, FizmoTextBuffer *buf);

/*
 * Submit a line of input to fizmo.
 * Echoes the command to the output buffer with appropriate prefix.
 * Clears the command buffer.
 *
 * line: null-terminated UTF-8 string.
 * is_desktop: if true, prefix with " " (fizmo already printed ">"),
 *             if false, prefix with "> " (hardware builds).
 */
void fizmo_poller_submit_line(FizmoPollerState *state,
                              FizmoTextBuffer *buf,
                              const char *line,
                              bool is_desktop);

/*
 * Submit a single character to fizmo.
 */
void fizmo_poller_submit_char(FizmoPollerState *state, uint32_t ch);

#ifdef __cplusplus
}
#endif

#endif /* FIZMO_POLLER_H */
