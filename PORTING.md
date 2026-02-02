# Porting ZorkForMCUs to a New UI Toolkit

This guide explains how to add a new UI frontend (LVGL, Slint, TouchGFX,
etc.) to ZorkForMCUs. The interpreter, text buffering, polling, and platform
bridging are already implemented in shared modules — your toolkit only needs
to provide the visual layer on top.

## Prerequisites

Familiarize yourself with the shared code:

| Module | Location | Purpose |
|--------|----------|---------|
| `FizmoTextBuffer` | `ui/common/FizmoTextBuffer.h/.c` | Output buffer, status line, command buffer, UTF-32→UTF-8 |
| `FizmoPoller` | `ui/common/FizmoPoller.h/.c` | Polls interpreter, returns change flags |
| `DisplayConfig` | `ui/common/DisplayConfig.h` | Screen dimensions and UI sizing per board |
| `fizmo_stub.c` | `ui/common/fizmo_stub.c` | Desktop stub for UI testing without interpreter |
| `FizmoCommon.cmake` | `cmake/FizmoCommon.cmake` | Shared CMake source lists, definitions, helpers |

Your new UI lives under `ui/<toolkit>/ZorkUI/` (e.g. `ui/lvgl/ZorkUI/`).

---

## Step 1: Create Widgets and Render Text

Create the four UI regions that make up the Zork interface:

### Status Bar (top)

Two text labels in a horizontal row:

- **Left:** Room name from `fizmo_text_buffer_get_status_room(&buf)`
- **Right:** Score/moves from `fizmo_text_buffer_get_status_score(&buf)`

Use the constants from `DisplayConfig.h` for sizing:

```c
#include "DisplayConfig.h"
// DISPLAY_STATUS_HEIGHT — status bar height in pixels
// DISPLAY_MARGIN         — padding around elements
```

### Output Text Area (middle, scrollable)

A scrollable container with a single text element showing the game's output.
Read the text with:

```c
const char *text = fizmo_text_buffer_get_output(&buf);
// Set this as the content of your text/label widget.
// The string is null-terminated UTF-8.
```

When the text changes (see Step 3), update the widget and auto-scroll to the
bottom so the player always sees the latest output.

### Input Area (bottom)

A single-line text input field, visible only when the interpreter is waiting
for input (`state.waitingForInput == true`). Prefix the display with `"> "`
to match the classic Zork prompt look.

For touchscreen devices without a physical keyboard, you also need an
on-screen keyboard (see below).

### Compass Rose (optional, overlaid on output area)

An interactive 8-direction touch widget for navigation commands. When the
player taps a direction, submit the corresponding command (`"n"`, `"se"`,
etc.) via `fizmo_poller_submit_line()`.

The math to convert a touch point to a cardinal direction:

```c
// Given touch at (x, y) relative to compass center:
double angle = atan2(-y, x);  // screen Y is inverted
// Map angle to 8 sectors of 45° each → "n","ne","e","se","s","sw","w","nw"
```

### On-Screen Keyboard

If your toolkit has a built-in keyboard widget, attach it to the input text
field. If not, build a minimal one — Zork only needs lowercase letters, digits,
space, backspace, and enter.

For UIs that build commands character-by-character (e.g. custom keyboards on
constrained displays), use the command buffer helpers:

```c
// When a key is pressed:
fizmo_text_buffer_append_command_char(&buf, "a", 1);

// When backspace is pressed:
fizmo_text_buffer_command_backspace(&buf);

// Display the in-progress command:
const char *cmd = fizmo_text_buffer_get_command(&buf);

// When enter is pressed, submit the accumulated command:
fizmo_poller_submit_line(&state, &buf, cmd, is_desktop);
fizmo_text_buffer_clear_command(&buf);
```

### Color Scheme

The reference UI uses this palette — match it or choose your own:

| Element | Background | Text |
|---------|-----------|------|
| Status bar | `#16213e` | `#e8e8e8` |
| Output area | `#1a1a2e` | `#00ff88` |
| Input area | `#0f3460` | `#e8e8e8` |

### Font

Use `DISPLAY_FONT_SIZE` from `DisplayConfig.h`. The reference UI uses a
monospace or clean sans-serif TTF rendered at runtime. For flash-constrained
boards (RT1050), consider pre-rendered bitmap fonts.

---

## Step 2: Set Up a Periodic Timer

The interpreter runs in a separate thread (or FreeRTOS task). Your UI needs
to poll it periodically to pick up new output, status changes, and state
transitions.

Create a repeating timer at **50ms** (20 Hz) using your toolkit's timer API
and call `fizmo_poller_poll()` from the callback:

```c
#include "FizmoPoller.h"
#include "FizmoTextBuffer.h"

static FizmoTextBuffer  g_buf;
static FizmoPollerState g_state;

void my_init(void) {
    fizmo_text_buffer_init(&g_buf);
    fizmo_poller_init(&g_state);

    // Create a 50ms repeating timer using your toolkit's API.
    // Example (LVGL):
    //   lv_timer_create(poll_callback, 50, NULL);
    // Example (Qt):
    //   timer.setInterval(50); timer.onTimeout(poll_callback);
}
```

50ms is a good balance between responsiveness and CPU overhead. You can go
faster (e.g. 16ms for 60fps toolkits) but there's no benefit below ~30ms
since the interpreter produces output in bursts.

---

## Step 3: Map Poll Results to Widget Updates

`fizmo_poller_poll()` returns a bitmask telling you exactly what changed.
Use it to update only the widgets that need it:

```c
void poll_callback(void) {
    int changed = fizmo_poller_poll(&g_state, &g_buf);

    if (changed & FIZMO_CHANGED_OUTPUT) {
        // Game produced new text — update the output label
        const char *text = fizmo_text_buffer_get_output(&g_buf);
        set_output_text(text);   // your widget update function
        scroll_to_bottom();      // auto-scroll
    }

    if (changed & FIZMO_CHANGED_STATUS) {
        // Status line changed — update room and score labels
        set_status_room(fizmo_text_buffer_get_status_room(&g_buf));
        set_status_score(fizmo_text_buffer_get_status_score(&g_buf));
    }

    if (changed & FIZMO_CHANGED_INPUT) {
        // Input state changed — show or hide the input bar and keyboard
        set_input_visible(g_state.waitingForInput);
    }

    if (changed & FIZMO_CHANGED_CHAR) {
        // Interpreter wants a single keypress (e.g. [MORE] prompt)
        // Enable raw key capture mode
        set_char_mode(g_state.waitingForChar);
    }

    if (changed & FIZMO_CHANGED_EXITED) {
        // Game over — show a message, disable input
        show_game_over();
    }
}
```

The flags are:

| Flag | Meaning |
|------|---------|
| `FIZMO_CHANGED_OUTPUT` | New game text appended to output buffer |
| `FIZMO_CHANGED_STATUS` | Room name or score/moves changed |
| `FIZMO_CHANGED_INPUT` | `waitingForInput` toggled (show/hide input bar) |
| `FIZMO_CHANGED_CHAR` | `waitingForChar` toggled (single-key mode) |
| `FIZMO_CHANGED_EXITED` | Game has ended |

---

## Step 4: Handle Input

### Line Input (normal commands)

When the player types a command and presses enter:

```c
void on_input_submitted(const char *text) {
    // is_desktop: true for desktop builds (fizmo already printed ">"),
    //             false for hardware builds (we add the "> " prefix).
    fizmo_poller_submit_line(&g_state, &g_buf, text, is_desktop);

    // The output buffer now contains the echoed command.
    // Your next poll will pick up FIZMO_CHANGED_OUTPUT.
}
```

The `is_desktop` parameter controls echo formatting. On desktop builds,
fizmo itself prints `"> "` before reading input, so `submit_line` only
adds a space before the echoed text. On hardware (FreeRTOS) builds, fizmo
doesn't print the prompt, so `submit_line` prepends `"> "`.

Set `is_desktop` based on your build configuration:

```c
#if defined(USE_FIZMO_BRIDGE) || defined(DESKTOP_STUB)
static const bool is_desktop = true;
#else
static const bool is_desktop = false;
#endif
```

### Single Character Input ([MORE] prompts, yes/no)

When `g_state.waitingForChar` is true, the interpreter wants a single
keypress. Capture the next key event and submit it:

```c
void on_key_pressed(uint32_t keycode) {
    if (g_state.waitingForChar) {
        fizmo_poller_submit_char(&g_state, keycode);
    }
}
```

### Compass Rose Input

When the player taps a direction on the compass rose, submit it as a
regular line command:

```c
fizmo_poller_submit_line(&g_state, &g_buf, "north", is_desktop);
```

---

## Step 5: Provide Platform Display and Touch Drivers

This step is toolkit-specific and depends on your target hardware.

### Desktop (SDL)

Most embedded UI toolkits have SDL backends for desktop development:

- **LVGL:** Built-in SDL driver via `lv_sdl_window_create()`
- **Slint:** Native desktop backend
- **TouchGFX:** Simulator

Use the desktop build to iterate on the UI before moving to hardware.

### NXP RT1170-EVKB (720x1280 portrait)

- **Display:** Configure your toolkit's display flush callback to write
  to the LCDIF framebuffer. The NXP SDK provides the low-level LCD init.
- **Touch:** Register a pointer/touch input device using the FT5406 I2C
  touch controller driver from the NXP SDK.
- **Fonts:** Runtime TTF rendering (24px) — the board has enough flash.

### NXP RT1050-EVK (480x272 landscape)

- **Display:** Same approach as RT1170 but for the smaller LCD.
- **Touch:** Same FT5406 driver.
- **Memory:** This board is constrained (~512KB flash, limited SRAM).
  - Use partial/band rendering if your toolkit supports it
  - Use pre-rendered bitmap fonts instead of runtime TTF
  - Tune your toolkit's memory pool (e.g. `LV_MEM_SIZE` for LVGL)
- **Fonts:** 14px bitmap font — pre-generate ASCII glyphs at build time.

### Threading Model

The interpreter and UI run in separate execution contexts:

```
FreeRTOS:
  Task 1 (high priority):  UI event loop + 50ms poll timer
  Task 2 (low priority):   fizmo interpreter (blocks on input)

Desktop:
  Main thread:    UI event loop + 50ms poll timer
  Worker thread:  fizmo interpreter (std::thread, blocks on input)
```

The `fizmo_poller_*` functions handle the thread-safe communication.
You do not need to add any locking — the underlying bridge layer
(`fizmo_rtos_bridge.c` or `fizmo_bridge.cpp`) uses FreeRTOS
queues/semaphores or `std::mutex`/`std::condition_variable`.

---

## Build System Setup

Your `CMakeLists.txt` should include the shared CMake module and use its
helpers:

```cmake
cmake_minimum_required(VERSION 3.21.1)
project(ZorkUI LANGUAGES C CXX ASM)

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${PROJECT_ROOT}/cmake/FizmoCommon.cmake")

# ... find your toolkit (find_package, add_subdirectory, etc.) ...

add_executable(ZorkUI
    your_backend.cpp
    your_screen.cpp
    ${FIZMO_COMMON_SOURCES}        # FizmoTextBuffer.c, FizmoPoller.c
    # Pick ONE of the following:
    # ${FIZMO_DESKTOP_BRIDGE_SOURCES}  # Desktop with real interpreter
    # ${FIZMO_STUB_SOURCES}            # Desktop stub (UI testing)
    # ${FIZMO_RTOS_SOURCES}            # FreeRTOS embedded
)

# Apply shared include dirs, compile definitions, compat header
fizmo_common_apply(ZorkUI)

# Add libfizmo interpreter (not needed for stub builds)
target_sources(ZorkUI PRIVATE
    ${FIZMO_INTERPRETER_SOURCES}
    ${FIZMO_TOOLS_SOURCES}
    # ${FIZMO_TOOLS_DESKTOP_EXTRAS}  # Add for desktop builds (filesys_c.c)
    # ${FIZMO_LOCALE_STUBS}          # Add for desktop builds
)

# Set display profile
fizmo_apply_display_profile(ZorkUI "${DISPLAY_PROFILE}")

# ... link your toolkit, threading, etc. ...
```

### Available CMake Variables

| Variable | Contents |
|----------|---------|
| `FIZMO_COMMON_DIR` | Path to `ui/common/` |
| `FIZMO_COMMON_SOURCES` | `FizmoTextBuffer.c`, `FizmoPoller.c` |
| `FIZMO_STUB_SOURCES` | `fizmo_stub.c` |
| `FIZMO_INTERPRETER_SOURCES` | libfizmo interpreter .c files |
| `FIZMO_TOOLS_SOURCES` | libfizmo tools .c files |
| `FIZMO_TOOLS_DESKTOP_EXTRAS` | `filesys_c.c` (desktop only) |
| `FIZMO_LOCALE_STUBS` | `fizmo_locale_stubs.c` |
| `FIZMO_RTOS_SOURCES` | RTOS bridge, filesystem, story data |
| `FIZMO_DESKTOP_BRIDGE_SOURCES` | `fizmo_bridge.cpp` |
| `FIZMO_COMPILE_DEFINITIONS` | Feature-disable flags for libfizmo |

### Available CMake Functions

| Function | What it does |
|----------|-------------|
| `fizmo_common_apply(TARGET)` | Adds include dirs (`src/`, `ui/common/`, `libfizmo/src/`), compile definitions, and force-includes the embedded compat header |
| `fizmo_apply_display_profile(TARGET PROFILE)` | Sets `DISPLAY_RT1050`, `DISPLAY_RT1170`, or `DISPLAY_RT1170_SCALED` compile definition |

---

## Reference Implementation

The Qt for MCUs port (`ui/qul/ZorkUI/`) serves as a working reference.
Study `FizmoBackend.cpp` to see exactly how the common modules are called.
The Qt-specific parts (which you replace with your toolkit's equivalents) are:

- `Qul::Property<int>` version counters → your toolkit's reactive/dirty mechanism
- `Qul::Timer` → your toolkit's timer API
- `Qul::EventQueue` → only needed if your toolkit requires event marshalling
- `Qul::Private::String` → your toolkit's string type (or plain `const char*`)

Everything else — buffer management, polling, UTF conversion, echo formatting,
status tracking — comes from `ui/common/` and works identically across toolkits.
