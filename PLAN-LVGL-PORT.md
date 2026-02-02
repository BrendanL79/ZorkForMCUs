# Plan: Port ZorkForMCUs UI from Qt for MCUs to LVGL

## Motivation

Qt for MCUs (QUL) requires a commercial license. LVGL (Light and Versatile
Graphics Library) is MIT-licensed, open source, free of charge, and widely
adopted in the embedded world. It runs on bare-metal, FreeRTOS, Zephyr, and
desktop (SDL), making it a natural replacement.

The repository is structured so that multiple UI toolkits (QUL, LVGL, Slint,
TouchGFX, etc.) can coexist. Shared functionality lives in `ui/common/` and
`cmake/FizmoCommon.cmake`; each toolkit only implements the thin UI-specific
layer.

---

## Repository Structure

```
ZorkForMCUs/
├── cmake/
│   └── FizmoCommon.cmake         # Shared CMake: libfizmo source lists,
│                                  # compile defs, helper functions
├── ui/
│   ├── common/                    # Toolkit-agnostic shared code
│   │   ├── DisplayConfig.h        # Display profiles (RT1050, RT1170, etc.)
│   │   ├── FizmoTextBuffer.h/.c   # Output buffer, UTF-32→UTF-8, trim, status,
│   │   │                          # command buffer management
│   │   ├── FizmoPoller.h/.c       # Polling logic: drains fizmo output queue,
│   │   │                          # tracks state, submits input
│   │   └── fizmo_stub.c           # Desktop stub (canned demo output)
│   │
│   ├── qul/                       # Qt for MCUs UI (existing)
│   │   └── ZorkUI/
│   │       ├── CMakeLists.txt     # Uses FizmoCommon.cmake
│   │       ├── FizmoBackend.h/.cpp # Qt-specific: Qul::Property, Timer,
│   │       │                       # EventQueue, String — delegates to common
│   │       ├── DisplayConfig.h     # Redirects to ui/common/DisplayConfig.h
│   │       ├── ZorkUI.qml          # Main QML UI
│   │       ├── ZorkUI_RT1050.qml   # RT1050-optimized UI
│   │       └── ...
│   │
│   └── lvgl/                      # LVGL UI (to be created)
│       └── ZorkUI/
│           ├── CMakeLists.txt
│           ├── lv_conf.h
│           ├── ZorkScreen.h/.cpp   # LVGL widgets, styles, layout
│           ├── CompassRose.h/.cpp  # Compass touch widget
│           ├── main_freertos.cpp   # FreeRTOS entry
│           └── main_desktop.cpp    # Desktop SDL entry
│
├── src/                           # Platform bridge code (toolkit-agnostic)
│   ├── fizmo_rtos_bridge.h/.c     # FreeRTOS↔fizmo communication
│   ├── fizmo_bridge.h/.cpp        # Desktop std::thread bridge
│   ├── fizmo_filesys_hybrid.c/.h  # FatFS save/load
│   ├── fizmo_locale_stubs.c       # Locale stubs
│   ├── fizmo_embedded_compat.h    # Embedded compatibility header
│   ├── story_data.S/.h            # Embedded story file
│   └── ...
│
├── external/
│   └── libfizmo/                  # Z-machine interpreter (git submodule)
│
└── zork1.z3                       # Story file
```

### What's shared (ui/common + cmake/FizmoCommon.cmake)

| Module | What it does |
|--------|-------------|
| `DisplayConfig.h` | Screen dimensions, font sizes, margins per display profile |
| `FizmoTextBuffer` | 4KB/16KB output ring buffer with smart trim, status line storage, command buffer, UTF-32→UTF-8 conversion |
| `FizmoPoller` | Polls `fizmo_output_available()` / `fizmo_output_read()`, converts and appends to text buffer, tracks input/char/exit state, returns change bitmask |
| `fizmo_stub.c` | Canned Zork I intro text for UI testing without the interpreter |
| `FizmoCommon.cmake` | libfizmo source file lists, compile definitions, `fizmo_common_apply()` and `fizmo_apply_display_profile()` helper functions |

### What each toolkit implements

Each UI toolkit only needs to:
1. Create widgets and render text from `FizmoTextBuffer`
2. Set up a periodic timer to call `fizmo_poller_poll()`
3. Map poll results (change bitmask) to widget updates
4. Handle touch/keyboard input → call `fizmo_poller_submit_line()` / `submit_char()`
5. Provide platform-specific display/touch drivers

---

## LVGL Port Plan

### Phase 1 — LVGL Integration & Build System

1. **Add LVGL as a git submodule** (v9.x branch, MIT license)
   - `git submodule add https://github.com/lvgl/lvgl.git external/lvgl`
2. **Create `ui/lvgl/ZorkUI/` directory**
3. **Write `CMakeLists.txt`** that:
   - `include()`s `cmake/FizmoCommon.cmake`
   - Adds LVGL sources
   - Calls `fizmo_common_apply()` and `fizmo_apply_display_profile()`
   - Links `${FIZMO_COMMON_SOURCES}` + interpreter sources
4. **Create `lv_conf.h`** with project-specific settings:
   - Enable `LV_USE_FREETYPE` or `LV_USE_TINY_TTF`
   - Enable `LV_USE_TEXTAREA`, `LV_USE_KEYBOARD`, `LV_USE_LABEL`
   - Tune `LV_MEM_SIZE` per platform

### Phase 2 — LVGL Backend (thin layer)

Create a small glue module that:
- Holds a `FizmoTextBuffer` and `FizmoPollerState` (from ui/common)
- Creates an `lv_timer` (50ms) that calls `fizmo_poller_poll()`
- On `FIZMO_CHANGED_OUTPUT`: calls `lv_label_set_text()` on output label
- On `FIZMO_CHANGED_STATUS`: updates status labels
- On `FIZMO_CHANGED_INPUT`: shows/hides input bar and keyboard
- On input submit: calls `fizmo_poller_submit_line()`

This is much simpler than Phase 2 of the original plan because the heavy
lifting (buffer management, polling, UTF conversion) already lives in
ui/common.

### Phase 3 — LVGL Widgets

#### Screen Layout

```
┌──────────────────────────────────┐
│  Status Bar  (lv_obj + 2 labels) │  ← room name (left), score (right)
├──────────────────────────────────┤
│                          ┌──────┐│
│  Output Text Area        │Compas││  ← lv_label in scrollable container
│  (scrollable)            │ Rose ││
│                          └──────┘│
├──────────────────────────────────┤
│  Input Bar  ("> " + lv_textarea) │  ← single-line text input
├──────────────────────────────────┤
│  Keyboard (lv_keyboard)         │  ← LVGL built-in keyboard widget
└──────────────────────────────────┘
```

#### Widget Mapping

| Current (QML) | LVGL Replacement | Notes |
|----------------|-----------------|-------|
| Status bar `Rectangle` + `Text` | `lv_obj` + 2× `lv_label` | Flex row, space-between |
| Output `Flickable` + `Text` | Scrollable `lv_obj` + `lv_label` | `LV_LABEL_LONG_WRAP`, auto-scroll |
| Compass rose | Custom `lv_obj` + click event | Same polar-coordinate math |
| `TextInput` | `lv_textarea` (single line) | `LV_EVENT_READY` for enter |
| `VirtualKeyboard` / `ZorkKeyboard` | `lv_keyboard` | Attach via `lv_keyboard_set_textarea()` |
| Color theme | `lv_style_t` objects | #1a1a2e bg, #00ff88 text, etc. |

#### Compass Rose

- `lv_image` with click callback
- Reuse polar-coordinate → 8-direction math from QML
- Call `fizmo_poller_submit_line()` with direction string

#### Fonts

- Desktop/RT1170: `LV_USE_FREETYPE` or `LV_USE_TINY_TTF` with same TTF
- RT1050: Pre-generated `lv_font_t` bitmaps (saves flash)

### Phase 4 — Platform Drivers

#### Desktop (SDL)
- LVGL v9 built-in SDL driver (`lv_sdl_window_create()`)

#### NXP RT1170-EVKB
- `lv_display_create()` with LCDIF flush callback
- `lv_indev_create(LV_INDEV_TYPE_POINTER)` with FT5406 I2C touch

#### NXP RT1050-EVK
- Partial rendering (`LV_DISPLAY_RENDER_MODE_PARTIAL`) to save RAM
- Bitmap fonts, tuned `LV_MEM_SIZE`

### Phase 5 — Threading

```
FreeRTOS:
  Task 1: LVGL task (lv_timer_handler() loop, ~5ms)
           └─ 50ms lv_timer polls fizmo via FizmoPoller
  Task 2: Fizmo task (blocks on input, produces output via queue)

Desktop:
  Main thread: LVGL + SDL event loop
  Worker thread: fizmo interpreter (std::thread)
```

Communication uses the same FreeRTOS queues/semaphores as today.

### Phase 6 — Save/Restore

No changes needed — `fizmo_filesys_hybrid.c` is toolkit-independent.

### Phase 7 — Testing & Polish

1. Desktop SDL build first
2. RT1170 hardware
3. RT1050 hardware
4. Visual polish (match color scheme)
5. Performance profiling

---

## Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| LVGL text rendering quality | Test with same TTF, tune hinting |
| RT1050 RAM too tight | Partial rendering, bitmap fonts, reduce `LV_MEM_SIZE` |
| Touch calibration | Reuse NXP SDK touch drivers, calibrate in LVGL indev |
| Scrolling performance | Keep buffer trimmed (FizmoTextBuffer does this), use `lv_label` not `lv_textarea` for read-only text |
| LVGL keyboard missing keys | Customize `lv_keyboard` map or build custom keyboard widget |
