# LVGL GUI Integration Plan for ZorkForMCUs

> **Status:** Draft
> **Date:** 2026-02-08
> **Scope:** Add an LVGL-based GUI as an alternative to the existing Qt for MCUs UI
> **Motivation:** Provide a fully open-source UI stack (MIT-licensed) alongside the
> existing commercial Qt for MCUs option

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [LVGL Version Selection](#3-lvgl-version-selection)
4. [Directory Structure](#4-directory-structure)
5. [Build System Design](#5-build-system-design)
6. [Display & Input Driver Layer](#6-display--input-driver-layer)
7. [FreeRTOS Integration](#7-freertos-integration)
8. [UI Design & Widget Layout](#8-ui-design--widget-layout)
9. [Fizmo Bridge Reuse](#9-fizmo-bridge-reuse)
10. [Desktop Simulator Build](#10-desktop-simulator-build)
11. [Memory Budget](#11-memory-budget)
12. [Font Strategy](#12-font-strategy)
13. [Implementation Phases](#13-implementation-phases)
14. [Risk Register](#14-risk-register)
15. [Testing Strategy](#15-testing-strategy)
16. [References](#16-references)

---

## 1. Executive Summary

This plan adds an **LVGL-based GUI** to ZorkForMCUs as a parallel UI implementation
that **coexists** with the existing Qt for MCUs UI. The LVGL UI will live under
`ui/lvgl/` alongside `ui/qul/`, sharing the same `src/` platform layer (fizmo bridge,
FreeRTOS integration, story embedding, filesystem).

**Targets:**
- NXP MIMXRT1170-EVKB (720×1280, portrait, MIPI-DSI)
- NXP MIMXRT1050-EVK (480×272, landscape, parallel RGB)
- Desktop simulator (SDL2, any resolution)

**Goal:** Full feature parity with the Qt for MCUs UI — scrollable text output, status
bar, text input field, virtual on-screen keyboard, and compass-rose navigation widget.

---

## 2. Architecture Overview

### Current Architecture (Qt for MCUs)

```
┌─────────────────────────────────────────────────────┐
│  QML UI Layer (ZorkUI.qml, ZorkKeyboard.qml)        │
├─────────────────────────────────────────────────────┤
│  FizmoBackend (Qt C++ singleton, 50ms poll timer)   │
├─────────────────────────────────────────────────────┤
│  fizmo_rtos_bridge.c ◄──FreeRTOS queues/sems──►     │
│  fizmo_bridge.cpp    ◄──std::thread/mutex──►        │
├─────────────────────────────────────────────────────┤
│  libfizmo (Z-machine interpreter)                   │
├─────────────────────────────────────────────────────┤
│  FreeRTOS  │  NXP MCUXpresso SDK  │  Display HW     │
└─────────────────────────────────────────────────────┘
```

### Proposed Architecture (LVGL)

```
┌─────────────────────────────────────────────────────┐
│  LVGL UI Layer (C, zork_ui.c)                       │
│  ┌──────────┬────────────┬──────────┬────────────┐  │
│  │ Output   │ Status Bar │ Input    │ Compass    │  │
│  │ textarea │ (lv_label) │ textarea │ Rose       │  │
│  │ (r/o)    │            │ + kbd    │ (img+touch)│  │
│  └──────────┴────────────┴──────────┴────────────┘  │
├─────────────────────────────────────────────────────┤
│  Fizmo↔LVGL Adapter (fizmo_lvgl_adapter.c)          │
│  - lv_timer polling (replaces Qt poll timer)        │
│  - UTF-32→UTF-8 conversion                         │
│  - Scrollback trimming                              │
├─────────────────────────────────────────────────────┤
│  fizmo_rtos_bridge.c  (SHARED — unchanged)          │
│  fizmo_bridge.cpp     (SHARED — desktop builds)     │
├─────────────────────────────────────────────────────┤
│  libfizmo (Z-machine interpreter)                   │
├─────────────────────────────────────────────────────┤
│  LVGL core + drivers                                │
│  ┌───────────────────┬──────────────────────────┐   │
│  │ lv_port_disp.c    │ lv_port_indev.c          │   │
│  │ (flush_cb → LCD)  │ (touch → lv_indev)       │   │
│  └───────────────────┴──────────────────────────┘   │
├─────────────────────────────────────────────────────┤
│  FreeRTOS  │  NXP MCUXpresso SDK  │  Display HW     │
└─────────────────────────────────────────────────────┘
```

**Key design principle:** The fizmo bridge layer (`src/fizmo_rtos_bridge.c`,
`src/fizmo_bridge.cpp`) is UI-framework-agnostic. It exposes output via a FreeRTOS
queue and accepts input via a semaphore-gated buffer. The LVGL adapter reads from
the same queue and writes to the same input buffer — no changes needed in the bridge.

---

## 3. LVGL Version Selection

### Decision: LVGL v9.x (targeting v9.4.0+)

| Factor | v8.x | v9.x | Winner |
|--------|-------|------|--------|
| NXP MCUXpresso SDK support | Legacy SDK 2.x only | SDK 25.06+ ships v9 | **v9** |
| Official RT1170 board port | Community only | [lv_port_nxp_imxrt1170-evkb](https://github.com/lvgl/lv_port_nxp_imxrt1170-evkb) | **v9** |
| FreeRTOS integration | Manual mutex | Built-in `LV_OS_FREERTOS` | **v9** |
| GPU acceleration | `LV_USE_GPU_NXP_PXP` (legacy) | `LV_USE_DRAW_PXP` / VGLite draw units | **v9** |
| SDL simulator | Supported | Supported, improved | **v9** |
| Long-term maintenance | Bug-fix only | Active development | **v9** |
| Virtual keyboard | `lv_keyboard` | `lv_keyboard` (same API) | Tie |

The user's preference for v8.x "unless feature parity requires v9.x" is overridden
by the practical reality: NXP's current SDK ships v9, and the official board ports
target v9. Using v8 would mean fighting the toolchain. All features needed for parity
(textarea, keyboard, image widget, labels) exist in both versions.

**LVGL will be included as a git submodule** at `external/lvgl/`, pinned to a v9.4.x
release tag.

---

## 4. Directory Structure

```
ZorkForMCUs/
├── external/
│   ├── libfizmo/                 # (existing submodule)
│   └── lvgl/                     # NEW — LVGL v9.4.x submodule
│
├── src/                          # SHARED platform layer (unchanged)
│   ├── fizmo_rtos_bridge.c/h     #   reused by LVGL builds
│   ├── fizmo_bridge.cpp/h        #   reused by LVGL desktop builds
│   ├── fizmo_filesys_hybrid.c/h  #   reused
│   ├── story_data.S/h            #   reused
│   └── ...
│
├── ui/
│   ├── qul/ZorkUI/               # (existing Qt for MCUs UI — untouched)
│   │
│   └── lvgl/                     # NEW — LVGL UI
│       ├── CMakeLists.txt        #   top-level LVGL build
│       ├── lv_conf.h             #   LVGL configuration
│       │
│       ├── app/                  #   application UI code
│       │   ├── zork_ui.c/h       #     main UI: screen layout, widgets
│       │   ├── zork_output.c/h   #     output textarea management
│       │   ├── zork_input.c/h    #     input field + keyboard handling
│       │   ├── zork_status.c/h   #     status bar (room, score)
│       │   ├── zork_compass.c/h  #     compass rose widget
│       │   └── zork_theme.c/h    #     green-on-black terminal theme
│       │
│       ├── adapter/              #   fizmo↔LVGL bridge
│       │   └── fizmo_lvgl_adapter.c/h
│       │
│       ├── port/                 #   hardware-specific drivers
│       │   ├── rt1170/
│       │   │   ├── lv_port_disp.c/h
│       │   │   └── lv_port_indev.c/h
│       │   ├── rt1050/
│       │   │   ├── lv_port_disp.c/h
│       │   │   └── lv_port_indev.c/h
│       │   └── sdl/
│       │       ├── lv_port_disp.c/h
│       │       └── lv_port_indev.c/h
│       │
│       ├── main_freertos.c       #   FreeRTOS entry point (LVGL variant)
│       ├── main_desktop.c        #   Desktop/SDL entry point
│       │
│       ├── assets/
│       │   └── compass-rose.c    #   LVGL image descriptor (converted PNG)
│       │
│       └── fonts/
│           └── lv_font_zork_*.c  #   custom LVGL fonts (offline-converted)
```

---

## 5. Build System Design

### CMake Structure

The LVGL build will be a **standalone CMake project** at `ui/lvgl/CMakeLists.txt`,
following the same pattern as the Qt build at `ui/qul/ZorkUI/CMakeLists.txt`. It will
share sources from `src/` via relative paths.

```cmake
# ui/lvgl/CMakeLists.txt (simplified outline)
cmake_minimum_required(VERSION 3.20)
project(ZorkLVGL C CXX ASM)

# --- Options ---
option(BUILD_DESKTOP "Build SDL2 desktop simulator" OFF)
set(DISPLAY_PROFILE "RT1170" CACHE STRING "Display profile: RT1050|RT1170|DESKTOP")

# --- LVGL library ---
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h)
add_subdirectory(${CMAKE_SOURCE_DIR}/../../external/lvgl lvgl_build)

# --- Shared fizmo sources ---
set(FIZMO_SRC_DIR ${CMAKE_SOURCE_DIR}/../../src)
# ... add fizmo bridge, filesystem, story embedding sources

# --- Platform-specific sources ---
if(BUILD_DESKTOP)
    # SDL2 port + desktop fizmo bridge
    find_package(SDL2 REQUIRED)
    add_subdirectory(port/sdl)
    target_sources(${TARGET} PRIVATE main_desktop.c)
elseif(DISPLAY_PROFILE STREQUAL "RT1170")
    add_subdirectory(port/rt1170)
    target_sources(${TARGET} PRIVATE main_freertos.c)
elseif(DISPLAY_PROFILE STREQUAL "RT1050")
    add_subdirectory(port/rt1050)
    target_sources(${TARGET} PRIVATE main_freertos.c)
endif()

# --- Application UI (shared across all platforms) ---
target_sources(${TARGET} PRIVATE
    app/zork_ui.c
    app/zork_output.c
    app/zork_input.c
    app/zork_status.c
    app/zork_compass.c
    app/zork_theme.c
    adapter/fizmo_lvgl_adapter.c
)
```

### Build Invocation Examples

```bash
# RT1170 FreeRTOS build (via MCUXpresso SDK toolchain file)
cmake -B build/rt1170 -S ui/lvgl \
    -DCMAKE_TOOLCHAIN_FILE=$MCUXPRESSO_SDK/tools/cmake_toolchain_files/armgcc.cmake \
    -DDISPLAY_PROFILE=RT1170 \
    -GNinja
cmake --build build/rt1170

# RT1050 FreeRTOS build
cmake -B build/rt1050 -S ui/lvgl \
    -DCMAKE_TOOLCHAIN_FILE=$MCUXPRESSO_SDK/tools/cmake_toolchain_files/armgcc.cmake \
    -DDISPLAY_PROFILE=RT1050 \
    -GNinja
cmake --build build/rt1050

# Desktop SDL2 simulator
cmake -B build/desktop -S ui/lvgl \
    -DBUILD_DESKTOP=ON \
    -DDISPLAY_PROFILE=DESKTOP \
    -GNinja
cmake --build build/desktop
```

---

## 6. Display & Input Driver Layer

### Strategy: Reuse NXP SDK + Official Board Port as Reference

The NXP MCUXpresso SDK includes complete LVGL example projects for both target boards.
The official [lv_port_nxp_imxrt1170-evkb](https://github.com/lvgl/lv_port_nxp_imxrt1170-evkb)
provides a standalone reference. We will adapt these rather than writing from scratch.

### RT1170 Display Driver (`port/rt1170/lv_port_disp.c`)

| Aspect | Detail |
|--------|--------|
| LCD panel | RK055HDMIPI4MA0 (720×1280, MIPI-DSI) |
| Controller | LCDIF → MIPI-DSI bridge → D-PHY |
| Color depth | RGB565 (16-bit) for memory savings |
| Rendering mode | `LV_DISPLAY_RENDER_MODE_PARTIAL` |
| Draw buffers | 2 × (720 × 64 × 2 bytes) = ~180 KB in internal SRAM |
| Framebuffer | 720 × 1280 × 2 = ~1.8 MB in SDRAM |
| GPU acceleration | PXP (`LV_USE_DRAW_PXP=1`) for blitting/fill; optionally VGLite |
| Flush callback | Set LCDIF next-buffer address, signal `lv_display_flush_ready()` from VSYNC IRQ |
| Double buffering | Yes — swap framebuffers on VSYNC to avoid tearing |

```c
// Pseudocode for RT1170 flush callback
static void rt1170_flush_cb(lv_display_t *disp, const lv_area_t *area,
                            uint8_t *px_map) {
    /* Copy rendered area to framebuffer via PXP or memcpy */
    pxp_blit(px_map, area, active_framebuffer);

    /* On full-screen flush: swap framebuffer on next VSYNC */
    if (lv_display_flush_is_last(disp)) {
        LCDIF_SetNextBufferAddr(LCDIF, (uint32_t)active_framebuffer);
        swap_framebuffer();
    }

    lv_display_flush_ready(disp);
}
```

### RT1050 Display Driver (`port/rt1050/lv_port_disp.c`)

| Aspect | Detail |
|--------|--------|
| LCD panel | RK043FN02H-CT (480×272, parallel RGB) |
| Controller | eLCDIF (parallel RGB: HSYNC, VSYNC, DE, DOTCLK) |
| Color depth | RGB565 (16-bit) |
| Rendering mode | `LV_DISPLAY_RENDER_MODE_PARTIAL` |
| Draw buffers | 2 × (480 × 32 × 2 bytes) = ~60 KB in OCRAM |
| Framebuffer | 480 × 272 × 2 = ~261 KB in SDRAM |
| GPU acceleration | PXP (`LV_USE_DRAW_PXP=1`) |
| Flush callback | Same pattern — eLCDIF next-buffer + VSYNC swap |

### Touch Input Driver (`port/*/lv_port_indev.c`)

Both boards use capacitive touch panels with I2C controllers. The NXP SDK provides
the touch driver API (`fsl_ft5406_rt.h` for RT1050, `fsl_gt911.h` for RT1170).

```c
// Pseudocode for touch input read callback
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    touch_point_t point;
    if (BOARD_Touch_GetPoint(&point)) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

### SDL2 Desktop Driver (`port/sdl/`)

LVGL v9 includes a built-in SDL driver (`lv_sdl_window.h`). We configure it to match
each target resolution for accurate on-PC development.

```c
// Desktop simulator display setup
lv_display_t *disp = lv_sdl_window_create(HOR_RES, VER_RES);
lv_indev_t *mouse = lv_sdl_mouse_create();
lv_indev_t *keyboard = lv_sdl_keyboard_create();
```

For the desktop build, a physical keyboard input device will be registered alongside
the mouse, so the on-screen keyboard is optional (just as in the Qt desktop build).

---

## 7. FreeRTOS Integration

### Task Architecture

The existing two-task model is preserved. A third responsibility (LVGL tick/render) is
folded into the Qt/UI task slot.

| Task | Priority | Stack | Role |
|------|----------|-------|------|
| **LVGL Task** | 3 (was Qt Task) | 8 KB | `lv_timer_handler()` loop + fizmo adapter polling |
| **Fizmo Task** | 4 | 8 KB | Z-machine interpreter (unchanged) |

### LVGL Task Implementation

```c
// main_freertos.c — LVGL variant
#define LVGL_TASK_STACK_SIZE  (8 * 1024)
#define FIZMO_TASK_STACK_SIZE (8 * 1024)

static void lvgl_task(void *pvParameters) {
    /* Initialize LVGL */
    lv_init();
    lv_tick_set_cb(xTaskGetTickCount);   /* Use FreeRTOS tick as LVGL tick */

    /* Initialize display and input drivers */
    lv_port_disp_init();
    lv_port_indev_init();

    /* Build the Zork UI */
    zork_ui_init();

    /* Start the fizmo adapter (registers an lv_timer for polling) */
    fizmo_lvgl_adapter_init();

    /* Main loop */
    for (;;) {
        uint32_t delay_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

int main(void) {
    BOARD_Init();  /* NXP SDK board init */

    xTaskCreate(lvgl_task,  "LVGL",  LVGL_TASK_STACK_SIZE / sizeof(StackType_t),
                NULL, 3, NULL);
    xTaskCreate(fizmo_task, "Fizmo", FIZMO_TASK_STACK_SIZE / sizeof(StackType_t),
                NULL, 4, NULL);

    vTaskStartScheduler();
}
```

### Thread Safety

LVGL v9 with `LV_USE_OS = LV_OS_FREERTOS` provides a built-in recursive mutex:

- `lv_timer_handler()` internally acquires the lock — no manual wrapping needed.
- The fizmo adapter's `lv_timer` callback runs inside `lv_timer_handler()` — safe.
- `lv_tick_inc()` and `lv_display_flush_ready()` are ISR-safe — no lock needed.
- If any future code needs to call LVGL APIs from the fizmo task, it must use
  `lv_lock()` / `lv_unlock()`.

### `lv_conf.h` FreeRTOS Settings

```c
#define LV_USE_OS               LV_OS_FREERTOS
#define LV_USE_MUTEX            1
#define LV_OS_CUSTOM_INCLUDE    "FreeRTOS.h"
```

---

## 8. UI Design & Widget Layout

### Screen Layout

The UI mirrors the existing Qt UI's information architecture, adapted for LVGL widgets.

#### RT1170 (720×1280, Portrait)

```
┌──────────────────────────────────────┐
│          STATUS BAR (48px)           │
│  ┌──────────────┬───────────────┐    │
│  │ West of House│  Score: 0/0   │    │
│  └──────────────┴───────────────┘    │
├──────────────────────────────────────┤
│                                      │
│      OUTPUT TEXT AREA (scrollable)   │
│      lv_textarea, read-only          │
│      Green text (#00FF00) on         │
│      black (#000000) background      │
│      ~700px height                   │
│                                      │
├──────────────────────────────────────┤
│  ┌─────────────────────────────────┐ │
│  │ > INPUT FIELD (lv_textarea)     │ │
│  │   Single-line, 56px height      │ │
│  └─────────────────────────────────┘ │
├──────────────────────────────────────┤
│                                      │
│      COMPASS ROSE (lv_image +        │
│      touch detection, 200×200)       │
│                                      │
├──────────────────────────────────────┤
│                                      │
│      VIRTUAL KEYBOARD (lv_keyboard)  │
│      ~280px height                   │
│                                      │
└──────────────────────────────────────┘
```

#### RT1050 (480×272, Landscape)

```
┌──────────────────────────────────────────────────┐
│ STATUS BAR (24px)                                │
│  West of House                    Score: 0/0     │
├────────────────────────────────┬─────────────────┤
│                                │                 │
│  OUTPUT TEXT AREA              │  COMPASS ROSE   │
│  lv_textarea, read-only        │  (100×100)      │
│  ~180px height                 │                 │
│                                │                 │
├────────────────────────────────┴─────────────────┤
│ > INPUT FIELD (lv_textarea, 32px)                │
└──────────────────────────────────────────────────┘

  (Virtual keyboard: slide-up overlay on tap, same as Qt RT1050 behavior)
```

### Widget Details

#### Output Text Area (`zork_output.c`)

- **Widget:** `lv_textarea` in read-only mode
- **Cursor:** Hidden (`lv_textarea_set_cursor_click_pos(ta, false)`)
- **Text append:** `lv_textarea_add_text(output_ta, new_text)`
- **Auto-scroll:** After appending, scroll to bottom:
  `lv_obj_scroll_to_y(output_ta, LV_COORD_MAX, LV_ANIM_OFF)`
- **Scrollback trimming:** Same strategy as `FizmoBackend.cpp` — when text exceeds
  a threshold (4 KB for RT1050, 16 KB for RT1170), trim from the top on newline
  boundaries, keeping at least 10–20 lines of context
- **Max length:** `lv_textarea_set_max_length(ta, MAX_OUTPUT_CHARS)`
- **Styling:** Green monospace text on black, 8px padding

#### Status Bar (`zork_status.c`)

- **Container:** `lv_obj` with horizontal flex layout
- **Room name:** `lv_label` left-aligned
- **Score/moves:** `lv_label` right-aligned
- **Styling:** White text on dark grey (#1A1A1A) background
- **Update:** Called from the fizmo adapter when status line version changes

#### Input Field (`zork_input.c`)

- **Widget:** `lv_textarea`, single-line mode (`lv_textarea_set_one_line(ta, true)`)
- **Prompt:** ">" prefix rendered as a label to the left of the textarea
- **Submit:** On `LV_EVENT_READY` from the keyboard (OK/Enter key):
  1. Read text from input textarea
  2. Echo `"> {command}\n"` to output textarea
  3. Pass command to fizmo bridge via `rtos_submit_input()`
  4. Clear input textarea
- **Focus:** Auto-focus when fizmo is waiting for input
  (`fizmo_rtos_is_waiting_for_input()`)

#### Virtual Keyboard (`zork_input.c`)

- **Widget:** `lv_keyboard`, bound to input textarea via
  `lv_keyboard_set_textarea(kb, input_ta)`
- **Modes:** Text lower, text upper, special characters
- **Custom layout (optional):** For the compass directions, a `USER_1` mode could map
  keys to N/S/E/W/NE/NW/SE/SW for quick navigation
- **Visibility:**
  - RT1170: Always visible (screen has room in portrait mode)
  - RT1050: Hidden by default; shown when input textarea receives focus; hidden
    after submit (matching Qt behavior)
  - Desktop: Hidden (physical keyboard available via SDL)

#### Compass Rose (`zork_compass.c`)

- **Widget:** `lv_image` displaying the compass rose PNG (converted to LVGL image
  descriptor via `lv_img_conv` offline tool)
- **Touch handling:** Event callback on `LV_EVENT_CLICKED` that computes the angle
  from the image center to the touch point, same 8-way detection logic as in
  `ZorkUI.qml`:
  ```
  angle → direction:
    337.5°–22.5°   → "n"
     22.5°–67.5°   → "ne"
     67.5°–112.5°  → "e"
    112.5°–157.5°  → "se"
    157.5°–202.5°  → "s"
    202.5°–247.5°  → "sw"
    247.5°–292.5°  → "w"
    292.5°–337.5°  → "nw"
  ```
- **Dead zone:** Touches within 15% of the center radius are ignored (prevents
  accidental input)
- **Action:** On valid direction touch, directly call `rtos_submit_input(direction)`
  — same as typing "n" + Enter

### Theme (`zork_theme.c`)

A custom LVGL theme callback to set the retro terminal aesthetic:

| Element | Style |
|---------|-------|
| Screen background | Black (#000000) |
| Output text | Green (#00FF00), monospace font |
| Input text | Green (#00FF00), monospace font |
| Input background | Dark grey (#1A1A1A) |
| Status bar background | Dark grey (#1A1A1A) |
| Status text | White (#FFFFFF) |
| Keyboard background | Dark grey (#222222) |
| Keyboard keys | Grey (#333333) text Green (#00FF00) |
| Scrollbar | Dark green (#004400) |
| Borders | None (clean terminal look) |

---

## 9. Fizmo Bridge Reuse

### What Gets Reused (Unchanged)

| File | Purpose | Reuse |
|------|---------|-------|
| `src/fizmo_rtos_bridge.c/h` | FreeRTOS queue/semaphore bridge | 100% — untouched |
| `src/fizmo_bridge.cpp/h` | Desktop std::thread bridge | 100% — untouched |
| `src/fizmo_filesys_hybrid.c/h` | Story from flash, saves to SD | 100% — untouched |
| `src/fizmo_locale_stubs.c` | Locale stubs | 100% — untouched |
| `src/fizmo_embedded_compat.h` | Compatibility macros | 100% — untouched |
| `src/posix_stubs.c` | POSIX function stubs | 100% — untouched |
| `src/story_data.S/h` | Embedded story file | 100% — untouched |
| `src/diskio_stub.c` | FatFS disk I/O | 100% — untouched |
| `zork1.z3` | Story file | 100% — untouched |

### What Gets Replaced

The `FizmoBackend` class (`ui/qul/ZorkUI/FizmoBackend.cpp/h`) is Qt-specific. Its
responsibilities are reimplemented in `fizmo_lvgl_adapter.c`:

| FizmoBackend Responsibility | LVGL Adapter Equivalent |
|----------------------------|------------------------|
| 50ms `QTimer` polling | `lv_timer_create(poll_cb, 50, NULL)` |
| `rtos_try_read_output()` → accumulate | Same call → `lv_textarea_add_text()` |
| UTF-32→UTF-8 conversion | Same conversion code (portable C) |
| Scrollback trimming | Same algorithm, adapted for `lv_textarea_get_text()` / `lv_textarea_set_text()` |
| `submitLine()` → `rtos_submit_input()` | `LV_EVENT_READY` handler → `rtos_submit_input()` |
| Status line version tracking | Same version counter check → `lv_label_set_text()` |
| `waitingForInput` state | Same `rtos_is_waiting_for_input()` check → enable/disable input field |

### Adapter API (`fizmo_lvgl_adapter.h`)

```c
/**
 * Initialize the fizmo↔LVGL adapter.
 * Registers an lv_timer for periodic output polling.
 * Must be called after zork_ui_init() and after LVGL is initialized.
 *
 * @param output_ta  The read-only output textarea widget
 * @param input_ta   The input textarea widget
 * @param room_label The room name label in the status bar
 * @param score_label The score/moves label in the status bar
 */
void fizmo_lvgl_adapter_init(lv_obj_t *output_ta, lv_obj_t *input_ta,
                             lv_obj_t *room_label, lv_obj_t *score_label);

/**
 * Submit a command string to the fizmo interpreter.
 * Called from input field submit handler or compass rose.
 */
void fizmo_lvgl_submit_command(const char *cmd);

/**
 * Submit a single character (for [MORE] prompt handling).
 */
void fizmo_lvgl_submit_char(char c);
```

---

## 10. Desktop Simulator Build

### Architecture

```
┌─────────────────────────────────┐
│  Same LVGL UI code (app/*.c)    │
├─────────────────────────────────┤
│  fizmo_lvgl_adapter.c           │
├─────────────────────────────────┤
│  fizmo_bridge.cpp (std::thread) │
├─────────────────────────────────┤
│  libfizmo (native build)        │
├─────────────────────────────────┤
│  LVGL SDL2 driver               │
│  (lv_sdl_window + mouse + kbd)  │
├─────────────────────────────────┤
│  SDL2                           │
└─────────────────────────────────┘
```

### `main_desktop.c`

```c
int main(int argc, char *argv[]) {
    lv_init();

    /* SDL display + input */
    lv_display_t *disp = lv_sdl_window_create(HOR_RES, VER_RES);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_t *kb = lv_sdl_keyboard_create();

    /* Build UI */
    zork_ui_init();
    fizmo_lvgl_adapter_init(output_ta, input_ta, room_label, score_label);

    /* Start fizmo in a background thread */
    fizmo_bridge_start(story_path);

    /* Main loop */
    while (1) {
        uint32_t delay = lv_timer_handler();
        SDL_Delay(delay);
    }
    return 0;
}
```

### Desktop-Specific Behavior

- Story file loaded from filesystem (path via env var `ZORK_STORY_PATH` or default
  `../../zork1.z3`)
- Window title: "Zork for MCUs — LVGL Simulator"
- Resizable window: No (fixed to target resolution for accurate layout testing)
- Multiple resolution profiles selectable at build time:
  - `DESKTOP_720x1280` — matches RT1170
  - `DESKTOP_480x272` — matches RT1050
  - `DESKTOP_800x600` — comfortable desktop default

---

## 11. Memory Budget

### RT1170 (4.5 MB RAM, 8 MB Flash)

| Component | RAM | Flash | Notes |
|-----------|-----|-------|-------|
| LVGL core + widgets | 48 KB heap | ~200 KB | `LV_MEM_SIZE=48*1024` |
| Draw buffers (2×) | 180 KB | — | 720×64×2×2 in SRAM |
| Framebuffer (2×) | 3.6 MB | — | 720×1280×2×2 in SDRAM |
| Fonts (Montserrat 20, 24) | — | ~40 KB | Two sizes |
| Compass rose image | ~4 KB | ~38 KB | Compressed in flash, decoded to RAM |
| Output text buffer | 16 KB | — | Scrollback in LVGL textarea |
| Fizmo interpreter | ~64 KB | ~100 KB | Z-machine dynamic memory + code |
| FreeRTOS kernel + stacks | ~20 KB | ~10 KB | Two tasks × 8 KB stack |
| **Total** | **~3.9 MB** | **~390 KB** | Comfortable fit |

### RT1050 (512 KB SRAM + 32 MB SDRAM, 8+ MB Flash)

| Component | Internal SRAM | SDRAM | Flash | Notes |
|-----------|--------------|-------|-------|-------|
| LVGL core + widgets | 32 KB heap | — | ~170 KB | Reduced `LV_MEM_SIZE=32*1024` |
| Draw buffers (2×) | 60 KB | — | — | 480×32×2×2 in OCRAM |
| Framebuffer (2×) | — | 522 KB | — | 480×272×2×2 in SDRAM |
| Fonts (Montserrat 14) | — | — | ~15 KB | Single size |
| Compass rose image | ~2 KB | — | ~20 KB | Smaller version |
| Output text buffer | 4 KB | — | — | Reduced scrollback |
| Fizmo interpreter | — | ~64 KB | ~100 KB | Dynamic memory in SDRAM |
| FreeRTOS kernel + stacks | ~20 KB | — | ~10 KB | |
| **Total** | **~120 KB** | **~590 KB** | **~315 KB** | Fits with margin |

### `lv_conf.h` Size Optimization (RT1050)

```c
/* Disable unused widgets to reduce flash */
#define LV_USE_CHART            0
#define LV_USE_CANVAS           0
#define LV_USE_CALENDAR         0
#define LV_USE_COLORWHEEL       0
#define LV_USE_IMGBTN           0
#define LV_USE_LED              0
#define LV_USE_METER            0
#define LV_USE_MSGBOX           0
#define LV_USE_SPAN             0
#define LV_USE_SPINBOX          0
#define LV_USE_TABLE            0
#define LV_USE_TABVIEW          0
#define LV_USE_TILEVIEW         0
#define LV_USE_WIN              0
#define LV_USE_MENU             0
#define LV_USE_ROLLER           0

/* Disable unused features */
#define LV_USE_ANIMATION        0   /* No animations needed */
#define LV_USE_SHADOW           0   /* No shadow effects */
#define LV_USE_BLEND_MODES      0   /* No blend modes */
#define LV_USE_LOG              0   /* Disable in production */

/* Enable only needed fonts */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_20   0   /* RT1050: only 14px */
#define LV_FONT_MONTSERRAT_24   0
```

---

## 12. Font Strategy

### Approach: Offline-Converted Custom Monospace Font

A terminal-style text adventure demands a **monospace** font. LVGL's built-in
Montserrat is proportional, so we need a custom font.

**Recommended font:** [IBM Plex Mono](https://github.com/IBM/plex) (SIL Open Font
License) or [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) (also OFL).

**Conversion:** Use the [LVGL Online Font Converter](https://lvgl.io/tools/fontconverter)
or the offline `lv_font_conv` tool:

```bash
npx lv_font_conv \
    --bpp 4 \
    --size 14 \
    --font JetBrainsMono-Regular.ttf \
    --range 0x20-0x7E \
    --format lvgl \
    --output lv_font_zork_14.c

npx lv_font_conv \
    --bpp 4 \
    --size 24 \
    --font JetBrainsMono-Regular.ttf \
    --range 0x20-0x7E \
    --format lvgl \
    --output lv_font_zork_24.c
```

| Font | Size | BPP | Glyph Range | Estimated Flash |
|------|------|-----|-------------|-----------------|
| `lv_font_zork_14` | 14px | 4 | ASCII 0x20–0x7E (95 glyphs) | ~8 KB |
| `lv_font_zork_24` | 24px | 4 | ASCII 0x20–0x7E (95 glyphs) | ~20 KB |

**Platform mapping:**
- RT1050: `lv_font_zork_14` only
- RT1170: `lv_font_zork_24` (output), `lv_font_zork_14` (status bar, keyboard)
- Desktop: Either, selectable via display profile

**Fallback:** LVGL's built-in `LV_FONT_MONTSERRAT_14` can be used during early
development before custom fonts are converted.

---

## 13. Implementation Phases

### Phase 0: Project Scaffolding (Est. 1–2 days)

- [ ] Add LVGL v9.4.x as git submodule at `external/lvgl/`
- [ ] Create `ui/lvgl/` directory structure
- [ ] Create initial `lv_conf.h` (desktop-first, permissive settings)
- [ ] Create `CMakeLists.txt` with desktop SDL2 build support
- [ ] Verify bare LVGL compiles and shows a test screen on desktop

**Exit criteria:** `cmake --build build/desktop && ./ZorkLVGL` opens an SDL window
showing "Hello LVGL" label.

### Phase 1: Desktop Simulator Shell (Est. 2–3 days)

- [ ] Implement `zork_theme.c` — green-on-black terminal aesthetic
- [ ] Implement `zork_ui.c` — screen layout with placeholder widgets
- [ ] Implement `zork_output.c` — read-only scrolling output textarea
- [ ] Implement `zork_status.c` — status bar with room and score labels
- [ ] Implement `zork_input.c` — input textarea + keyboard binding
- [ ] Wire up physical keyboard input (SDL keyboard → input textarea)
- [ ] Verify layout matches target resolutions (720×1280, 480×272)

**Exit criteria:** Desktop simulator shows the full UI layout with placeholder text.
Typing on the physical keyboard appears in the input field. Pressing Enter clears
the input and echoes text to the output area.

### Phase 2: Fizmo Integration on Desktop (Est. 2–3 days)

- [ ] Implement `fizmo_lvgl_adapter.c` — lv_timer polling, output accumulation,
      UTF-32→UTF-8 conversion, scrollback trimming
- [ ] Integrate `fizmo_bridge.cpp` into the desktop build (std::thread)
- [ ] Wire up `main_desktop.c` — full fizmo + LVGL pipeline
- [ ] Implement `[MORE]` prompt handling (single-char submit)
- [ ] Test: Play through opening sequence of Zork I on desktop

**Exit criteria:** Full game is playable on desktop — can navigate West of House,
open mailbox, read leaflet, enter house, explore. Status bar updates with room name
and score. Output scrolls correctly with trimming.

### Phase 3: Compass Rose (Est. 1 day)

- [ ] Convert `compass-rose.png` to LVGL image descriptor
- [ ] Implement `zork_compass.c` — image display + 8-way touch detection
- [ ] Integrate into screen layout (both portrait and landscape variants)
- [ ] Test all 8 directions + dead zone on desktop (mouse click)

**Exit criteria:** Clicking compass directions on desktop simulator sends the correct
movement command. Dead zone in center is ignored.

### Phase 4: On-Screen Keyboard Tuning (Est. 1 day)

- [ ] Configure `lv_keyboard` visibility behavior per display profile
- [ ] Implement show/hide animation for RT1050 (tap-to-show, hide-after-submit)
- [ ] Create optional `USER_1` compass-direction quick-key mode
- [ ] Test keyboard → input textarea → fizmo pipeline

**Exit criteria:** Virtual keyboard works correctly for all three display profiles.

### Phase 5: RT1170 Board Bring-Up (Est. 3–5 days)

- [ ] Create `port/rt1170/lv_port_disp.c` based on NXP SDK example / official port
- [ ] Create `port/rt1170/lv_port_indev.c` for GT911 touch controller
- [ ] Create `main_freertos.c` with two-task architecture
- [ ] Adapt `CMakeLists.txt` for ARM cross-compilation with MCUXpresso SDK
- [ ] Configure `lv_conf.h` RT1170 variant (PXP acceleration, memory sizes)
- [ ] Flash and test on hardware — verify display output, touch input, game play
- [ ] Performance tuning: draw buffer sizes, PXP utilization, FPS

**Exit criteria:** Zork I is fully playable on RT1170-EVKB hardware. Touch keyboard,
compass rose, scrolling output, status bar all functional. Visually matches the Qt
version's layout.

### Phase 6: RT1050 Board Bring-Up (Est. 3–5 days)

- [ ] Create `port/rt1050/lv_port_disp.c` based on NXP SDK example
- [ ] Create `port/rt1050/lv_port_indev.c` for FT5406 touch controller
- [ ] Create RT1050-optimized `lv_conf.h` (disabled widgets, reduced memory)
- [ ] Convert custom fonts at 14px only
- [ ] Adapt compass rose image (smaller resolution)
- [ ] Test on RT1050-EVK hardware: full game playthrough
- [ ] Memory optimization: verify fit within SRAM + SDRAM budget

**Exit criteria:** Zork I is fully playable on RT1050-EVK hardware within the memory
budget. Keyboard show/hide works on tap.

### Phase 7: Polish & Documentation (Est. 1–2 days)

- [ ] README updates: document LVGL build instructions for all three targets
- [ ] Verify both Qt and LVGL builds still work (no regressions in shared code)
- [ ] Code cleanup, consistent naming, header documentation
- [ ] Add build CI configuration (if applicable)

**Exit criteria:** A new developer can clone the repo, follow the README, and build
either the Qt or LVGL variant for any target.

### Total Estimated Effort: 14–22 days

---

## 14. Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|-----------|--------|------------|
| 1 | **RT1050 memory overflow** — LVGL + fizmo exceeds 512 KB SRAM | Medium | High | Place framebuffer in SDRAM. Aggressively disable unused LVGL widgets/features. Measure actual usage at Phase 6 start. Fallback: reduce draw buffer count to 1. |
| 2 | **LVGL textarea performance with long text** — Slow scrolling or redraws with 16 KB of accumulated text | Low | Medium | Enable `LV_LABEL_LONG_TXT_HINT`. Implement aggressive trimming (keep only last N lines). Profile on hardware in Phase 5. |
| 3 | **NXP display driver complexity** — Display init code is board-specific and fragile | Medium | Medium | Start from official NXP SDK LVGL examples rather than writing from scratch. The lv_port_nxp_imxrt1170-evkb repo provides a reference. |
| 4 | **Touch calibration / orientation mismatch** — RT1170 portrait mode may require coordinate transformation | Medium | Low | Handle in `lv_port_indev.c` with a coordinate mapping function. NXP SDK examples typically handle this. |
| 5 | **LVGL v9 API instability** — Minor API changes between v9.4.x point releases | Low | Low | Pin to a specific release tag. Use the LVGL migration guide if updating. |
| 6 | **CMake integration with MCUXpresso SDK** — Toolchain file incompatibilities | Medium | Medium | Reference existing Qt build's CMake patterns. Use the NXP SDK's armgcc toolchain file. Test cross-compilation early (Phase 5). |
| 7 | **Shared `src/` code modifications needed** — Despite planning for reuse, the fizmo bridge may need changes | Low | Medium | Any changes to `src/` must remain backward-compatible with the Qt build. Use `#ifdef USE_LVGL` only as a last resort. |
| 8 | **Font rendering quality** — Offline-converted bitmap fonts may look poor at small sizes or non-integer scaling | Low | Low | Use 4-BPP anti-aliasing. Test multiple font sizes. Montserrat fallback for initial development. |

---

## 15. Testing Strategy

### Desktop Simulator Tests (Continuous)

All development starts on the desktop simulator. Manual testing covers:

- **Smoke test:** Launch game, read opening text, enter first command
- **Scrollback test:** Play 50+ moves, verify output trimming works (no memory growth)
- **Input edge cases:** Empty input, very long input (>256 chars), special characters
- **Status bar updates:** Verify room name and score change on movement
- **Compass rose:** Click all 8 directions + center dead zone
- **Keyboard:** All key modes (lower, upper, special, numbers)
- **[MORE] prompt:** Trigger a long description, verify single-key continue works
- **Resolution profiles:** Test at 720×1280 and 480×272

### Hardware Tests (Phase 5–6)

- **Visual inspection:** Compare LVGL UI side-by-side with Qt UI screenshots
- **Touch responsiveness:** Verify <100ms input-to-display latency
- **Long play session:** 30+ minutes continuous play to check for memory leaks
- **FreeRTOS diagnostics:** Heap high-water mark, stack high-water mark per task
- **Power cycle:** Verify game starts cleanly after reset

### Regression Tests for Qt Build

After any changes to shared `src/` files:

- Rebuild the Qt for MCUs desktop build and verify it still works
- Rebuild the Qt FreeRTOS builds if hardware is available

---

## 16. References

### LVGL Documentation
- [LVGL v9 Docs](https://docs.lvgl.io/master/)
- [LVGL Porting Guide](https://docs.lvgl.io/master/porting/index.html)
- [LVGL FreeRTOS Threading](https://docs.lvgl.io/master/intro/add-lvgl-to-your-project/threading.html)
- [LVGL Keyboard Widget](https://docs.lvgl.io/master/widgets/keyboard.html)
- [LVGL Text Area Widget](https://docs.lvgl.io/master/widgets/textarea.html)
- [LVGL Font Converter](https://lvgl.io/tools/fontconverter)
- [LVGL Image Converter](https://lvgl.io/tools/imageconverter)

### NXP Resources
- [lv_port_nxp_imxrt1170-evkb](https://github.com/lvgl/lv_port_nxp_imxrt1170-evkb) — Official LVGL board port
- [MCUXpresso SDK LVGL Middleware](https://mcuxpresso.nxp.com/mcuxsdk/latest/html/middleware/lvgl/index.html)
- [NXP GUI Guider](https://www.nxp.com/design/design-center/software/embedded-software/gui-guider:GUI-GUIDER)
- [NXP RT1050 EVK LVGL Board Review](https://blog.lvgl.io/2021-03-24/nxp-imxrt1050-evk-review)

### Desktop Simulator
- [lv_port_pc_vscode](https://github.com/lvgl/lv_port_pc_vscode) — CMake + SDL2 PC simulator
- [LVGL SDL Driver](https://docs.lvgl.io/9.1/integration/ide/pc-simulator.html)

### Project-Internal
- `ui/qul/ZorkUI/FizmoBackend.cpp` — Reference for fizmo adapter logic
- `ui/qul/ZorkUI/ZorkUI.qml` — Reference for UI layout
- `src/fizmo_rtos_bridge.c` — FreeRTOS bridge API (to be reused)
- `ui/qul/ZorkUI/DisplayConfig.h` — Display profile constants (to be mirrored)
