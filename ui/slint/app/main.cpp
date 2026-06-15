#include "zork.h"   // generated from app/zork.slint
#include "fizmo_slint_bridge.h"
#include "fizmo_bridge.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

int main() {
    const char *story_path = ZORK_STORY_PATH;
    if (const char *env = std::getenv("ZORK_STORY_PATH"); env && env[0]) {
        story_path = env;
    }

    if (fizmo_bridge_init(story_path) != 0) {
        std::fprintf(stderr, "Failed to init fizmo bridge with story: %s\n", story_path);
        return 1;
    }
    if (fizmo_start_interpreter() != 0) {
        std::fprintf(stderr, "Failed to start fizmo interpreter\n");
        return 1;
    }

    auto ui = ZorkWindow::create();

    // Register CascadiaCode so the "Cascadia Code" font-family resolves even on
    // machines where the font is not system-installed.  SLINT_FONT_PATH is set by
    // CMakeLists.txt; it points to the system font on the dev machine and can be
    // overridden at configure time for other environments.  Failure is non-fatal —
    // Slint will fall back to whatever monospace font the system has.
#ifdef SLINT_FONT_PATH
    if (auto err = ui->window().window_handle().register_font_from_path(
                slint::SharedString(SLINT_FONT_PATH))) {
        std::fprintf(stderr, "font load warning: %s\n", err->data());
    }
#endif

    auto transcript = std::make_shared<std::string>();

    // Echo the typed command into the transcript and send it to the interpreter.
    ui->on_submit([ui, transcript](const slint::SharedString &cmd) {
        std::string line(cmd);
        *transcript += "\n>";
        *transcript += line;
        *transcript += "\n";
        ui->set_transcript(slint::SharedString(*transcript));
        fizmo_submit_line(line.c_str());
    });

    // Poll the bridge on the UI thread and append new output to the transcript.
    slint::Timer poll_timer;
    poll_timer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(30),
        [ui, transcript]() {
            std::string chunk = zork_drain_output();
            if (!chunk.empty()) {
                *transcript += chunk;
                ui->set_transcript(slint::SharedString(*transcript));
            }
        });

    ui->run();

    // fizmo_bridge_shutdown() deadlocks (worker thread blocked in read_line),
    // so exit hard once the window closes. The OS reclaims everything.
    std::_Exit(0);
}
