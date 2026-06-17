# Zork — Slint UI (spike)

Third peer UI stack (Slint 1.16.1, C++ binding) for the ZorkForMCUs bake-off,
alongside `ui/qul` (Qt for MCUs) and `ui/lvgl` (LVGL). Targets NXP RT1050/RT1170;
developed desktop-first.

## Status
Integration spike — the text round-trip is playable: the game's output renders in
a scrolling transcript and typed commands are sent to the interpreter. The three
display profiles (RT1050/RT1170/RT1170_SCALED) are also wired up. Not yet
implemented (future phases): status bar, compass rose, on-screen keyboard, and
the hardware/FreeRTOS port. See
`docs/superpowers/specs/2026-06-15-slint-ui-integration-spike-design.md`.

## Build (Windows, MSVC / ARM64)
This is a native ARM64 build using Slint's prebuilt MSVC package (no Rust).

1. Install the Slint C++ ARM64 package into `ui/slint/slint-install/`:
   `Slint-cpp-1.16.1-win64-MSVC-ARM64.exe /S /D=<abs path>\ui\slint\slint-install`
   (download from https://github.com/slint-ui/slint/releases/tag/v1.16.1)
2. From a shell with MSVC seeded for arm64 (`vcvarsall.bat arm64`):

   ```powershell
   cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build ui/slint/build --target ZorkSlint
   ./ui/slint/build/ZorkSlint.exe
   ```

   The Slint runtime DLL is copied next to the exe automatically (POST_BUILD).

   Select a display profile with `-DDISPLAY_PROFILE=RT1050|RT1170|RT1170_SCALED`
   (default `RT1170_SCALED`, 540×960; RT1050 is 480×272, RT1170 is 720×1280).

## Tests

`ctest --test-dir ui/slint/build` runs the UTF-32→UTF-8 encoder unit test.

## Notes / known issues

- **Font:** the transcript uses Cascadia Code referenced from the system
  (`SLINT_FONT_PATH`, default `C:/Windows/Fonts/CascadiaCode.ttf`); it is not
  bundled. Override at configure time with `-DSLINT_FONT_PATH=...` if needed.
- **Excess `>` prompts:** the UI echoes the typed command as `>command` while the
  interpreter also emits its own `>` prompt, so two angle brackets can appear.
  This is cosmetic and also occurs in the original QUL stack (not Slint-specific).
- **Shutdown:** `fizmo_bridge_shutdown()` deadlocks (the worker thread is blocked
  in the interpreter's read_line loop), so the app exits via `std::_Exit(0)` when
  the window closes.
- **libfizmo is a pinned upstream submodule and is never modified.** Its two
  MSVC-incompatible VLAs are neutralized by CMake-generated patched copies at
  configure time (see the `patch_vla` function in CMakeLists.txt).
