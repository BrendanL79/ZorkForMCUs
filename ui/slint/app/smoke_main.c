/*
 * smoke_main.c - Headless de-risk test for libfizmo under MSVC.
 * Starts the interpreter, polls a little output, prints it, and exits.
 * Success = the Zork opening text appears on stdout.
 */
#include "fizmo_bridge.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <process.h>  /* _exit */
#include <windows.h>

int main(void) {
    /* Unbuffered stdout so captured output is never lost if we exit abruptly. */
    setvbuf(stdout, NULL, _IONBF, 0);

    const char *story = ZORK_STORY_PATH;
    const char *env = getenv("ZORK_STORY_PATH");
    if (env && env[0]) story = env;

    if (fizmo_bridge_init(story) != 0) {
        fprintf(stderr, "fizmo_bridge_init failed for %s\n", story);
        return 1;
    }
    if (fizmo_start_interpreter() != 0) {
        fprintf(stderr, "fizmo_start_interpreter failed\n");
        return 1;
    }

    uint32_t buf[512];
    int printed = 0;
    for (int i = 0; i < 300; i++) {
        size_t n = fizmo_output_read(buf, 512);
        for (size_t k = 0; k < n; k++) {
            putchar(buf[k] < 128 ? (int)buf[k] : '?');
            printed = 1;
        }
        if (printed && fizmo_waiting_for_input()) {
            /* Drain any output produced alongside the first input prompt. */
            Sleep(20);
            n = fizmo_output_read(buf, 512);
            for (size_t k = 0; k < n; k++)
                putchar(buf[k] < 128 ? (int)buf[k] : '?');
            break;
        }
        Sleep(10);
    }

    fflush(stdout);

    /*
     * Note: we intentionally do NOT call fizmo_bridge_shutdown() here. The fizmo
     * thread is blocked inside the interpreter's input loop (read_line); the
     * desktop bridge cannot unwind that loop cleanly without feeding it a quit
     * command, so shutdown()'s thread join would hang. For a headless smoke test
     * that has already proven the interpreter runs and produced output, exit the
     * process directly and let the OS reap the worker thread.
     */
    _exit(printed ? 0 : 2);
}
