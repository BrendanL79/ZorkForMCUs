# Plan: Port ZorkForMCUs UI from Qt for MCUs to LVGL

## Motivation

Qt for MCUs (QUL) requires a commercial license. LVGL (Light and Versatile
Graphics Library) is MIT-licensed, open source, free of charge, and widely
adopted in the embedded world. It runs on bare-metal, FreeRTOS, Zephyr, and
desktop (SDL), making it a natural replacement.

---

## Current Architecture Summary

| Layer | Current Implementation |
|-------|----------------------|
| **UI Layout** | QML files (`ZorkUI.qml`, `ZorkUI_RT1050.qml`) |
| **Backend Singleton** | `FizmoBackend` — C++ class inheriting `Qul::Singleton`, exposes properties/signals to QML |
| **Event Bridge** | `Qul::EventQueue<FizmoEvent>` for thread-safe fizmo→UI communication |
| **Timer Polling** | `Qul::Timer` polls fizmo output every 50ms |
| **Input Handling** | QML `TextInput` / custom `ZorkKeyboard` / compass rose touch widget |
| **Text Rendering** | Spark font engine (runtime TTF) or Static (pre-rendered glyphs) |
| **Build System** | CMake with `qul_add_target()` and `.qmlproject` files |
| **Platforms** | NXP RT1050-EVK, RT1170-EVKB, Desktop (SDL-based via Qt) |

### Key Data Flows

```
fizmo interpreter (FreeRTOS task / std::thread)
  → FreeRTOS queue / ring buffer
  → FizmoBackend::pollFizmoOutput()  [50ms timer]
  → appendOutput() → outputVersion++ → QML rebinds → Text element updates

Touch/keyboard input
  → QML handler → FizmoBackend::submitLine() / submitChar()
  → fizmo_submit_line() / fizmo_submit_char()  [C bridge]
  → interpreter wakes and processes command
```

---

## LVGL Port Plan

### Phase 1 — LVGL Integration & Build System

**Goal:** Get LVGL compiling alongside the existing project for desktop first.

1. **Add LVGL as a git submodule** (v9.x branch, MIT license)
   - `git submodule add https://github.com/lvgl/lvgl.git lib/lvgl`
2. **Add `lv_drivers` or `lv_port_pc_visual_studio`** for desktop SDL backend
   - Or use LVGL v9's built-in SDL driver
3. **Create `ui/lvgl/` directory** mirroring the `ui/qul/` structure
4. **Write new CMakeLists.txt** under `ui/lvgl/ZorkUI/`
   - Include LVGL sources, `lv_conf.h`, and the fizmo interpreter sources
   - Reuse the existing fizmo source file list and compile definitions
   - Support `DISPLAY_PROFILE` variable (RT1050 / RT1170 / Desktop)
5. **Create `lv_conf.h`** with project-specific settings:
   - Enable `LV_USE_FREETYPE` or `LV_USE_TINY_TTF` for runtime font rendering
   - Enable `LV_USE_TEXTAREA`, `LV_USE_KEYBOARD`, `LV_USE_LABEL`
   - Set color depth, DPI, and memory pool sizes per platform

### Phase 2 — Replace FizmoBackend (UI Backend)

**Goal:** Replace the Qt-specific singleton with a plain C/C++ module that
drives LVGL widgets.

1. **Create `FizmoLvglBackend.h / .cpp`** (plain C++ class, no Qt dependency)
   - Same public interface concept:
     - `appendOutput(const char *text)`
     - `getOutputText() → const char*`
     - `submitLine(const char *text)`
     - `submitChar(int ch)`
     - `getStatusRoom() / getStatusScore()`
     - `getCommandText() / appendCommandChar() / commandBackspace()`
   - Internally manages the same output buffer (4KB / 16KB) and status buffers
   - Instead of `Qul::Property` version counters, sets LVGL dirty flags or
     calls `lv_label_set_text()` directly

2. **Replace `Qul::EventQueue`** with a FreeRTOS queue (already exists in
   `fizmo_rtos_bridge.c`) or a simple mutex + ring buffer for desktop
   - The existing `fizmo_rtos_bridge.c` / `fizmo_bridge.cpp` can remain
     largely unchanged — they already use FreeRTOS primitives / std::thread
   - Only the "consumer" side changes: instead of posting to `Qul::EventQueue`,
     post to a platform-agnostic queue that the LVGL backend drains

3. **Replace `Qul::Timer`** with `lv_timer_create()` for the 50ms poll cycle

### Phase 3 — Rebuild the UI in LVGL Widgets

**Goal:** Recreate the four UI regions using LVGL's widget system.

#### 3a. Screen Layout

```
┌──────────────────────────────────┐
│  Status Bar  (lv_obj + 2 labels) │  ← room name (left), score (right)
├──────────────────────────────────┤
│                          ┌──────┐│
│  Output Text Area        │Compas││  ← lv_textarea (read-only) or lv_label
│  (scrollable)            │ Rose ││     in a scrollable lv_obj container
│                          └──────┘│
├──────────────────────────────────┤
│  Input Bar  ("> " + lv_textarea) │  ← single-line text input
├──────────────────────────────────┤
│  Keyboard (lv_keyboard)         │  ← LVGL built-in keyboard widget
└──────────────────────────────────┘
```

#### 3b. Widget Mapping

| Current (QML) | LVGL Replacement | Notes |
|----------------|-----------------|-------|
| Status bar `Rectangle` + `Text` | `lv_obj` container + 2x `lv_label` | Set `lv_obj_set_flex_flow(LV_FLEX_FLOW_ROW)`, justify space-between |
| Output `Flickable` + `Text` | `lv_obj` (scrollable) + `lv_label` | `lv_label_set_long_mode(LV_LABEL_LONG_WRAP)`, auto-scroll to bottom |
| Compass rose `Image` + touch handler | Custom `lv_obj` with `lv_event_cb` | Convert touch coordinates to 8 directions, same math as current QML |
| `TextInput` | `lv_textarea` (single line) | `lv_textarea_set_one_line(true)`, handle `LV_EVENT_READY` for enter |
| `VirtualKeyboard` / `ZorkKeyboard` | `lv_keyboard` | Attach to input textarea via `lv_keyboard_set_textarea()` |
| Color theme (#1a1a2e bg, #00ff88 text) | `lv_style_t` objects | Apply via `lv_obj_add_style()` on each widget |

#### 3c. Compass Rose

- Create a custom widget or use an `lv_image` with a click event callback
- Reuse the same polar-coordinate math from the QML compass:
  - Compute angle from center, map to 8 sectors → direction string
  - Call `backend.submitLine("n")` etc.

#### 3d. Text Rendering & Fonts

- Use `LV_USE_FREETYPE` (FreeType integration) or `LV_USE_TINY_TTF` for
  runtime rendering from the same TTF file
- Alternatively, pre-generate `lv_font_t` bitmaps with LVGL's font converter
  tool for the RT1050 (smaller flash footprint, like the current Static engine)
- Desktop and RT1170 can use runtime font rendering

### Phase 4 — Platform Drivers

**Goal:** Wire LVGL to the actual displays and touch controllers.

#### 4a. Desktop (SDL)

- Use LVGL v9's built-in SDL display/input drivers (`lv_sdl_window_create()`)
- Straightforward, good for development iteration

#### 4b. NXP RT1170-EVKB

- Use NXP's LVGL port or write a thin display driver:
  - `lv_display_create()` with a flush callback writing to the LCD framebuffer
  - Touch: `lv_indev_create(LV_INDEV_TYPE_POINTER)` with FT5406 I2C driver
- NXP provides reference LVGL ports for i.MX RT series — leverage those
- The existing board SDK initialization (clocks, LCDIF, touch) can be reused

#### 4c. NXP RT1050-EVK

- Similar to RT1170 but with tighter memory constraints
- Use `LV_MEM_SIZE` tuned to available SRAM
- Consider partial rendering (`LV_DISPLAY_RENDER_MODE_PARTIAL`) to reduce
  framebuffer RAM (current display is only 480×272)
- Pre-rendered bitmap fonts instead of FreeType to save flash

### Phase 5 — Threading & RTOS Integration

**Goal:** Maintain the existing dual-task model.

```
FreeRTOS:
  Task 1: LVGL task (calls lv_timer_handler() in a loop, ~5ms period)
  Task 2: Fizmo task (blocks on input, produces output via queue)

Desktop:
  Main thread: LVGL event loop (SDL)
  Worker thread: fizmo interpreter (same as current std::thread model)
```

- The LVGL task replaces the current Qt event loop task (`Qul_Thread`)
- `lv_timer_handler()` is non-blocking and handles widget refresh + timers
- The 50ms fizmo poll timer runs inside LVGL's timer system
- Communication between tasks remains the same FreeRTOS queues/semaphores

### Phase 6 — Save/Restore (File System)

- No changes needed — `fizmo_filesys_hybrid.c` (FatFS on SD card) and
  desktop stdio are independent of the UI framework
- LVGL has optional `LV_USE_FS_*` drivers but we don't need them for save files

### Phase 7 — Testing & Polish

1. **Desktop first:** Get full gameplay working on SDL desktop build
2. **RT1170:** Port to hardware, verify display, touch, and save/load
3. **RT1050:** Port to constrained hardware, tune memory, verify fonts
4. **Visual polish:** Match the current color scheme and layout proportions
5. **Performance:** Profile `lv_timer_handler()` cycle time, ensure <16ms for
   60fps or <33ms for 30fps on MCU targets

---

## Files to Create

```
ui/lvgl/
├── ZorkUI/
│   ├── CMakeLists.txt          # Build config (replaces qul CMakeLists.txt)
│   ├── lv_conf.h               # LVGL configuration
│   ├── FizmoLvglBackend.h      # Backend (replaces FizmoBackend.h)
│   ├── FizmoLvglBackend.cpp    # Backend implementation
│   ├── ZorkScreen.h            # Main UI screen builder
│   ├── ZorkScreen.cpp          # Creates all LVGL widgets, styles, callbacks
│   ├── CompassRose.h           # Compass widget
│   ├── CompassRose.cpp         # Touch→direction logic
│   ├── DisplayConfig.h         # Display constants (can reuse/adapt existing)
│   ├── main_freertos.cpp       # FreeRTOS entry (adapted from existing)
│   └── main_desktop.cpp        # Desktop SDL entry
```

## Files to Reuse Unchanged

```
src/fizmo_rtos_bridge.c/.h      # FreeRTOS↔fizmo communication (minor edits)
src/fizmo_bridge.cpp/.h         # Desktop threading (minor edits)
src/fizmo_filesys_hybrid.c      # File system for saves
src/fizmo_locale_stubs.c        # Locale stubs
src/story_data.S / .h           # Embedded story data
libfizmo/                       # Entire interpreter submodule
zork1.z3                        # Story file
```

## Files That Can Be Retired

```
ui/qul/                         # Entire Qt for MCUs UI (kept for reference)
*.qml, *.qmlproject, *.qmlprojectconfig  # QML UI definitions
FizmoBackend.h/.cpp             # Qt-specific backend (replaced)
```

---

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| LVGL text rendering quality differs from Qt Spark | Test with same TTF font, tune hinting settings |
| RT1050 RAM too tight for LVGL | Use partial rendering, bitmap fonts, reduce `LV_MEM_SIZE` |
| Touch calibration differs between frameworks | Reuse NXP SDK touch drivers directly, calibrate in LVGL `indev` |
| Scrolling performance for large text output | Keep output buffer trimmed (already done), use `lv_label` with wrap instead of `lv_textarea` for read-only text |
| LVGL keyboard missing keys needed for Zork | Customize `lv_keyboard` map or build custom keyboard (like current `ZorkKeyboard.qml`) |

---

## Estimated Effort (by phase)

| Phase | Scope |
|-------|-------|
| Phase 1 | Build system, submodule, lv_conf.h |
| Phase 2 | Backend rewrite (FizmoLvglBackend) |
| Phase 3 | UI widget implementation (biggest phase) |
| Phase 4 | Platform display/touch drivers |
| Phase 5 | Threading integration |
| Phase 6 | Save/restore verification |
| Phase 7 | Testing and visual polish |

Phase 3 (UI widgets) is the largest piece of work. Phases 1-2 and 4-5 are
mostly mechanical. Phase 6 should require no changes. Phase 7 depends on
how close we want to match the original look.
