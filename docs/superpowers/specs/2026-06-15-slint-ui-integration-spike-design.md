# Slint UI — Minimal Integration Spike (Design)

**Date:** 2026-06-15
**Branch target:** new feature branch off `main` (peer to the `lvgl` branch)
**Status:** Approved design — pending implementation plan

## Context

ZorkForMCUs is a bake-off proof-of-concept: a Zork I Z-machine interpreter
(fizmo) driven by multiple embedded/MCU UI kits, presented side by side. Two
stacks exist today:

- `ui/qul/` — Qt for MCUs (C++, on `main`)
- `ui/lvgl/` — LVGL v9 (C, on the `lvgl` branch)

This effort adds a **third peer stack, Slint**, targeting the same NXP hardware
(i.MX RT1050 and RT1170 kits). The simultaneous presentation of multiple
implementations is the product; Slint is not replacing anything.

Every UI stack integrates through the same UI-framework-agnostic C bridge,
[`src/fizmo_bridge.h`](../../../src/fizmo_bridge.h):

- `fizmo_bridge_init(story_path)` / `fizmo_start_interpreter()` / `fizmo_bridge_shutdown()`
- `fizmo_output_available()` / `fizmo_output_read()` — poll UTF-32 (`z_ucs`) output
- `fizmo_waiting_for_input()` / `fizmo_waiting_for_char()` / `fizmo_has_exited()`
- `fizmo_get_status_line()` — room / score
- `fizmo_submit_line()` / `fizmo_submit_char()` — user input

LVGL's desktop build (see [`ui/lvgl/CMakeLists.txt`](../../../ui/lvgl/CMakeLists.txt))
sets the template: compile the libfizmo interpreter sources + `fizmo_bridge.cpp`
+ `fizmo_locale_stubs.c`, bring the UI kit's own renderer, fonts, and assets,
and provide a desktop entry point for the simulator.

## Decisions (locked during brainstorming)

| Decision | Choice | Rationale |
|---|---|---|
| Goal / investment | Peer bake-off stack; hardware in scope; **desktop-first** dev | Repo's purpose is side-by-side MCU UI-kit comparison |
| Language binding | **C++** (not Rust) | Uniform CMake/Ninja build; calls the C bridge directly; isolates the bake-off variable to the UI kit, not the language/toolchain (QUL=C++, LVGL=C) |
| First milestone scope | **Minimal integration spike** | Prove the Slint↔fizmo round-trip first, decide the rest after seeing it run (YAGNI) |
| Slint dependency | **FetchContent prebuilt binary** now; convert to submodule + from-source build later (when moving to the board) | No Rust toolchain needed for the desktop spike; fast first build |
| Skeleton depth | **Structured-minimal** (approach B) | Lay down a `ui/slint/` skeleton echoing the LVGL layout, implement only the text round-trip, so later phases drop in with no rework |

## Goals

The spike succeeds when the Slint↔fizmo integration is proven: text out, line
in, round-trip, in a Slint window, with **no Rust toolchain installed**.

### In scope
- Scrolling text output (accumulated game transcript)
- Single-line text input submitted on Enter
- Window fixed at the default display profile (RT1170_SCALED, 540×960)
- Cascadia Code font for parity with the other stacks
- Existing dark-blue / teal color scheme

### Explicitly out of scope (deferred to later phases)
- Status bar (room / score) — the obvious next increment
- Compass rose image
- On-screen touch keyboard
- The three-profile CMake switch (RT1050 / RT1170 / RT1170_SCALED)
- Hardware / FreeRTOS port and from-source (Rust) Slint build
- Theming polish beyond basic colors/font

## Architecture

### Directory layout (`ui/slint/`)

```
ui/slint/
  CMakeLists.txt
  app/zork.slint                  # the UI markup
  app/main.cpp                    # entry point + Slint event loop + bridge wiring
  app/fizmo_slint_bridge.h/.cpp   # drains bridge output, UTF-32 z_ucs -> UTF-8
  fonts/CascadiaCode.ttf          # (or reuse the TTF already in the repo)
```

Target name: **`ZorkSlint`**. The `app/` (glue) plus markup split mirrors
`ui/lvgl/`'s adapter/app structure so M2+ features slot in naturally.

### Units and responsibilities

- **`zork.slint`** — defines one `ZorkWindow` component. *What it does:* renders
  the transcript and captures a line of input. *Interface:*
  `in property <string> transcript;` and `callback submit(string);`.
  *Depends on:* nothing (pure markup; the Slint compiler generates a C++ header).
  - A `ScrollView` containing a word-wrapped monospace `Text` bound to
    `transcript`, auto-scrolling to the bottom on update.
  - A `LineEdit` pinned at the bottom; on Enter fires `submit(text)` and clears.
  - Fixed window 540×960; colors from the existing dark-blue/teal scheme.

- **`fizmo_slint_bridge` (C++)** — *What it does:* owns the UTF-32 (`z_ucs`) →
  UTF-8 (`slint::SharedString`) conversion and the drain-output helper.
  *Interface:* a small function that reads all available bridge output and
  returns it as a UTF-8 string (or appends to a running buffer). *Depends on:*
  `fizmo_bridge.h`. Conversion logic ported from LVGL's
  [`ui/lvgl/app/zork_output.c`](../../../ui/lvgl/app/zork_output.c).

- **`main.cpp`** — *What it does:* wires everything and runs the event loop.
  *Depends on:* the generated Slint header, `fizmo_bridge.h`,
  `fizmo_slint_bridge`.
  1. `fizmo_bridge_init(ZORK_STORY_PATH)` → `fizmo_start_interpreter()`.
  2. Instantiate `ZorkWindow`.
  3. Wire `submit` callback → echo the typed line into `transcript`, then
     `fizmo_submit_line(line.c_str())`.
  4. `slint::Timer` (~30 ms, UI thread) → drain bridge output via
     `fizmo_slint_bridge`, append to `transcript` (auto-scroll follows).
  5. `window->run()`; `fizmo_bridge_shutdown()` on exit.

### Data flow

```
fizmo interpreter (background thread, in fizmo_bridge.cpp)
        │  UTF-32 z_ucs output
        ▼
fizmo_output_read()  ──drained by──▶  slint::Timer (UI thread, ~30ms)
        │                                   │ UTF-32 -> UTF-8
        │                                   ▼
        │                            transcript property ──▶ ScrollView/Text
        ▼
fizmo_submit_line()  ◀──submit() callback──  LineEdit (Enter)
```

The fizmo bridge already runs the interpreter on its own thread and exposes a
thread-safe poll/submit API, so all Slint-side work stays on the UI thread via
the timer — no additional threading or locking introduced by this stack.

## Build integration

- `FetchContent_Declare(Slint URL <prebuilt slint-cpp Windows x86_64 tarball,
  version pinned during implementation>)` → `FetchContent_MakeAvailable(Slint)`
  → `find_package(Slint)`. This uses Slint's **prebuilt binary package**, so
  **no Rust toolchain** is required. (The GIT/SOURCE_SUBDIR FetchContent method
  builds from source and *does* need Rust — not used here.)
- `target_link_libraries(ZorkSlint PRIVATE Slint::Slint)`
- `slint_target_sources(ZorkSlint app/zork.slint)` — compiles the `.slint` file
  and makes the generated header available to the target.
- `set_target_properties(ZorkSlint PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)`
  — Slint requires **C++20** (repo default elsewhere is C++17, so this is set
  per-target).
- Compile the same libfizmo source list + `fizmo_bridge.cpp` +
  `fizmo_locale_stubs.c` as LVGL's desktop block, with the same `DISABLE_*`
  defines and `ZORK_STORY_PATH` pointing at the repo's `zork1.z3`.
- **No SDL2** — Slint's default desktop backend provides the window.
- CMake ≥ 3.21.

## Fonts

Bundle **CascadiaCode.ttf** via Slint's font embedding and reference it by
`font-family` in `zork.slint`. The exact bundling mechanism is confirmed during
implementation; if it proves fiddly for the spike, fall back to a monospace
system family (parity polish is a later phase). This sidesteps the
`lv_font_conv` compressed-format issues encountered with LVGL.

## Error handling

- `fizmo_bridge_init` / `fizmo_start_interpreter` failure (returns -1): log to
  stderr and exit non-zero before creating the window — there is no game to show.
- `fizmo_has_exited()` observed in the timer: append any final output, then the
  window may stay open (transcript remains readable) or close — spike behavior,
  decided in implementation; not load-bearing.
- UTF-32 → UTF-8 conversion handles the full `z_ucs` range; unmappable values
  are skipped rather than crashing.

## Testing

Manual, consistent with the repo (no automated framework yet).

**Acceptance criteria:**
1. `ZorkSlint` configures and builds with **no Rust installed**.
2. Launches and renders the Zork opening text ("West of House… There is a small
   mailbox here.").
3. Typing `open mailbox` + Enter shows the game's response in the transcript.

That round-trip proves the Slint↔fizmo integration — the entire point of the
spike.

## Risks / unknowns (resolved during implementation)

- Exact prebuilt-tarball URL and Slint version to pin.
- Slint font-bundling specifics for Cascadia Code.
- Auto-scroll-to-bottom idiom in Slint's `ScrollView`.

## Addendum — planning refinements (2026-06-15)

Two details discovered while writing the implementation plan refine, but do not
change the intent of, the decisions above:

1. **Windows acquisition is an installer, not a tarball.** Slint 1.16.1 ships its
   Windows C++ package only as an NSIS installer — there is no Windows
   `.tar.gz`/`.zip` of the C++ package. This is an **ARM64** machine, so the
   relevant package is `Slint-cpp-1.16.1-win64-MSVC-ARM64.exe`. The no-Rust path
   is: silent-install to a repo-local directory, then `find_package(Slint)` via
   `CMAKE_PREFIX_PATH` (not the `FetchContent(URL → extract)` mechanism sketched
   earlier, which only applies to the Linux tarballs). Intent (prebuilt binary,
   no Rust) is preserved.

2. **Toolchain: MSVC native (ARM64), with a MinGW fallback.** The Windows
   prebuilt is MSVC-built, while libfizmo has only ever compiled under GCC (the
   existing QUL and LVGL desktop sims use GCC-style toolchains). `ZorkSlint`
   therefore builds under **MSVC** (seeded via `vcvarsall.bat arm64`) to match the
   prebuilt. libfizmo's POSIX stubs are gated on
   `__ARM_EABI__` and inactive on desktop, so MSVC will need a small desktop
   compatibility shim (missing `unistd.h`/`dirent.h`, `strcasecmp`,
   `getuid`/`getpwuid`, etc.). The plan opens with a de-risk task that compiles
   libfizmo + the bridge under MSVC and runs a headless smoke test before any UI
   work. **Bail criterion:** if MSVC compatibility is not converging within ~1–2
   iterations, switch to MinGW + building Slint from source (accepts a Rust
   toolchain, keeps GCC for libfizmo).

## Later phases (not part of this spike)

- M2: status bar (room/score), compass rose image, on-screen touch keyboard.
- Three display profiles (RT1050 / RT1170 / RT1170_SCALED) via CMake switch.
  **Implemented post-spike — see the update below.**
- Convert Slint dependency to a submodule + from-source build.
- Hardware / FreeRTOS port using Slint's MCU software renderer.

## Addendum — post-spike work (2026-06-17)

The "Explicitly out of scope" list above describes the integration spike as
designed. Beyond it, the following shipped on the `slint-display-profiles`
branch (PR #2) and are therefore no longer pending:

- **Display profiles:** the three-profile `DISPLAY_PROFILE` CMake switch (RT1050
  480×272 / RT1170 720×1280 / RT1170_SCALED 540×960 default), driving the window
  size via `ZORK_WIN_W`/`ZORK_WIN_H` compile defs and `ZorkWindow`
  `win-width`/`win-height` properties.
- **Transcript scroll fix:** the RT1050 window exposed that a bare `Text` child
  leaves a `ScrollView`'s `viewport-height` stuck at `visible-height` (nothing
  scrolls). Fixed by sizing `viewport-height` to the text's content height plus a
  polled stick-to-bottom auto-scroll (`Timer` + `scrolled()`).
