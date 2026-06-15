# Slint UI Integration Spike — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove the Slint↔fizmo round-trip — render Zork's text output and submit typed commands — in a native-Windows Slint (C++) window built with MSVC, as a third peer UI stack under `ui/slint/`.

**Architecture:** A static library compiles the libfizmo interpreter + the existing C bridge ([`src/fizmo_bridge.h`](../../../src/fizmo_bridge.h)) under MSVC. A small Slint C++ app (`ZorkSlint`) owns a `ZorkWindow` (`.slint` markup): a scrolling transcript bound to a string property and a `LineEdit` for input. On the UI thread a `slint::Timer` polls the bridge, converts UTF-32 → UTF-8, and appends to the transcript; the `LineEdit`'s `accepted` callback submits the line back to fizmo.

**Tech Stack:** Slint 1.16.1 (C++ binding, MSVC prebuilt), CMake ≥ 3.21 + Ninja, MSVC (cl.exe, C++20), libfizmo (C), the existing `fizmo_bridge.cpp` (std::thread).

**Spec:** [`docs/superpowers/specs/2026-06-15-slint-ui-integration-spike-design.md`](../specs/2026-06-15-slint-ui-integration-spike-design.md)

**Branch:** `slint` (already created off `main`).

---

## Conventions for every task

- **Build environment:** all `cmake`/`ninja`/`cl` commands must run from a shell where MSVC is on `PATH` — a "Developer PowerShell for VS 2022" (or after running `vcvars64.bat`). Verify once with `cl` (should print the MSVC version banner).
- **Configure command** (from repo root, unless a task says otherwise):
  ```
  cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  ```
- **Build command:**
  ```
  cmake --build ui/slint/build
  ```
- **Run** produces `ui/slint/build/<target>.exe`.
- Commit after each task with the message shown in its final step.

---

## File structure

| Path | Responsibility |
|---|---|
| `ui/slint/CMakeLists.txt` | Build: libfizmo static lib (MSVC), Slint app, smoke + unit-test targets |
| `src/fizmo_msvc_compat.h` | **New shared file.** MSVC-only desktop POSIX shim (force-included for this build) |
| `src/msvc_shims/unistd.h`, `strings.h`, `dirent.h` | **New.** Header shims placed on the include path only for the MSVC build |
| `ui/slint/app/zork.slint` | `ZorkWindow` markup — transcript `ScrollView`/`Text` + input `LineEdit` |
| `ui/slint/app/utf8.h` / `utf8.cpp` | Pure UTF-32 `z_ucs` → UTF-8 encoder (unit-tested) |
| `ui/slint/app/fizmo_slint_bridge.h` / `.cpp` | `zork_drain_output()` — drain bridge output as UTF-8 |
| `ui/slint/app/main.cpp` | Entry point: init fizmo, build window, poll timer, wire input, run loop |
| `ui/slint/app/smoke_main.c` | Task 1 headless smoke test (deleted at end of Task 1) |
| `ui/slint/test/test_utf8.cpp` | Unit test for `zucs_to_utf8` |
| `ui/slint/fonts/CascadiaCode.ttf` | Bundled monospace font (best-effort parity, Task 6) |
| `ui/slint/.gitignore` | Ignore `build/` and the local Slint install |

---

## Task 1: De-risk — compile libfizmo + bridge under MSVC and run it headless

**Goal of this task:** find out *immediately* whether libfizmo runs under MSVC. No Slint yet. If this does not converge in ~1–2 iterations of adding shims, **STOP and switch to the MinGW fallback** (see the bail note at the end of this task).

**Files:**
- Create: `ui/slint/CMakeLists.txt`
- Create: `ui/slint/.gitignore`
- Create: `ui/slint/app/smoke_main.c`
- Create (likely, applied as errors demand): `src/fizmo_msvc_compat.h`, `src/msvc_shims/unistd.h`, `src/msvc_shims/strings.h`, `src/msvc_shims/dirent.h`

- [ ] **Step 1: Create `ui/slint/.gitignore`**

```gitignore
build/
slint-install/
```

- [ ] **Step 2: Create the initial `ui/slint/CMakeLists.txt` (libfizmo static lib + smoke exe)**

```cmake
cmake_minimum_required(VERSION 3.21)
project(ZorkSlint C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# ---------------------------------------------------------------------------
# libfizmo interpreter + desktop bridge, compiled as a static library (MSVC).
# Source list mirrors the QUL desktop (BUILD_WITH_FIZMO) build.
# ---------------------------------------------------------------------------
set(LIBFIZMO_DIR "${PROJECT_ROOT}/external/libfizmo/src")
set(LIBFIZMO_SOURCES
    "${LIBFIZMO_DIR}/interpreter/blockbuf.c"
    "${LIBFIZMO_DIR}/interpreter/config.c"
    "${LIBFIZMO_DIR}/interpreter/fizmo.c"
    "${LIBFIZMO_DIR}/interpreter/mathemat.c"
    "${LIBFIZMO_DIR}/interpreter/misc.c"
    "${LIBFIZMO_DIR}/interpreter/mt19937ar.c"
    "${LIBFIZMO_DIR}/interpreter/object.c"
    "${LIBFIZMO_DIR}/interpreter/output.c"
    "${LIBFIZMO_DIR}/interpreter/property.c"
    "${LIBFIZMO_DIR}/interpreter/routine.c"
    "${LIBFIZMO_DIR}/interpreter/savegame.c"
    "${LIBFIZMO_DIR}/interpreter/sound.c"
    "${LIBFIZMO_DIR}/interpreter/stack.c"
    "${LIBFIZMO_DIR}/interpreter/streams.c"
    "${LIBFIZMO_DIR}/interpreter/table.c"
    "${LIBFIZMO_DIR}/interpreter/text.c"
    "${LIBFIZMO_DIR}/interpreter/undo.c"
    "${LIBFIZMO_DIR}/interpreter/variable.c"
    "${LIBFIZMO_DIR}/interpreter/wordwrap.c"
    "${LIBFIZMO_DIR}/interpreter/zpu.c"
    "${LIBFIZMO_DIR}/interpreter/iff.c"
    "${LIBFIZMO_DIR}/tools/filesys.c"
    "${LIBFIZMO_DIR}/tools/filesys_c.c"
    "${LIBFIZMO_DIR}/tools/i18n.c"
    "${LIBFIZMO_DIR}/tools/list.c"
    "${LIBFIZMO_DIR}/tools/stringmap.c"
    "${LIBFIZMO_DIR}/tools/tracelog.c"
    "${LIBFIZMO_DIR}/tools/types.c"
    "${LIBFIZMO_DIR}/tools/z_ucs.c"
    "${PROJECT_ROOT}/src/fizmo_locale_stubs.c"
    "${PROJECT_ROOT}/src/fizmo_bridge.cpp"
)

add_library(zorkfizmo STATIC ${LIBFIZMO_SOURCES})

# src first so our shims/overrides win; then libfizmo headers.
target_include_directories(zorkfizmo BEFORE PRIVATE
    "${PROJECT_ROOT}/src/msvc_shims"   # MSVC-only header shims (unistd.h, etc.)
    "${PROJECT_ROOT}/src"
)
target_include_directories(zorkfizmo PRIVATE
    "${LIBFIZMO_DIR}"
)

target_compile_definitions(zorkfizmo PRIVATE
    USE_FIZMO_BRIDGE=1
    DISABLE_BABEL=1
    DISABLE_FILELIST=1
    DISABLE_CONFIGFILES=1
    DISABLE_COMMAND_HISTORY=1
    DISABLE_OUTPUT_HISTORY=1
    DISABLE_PREFIX_COMMANDS=1
    DISABLE_BLOCKBUFFER=1
    _CRT_SECURE_NO_WARNINGS=1
    ZORK_STORY_PATH="${PROJECT_ROOT}/zork1.z3"
)

# Force-include the MSVC desktop compatibility header (cl.exe uses /FI).
target_compile_options(zorkfizmo PRIVATE
    "/FI${PROJECT_ROOT}/src/fizmo_msvc_compat.h"
)

find_package(Threads REQUIRED)
target_link_libraries(zorkfizmo PRIVATE Threads::Threads)

# ---------------------------------------------------------------------------
# Headless smoke test: prove the interpreter runs under MSVC (no UI).
# ---------------------------------------------------------------------------
add_executable(zork_smoke app/smoke_main.c)
target_include_directories(zork_smoke PRIVATE "${PROJECT_ROOT}/src")
target_link_libraries(zork_smoke PRIVATE zorkfizmo Threads::Threads)
```

- [ ] **Step 3: Create the headless smoke test `ui/slint/app/smoke_main.c`**

```c
/*
 * smoke_main.c - Headless de-risk test for libfizmo under MSVC.
 * Starts the interpreter, polls a little output, prints it, and exits.
 * Success = the Zork opening text appears on stdout.
 */
#include "fizmo_bridge.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

int main(void) {
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

    /* Poll for up to ~3 seconds for the opening text. */
    uint32_t buf[512];
    int printed = 0;
    for (int i = 0; i < 300; i++) {
        size_t n = fizmo_output_read(buf, 512);
        for (size_t k = 0; k < n; k++) {
            /* ASCII-only dump is fine for the smoke check. */
            putchar(buf[k] < 128 ? (int)buf[k] : '?');
            printed = 1;
        }
        if (printed && fizmo_waiting_for_input()) break;
        Sleep(10);
    }

    fizmo_bridge_shutdown();
    return printed ? 0 : 2;
}
```

- [ ] **Step 4: First configure + build attempt — capture the errors**

Run:
```
cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/slint/build --target zork_smoke
```
Expected: this will **likely fail** on MSVC with missing POSIX headers/symbols
(`unistd.h`, `dirent.h`, `strings.h`/`strcasecmp`, `getuid`/`getpwuid`,
`sys/time.h`/`gettimeofday`). That is the point of this step — read the actual
errors. Proceed to Step 5 to apply shims for the errors you see.

- [ ] **Step 5: Create the MSVC compatibility header `src/fizmo_msvc_compat.h`**

This is force-included into every libfizmo TU for this build. It provides the
desktop-POSIX pieces MSVC lacks (the existing `posix_stubs.c` is `__ARM_EABI__`-only
and inactive here).

```c
/*
 * fizmo_msvc_compat.h
 *
 * Force-included (cl /FI) compatibility shim for building libfizmo + the
 * desktop bridge under MSVC. Only active for MSVC (_MSC_VER); a no-op
 * elsewhere so the file is safe to share.
 *
 * Companion header shims live in src/msvc_shims/ (unistd.h, strings.h,
 * dirent.h) and are reached via the include path, not from here.
 */
#ifndef FIZMO_MSVC_COMPAT_H
#define FIZMO_MSVC_COMPAT_H

#ifdef _MSC_VER

#include <string.h>
#include <stdlib.h>

/* BSD string comparisons -> MSVC intrinsics. */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/* POSIX types/functions libfizmo references on "unix" desktop builds.
 * With DISABLE_CONFIGFILES=1 these are compiled out of the hot paths, but
 * the declarations may still be needed to satisfy the compiler/linker. */
typedef int uid_t;

static inline uid_t getuid(void) { return 0; }

struct passwd {
    char *pw_name;
    char *pw_dir;
};
static inline struct passwd *getpwuid(uid_t uid) {
    (void)uid;
    static struct passwd pw = { "windows", 0 };
    return &pw;
}

#endif /* _MSC_VER */
#endif /* FIZMO_MSVC_COMPAT_H */
```

- [ ] **Step 6: Create the header shims under `src/msvc_shims/`**

These satisfy `#include <unistd.h>`, `<strings.h>`, `<dirent.h>` on MSVC. They
are on the include path *before* `src/`, and only this MSVC build uses that path.

`src/msvc_shims/unistd.h`:
```c
#ifndef MSVC_SHIM_UNISTD_H
#define MSVC_SHIM_UNISTD_H
/* MSVC has no <unistd.h>. Map the few pieces libfizmo touches. */
#include <io.h>
#include <process.h>
#include <direct.h>
#endif
```

`src/msvc_shims/strings.h`:
```c
#ifndef MSVC_SHIM_STRINGS_H
#define MSVC_SHIM_STRINGS_H
#include <string.h>
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#endif
```

`src/msvc_shims/dirent.h`:
```c
#ifndef MSVC_SHIM_DIRENT_H
#define MSVC_SHIM_DIRENT_H
/* Directory enumeration is unused when DISABLE_CONFIGFILES=1. Minimal types
 * to satisfy compilation only. */
typedef struct { void *__opaque; } DIR;
struct dirent { char *d_name; };
DIR *opendir(const char *name);
int closedir(DIR *d);
struct dirent *readdir(DIR *d);
void rewinddir(DIR *d);
#endif
```

- [ ] **Step 7: Rebuild after applying shims**

Run:
```
cmake --build ui/slint/build --target zork_smoke
```
Expected: compiles and links. If new missing symbols appear (e.g.
`gettimeofday`, `basename`), add the minimal equivalent to
`src/fizmo_msvc_compat.h` (e.g. a `gettimeofday` using `GetSystemTimeAsFileTime`,
or a `basename` that scans for the last `\\` or `/`) and rebuild. **Keep each
addition tiny and specific to an actual linker/compiler error.**

> **BAIL CRITERION (Brendan's guardrail):** If after ~1–2 rounds of shimming the
> interpreter still won't build/run under MSVC (e.g. VLAs or other hard MSVC-C
> incompatibilities in libfizmo), STOP. Switch to the MinGW fallback: build
> `ZorkSlint` with MinGW-w64 GCC (libfizmo's existing GCC path works as-is) and
> obtain Slint by building from source (`FetchContent` GIT method,
> `SOURCE_SUBDIR api/cpp`), which requires installing a Rust toolchain. Record the
> switch in the spec's Addendum and re-plan Task 2 onward for MinGW. Do not keep
> grinding on MSVC past two iterations.

- [ ] **Step 8: Run the smoke test**

Run:
```
./ui/slint/build/zork_smoke.exe
```
Expected: exit code 0 and stdout containing the opening text, e.g.:
```
ZORK I: The Great Underground Empire
... West of House ... There is a small mailbox here.
```
If you see that text, libfizmo runs under MSVC and the spike is unblocked.

- [ ] **Step 9: Commit**

```
git add ui/slint/CMakeLists.txt ui/slint/.gitignore ui/slint/app/smoke_main.c \
        src/fizmo_msvc_compat.h src/msvc_shims/
git commit -m "Build libfizmo + bridge under MSVC; headless smoke test passes"
```

---

## Task 2: Acquire Slint and show an empty `ZorkWindow`

**Goal:** prove the Slint MSVC toolchain end-to-end — `find_package(Slint)`,
`.slint` codegen, and a window appears. Still no game wiring.

**Files:**
- Modify: `ui/slint/CMakeLists.txt` (append the Slint app block)
- Create: `ui/slint/app/zork.slint`
- Create: `ui/slint/app/main.cpp`

- [ ] **Step 1: Download and silently install the Slint C++ MSVC package**

From the repo root (the installer is NSIS: `/S` = silent, `/D=` = install dir,
**must be the last argument, absolute, unquoted, backslashes**):
```
curl -L -o slint-cpp-setup.exe \
  https://github.com/slint-ui/slint/releases/download/v1.16.1/Slint-cpp-1.16.1-win64-MSVC-AMD64.exe
cmd //c "slint-cpp-setup.exe /S /D=C:\Users\brend\src\ZorkForMCUs\ui\slint\slint-install"
```
Then verify the CMake package landed (path may be `lib/cmake/Slint` —
adjust the next step if the installed tree differs):
```
ls ui/slint/slint-install/lib/cmake/Slint
```
Expected: a `SlintConfig.cmake` (or `Slint-config.cmake`). Delete the installer:
`rm slint-cpp-setup.exe`. (`slint-install/` is already gitignored from Task 1.)

- [ ] **Step 2: Append the Slint app block to `ui/slint/CMakeLists.txt`**

Add at the end of the file:
```cmake
# ---------------------------------------------------------------------------
# Slint C++ (prebuilt MSVC package) + the ZorkSlint application.
# ---------------------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/slint-install")
find_package(Slint REQUIRED)

add_executable(ZorkSlint WIN32
    app/main.cpp
)
target_include_directories(ZorkSlint PRIVATE
    "${PROJECT_ROOT}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/app"
)
target_link_libraries(ZorkSlint PRIVATE zorkfizmo Slint::Slint Threads::Threads)
slint_target_sources(ZorkSlint app/zork.slint)
```
Note: `WIN32` makes it a GUI app (no console window). The generated header for
`app/zork.slint` is `zork.h`; `slint_target_sources` adds the include path
automatically.

- [ ] **Step 3: Create `ui/slint/app/zork.slint` (empty window for now)**

```slint
export component ZorkWindow inherits Window {
    title: "Zork — Slint";
    width: 540px;
    height: 960px;
    background: #06141f;
}
```

- [ ] **Step 4: Create `ui/slint/app/main.cpp` (create + run the window)**

```cpp
#include "zork.h"   // generated from app/zork.slint

int main() {
    auto ui = ZorkWindow::create();
    ui->run();
    return 0;
}
```

- [ ] **Step 5: Configure, build, and run**

Run:
```
cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/slint/build --target ZorkSlint
./ui/slint/build/ZorkSlint.exe
```
Expected: a 540×960 dark-blue window titled "Zork — Slint" appears. Close it to exit.

- [ ] **Step 6: Commit**

```
git add ui/slint/CMakeLists.txt ui/slint/app/zork.slint ui/slint/app/main.cpp
git commit -m "Render empty Slint ZorkWindow (proves Slint MSVC toolchain)"
```

---

## Task 3: UTF-32 → UTF-8 encoder (TDD)

**Goal:** the one piece of pure logic worth a real unit test — the `z_ucs`
(UTF-32) → UTF-8 conversion — written test-first.

**Files:**
- Create: `ui/slint/app/utf8.h`, `ui/slint/app/utf8.cpp`
- Create: `ui/slint/test/test_utf8.cpp`
- Modify: `ui/slint/CMakeLists.txt` (add the test target)

- [ ] **Step 1: Write the failing test `ui/slint/test/test_utf8.cpp`**

```cpp
#include "utf8.h"
#include <cassert>
#include <string>
#include <cstdio>

int main() {
    std::string s;

    s.clear(); zucs_to_utf8('A', s);            // 1-byte ASCII
    assert(s == "A");

    s.clear(); zucs_to_utf8(0x00E9u, s);        // é  -> C3 A9
    assert(s == "\xC3\xA9");

    s.clear(); zucs_to_utf8(0x2022u, s);        // •  -> E2 80 A2
    assert(s == "\xE2\x80\xA2");

    s.clear(); zucs_to_utf8(0x1F600u, s);       // 😀 -> F0 9F 98 80
    assert(s == "\xF0\x9F\x98\x80");

    s.clear(); zucs_to_utf8(0x110000u, s);      // out of range -> skipped
    assert(s.empty());

    std::printf("test_utf8: all assertions passed\n");
    return 0;
}
```

- [ ] **Step 2: Create the header `ui/slint/app/utf8.h`**

```cpp
#ifndef ZORK_UTF8_H
#define ZORK_UTF8_H

#include <cstdint>
#include <string>

// Append the UTF-8 encoding of a single Unicode code point to `out`.
// Code points outside the valid range (> U+10FFFF) are skipped.
void zucs_to_utf8(std::uint32_t cp, std::string &out);

#endif
```

- [ ] **Step 3: Add the test target to `ui/slint/CMakeLists.txt`**

Append at the end:
```cmake
# ---------------------------------------------------------------------------
# Unit test: UTF-32 -> UTF-8 encoder (pure, no fizmo/Slint dependency).
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_utf8 test/test_utf8.cpp app/utf8.cpp)
target_include_directories(test_utf8 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/app")
add_test(NAME test_utf8 COMMAND test_utf8)
```

- [ ] **Step 4: Run the test to verify it FAILS (no implementation yet)**

Run:
```
cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/slint/build --target test_utf8
```
Expected: **link error** — `unresolved external symbol zucs_to_utf8` (the `.cpp`
doesn't exist yet). That is the failing state.

- [ ] **Step 5: Implement `ui/slint/app/utf8.cpp`**

```cpp
#include "utf8.h"

void zucs_to_utf8(std::uint32_t cp, std::string &out) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
    // else: invalid code point, skip.
}
```

- [ ] **Step 6: Run the test to verify it PASSES**

Run:
```
cmake --build ui/slint/build --target test_utf8
ctest --test-dir ui/slint/build -R test_utf8 --output-on-failure
```
Expected: `test_utf8: all assertions passed` and ctest reports `1 passed`.

- [ ] **Step 7: Commit**

```
git add ui/slint/app/utf8.h ui/slint/app/utf8.cpp ui/slint/test/test_utf8.cpp ui/slint/CMakeLists.txt
git commit -m "Add UTF-32 to UTF-8 encoder with unit test"
```

---

## Task 4: Output path — render fizmo's text in the transcript

**Goal:** the opening text of Zork appears in the Slint window, auto-scrolling.

**Files:**
- Create: `ui/slint/app/fizmo_slint_bridge.h`, `ui/slint/app/fizmo_slint_bridge.cpp`
- Modify: `ui/slint/app/zork.slint` (add transcript view)
- Modify: `ui/slint/app/main.cpp` (init fizmo + poll timer)
- Modify: `ui/slint/CMakeLists.txt` (add the new sources to `ZorkSlint`)

- [ ] **Step 1: Create `ui/slint/app/fizmo_slint_bridge.h`**

```cpp
#ifndef FIZMO_SLINT_BRIDGE_H
#define FIZMO_SLINT_BRIDGE_H

#include <string>

// Drain all currently-available interpreter output from the fizmo bridge,
// returning it as a UTF-8 string (empty if nothing is available).
std::string zork_drain_output();

#endif
```

- [ ] **Step 2: Create `ui/slint/app/fizmo_slint_bridge.cpp`**

```cpp
#include "fizmo_slint_bridge.h"
#include "utf8.h"
#include "fizmo_bridge.h"

#include <cstdint>
#include <vector>

std::string zork_drain_output() {
    std::string out;
    size_t avail = fizmo_output_available();
    if (avail == 0) {
        return out;
    }
    std::vector<std::uint32_t> buf(avail);
    size_t n = fizmo_output_read(buf.data(), buf.size());
    for (size_t i = 0; i < n; i++) {
        zucs_to_utf8(buf[i], out);
    }
    return out;
}
```

- [ ] **Step 3: Update `ui/slint/app/zork.slint` to show the transcript**

```slint
import { ScrollView } from "std-widgets.slint";

export component ZorkWindow inherits Window {
    title: "Zork — Slint";
    width: 540px;
    height: 960px;
    background: #06141f;

    in property <string> transcript;

    sv := ScrollView {
        x: 8px;
        y: 8px;
        width: parent.width - 16px;
        height: parent.height - 16px;
        // Auto-pin to the bottom as content grows.
        viewport-y: min(0px, sv.visible-height - sv.viewport-height);

        Text {
            width: sv.visible-width;
            text: root.transcript;
            color: #6fe0c0;
            font-family: "Cascadia Mono";
            font-size: 16px;
            wrap: word-wrap;
        }
    }
}
```
Note: "Cascadia Mono" is not bundled until Task 6; until then Slint falls back to
a default font. The auto-scroll binding is the idiom flagged in the spec — verify
it pins to the bottom in Step 6 and adjust if it fights you.

- [ ] **Step 4: Update `ui/slint/app/main.cpp` to start fizmo and poll output**

```cpp
#include "zork.h"   // generated from app/zork.slint
#include "fizmo_slint_bridge.h"
#include "fizmo_bridge.h"

#include <slint.h>
#include <slint_timer.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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

    // Accumulated transcript (UTF-8). Kept alive for the program's lifetime.
    auto transcript = std::make_shared<std::string>();

    slint::Timer poll_timer;
    poll_timer.start(slint::TimerMode::Repeated, std::chrono::milliseconds(30),
        [ui = slint::ComponentWeakHandle(ui), transcript]() {
            std::string chunk = zork_drain_output();
            if (chunk.empty()) {
                return;
            }
            *transcript += chunk;
            if (auto strong = ui.lock()) {
                (*strong)->set_transcript(slint::SharedString(*transcript));
            }
        });

    ui->run();

    fizmo_bridge_shutdown();
    return 0;
}
```
Note: `slint::ComponentWeakHandle` avoids a timer↔window reference cycle. If the
exact weak-handle type name differs in 1.16.1, capture `ui` directly (a strong
`ComponentHandle`) — acceptable for the spike — and rebuild.

- [ ] **Step 5: Add the new sources to `ZorkSlint` in `ui/slint/CMakeLists.txt`**

Change the `ZorkSlint` executable definition from:
```cmake
add_executable(ZorkSlint WIN32
    app/main.cpp
)
```
to:
```cmake
add_executable(ZorkSlint WIN32
    app/main.cpp
    app/fizmo_slint_bridge.cpp
    app/utf8.cpp
)
```

- [ ] **Step 6: Build and run**

Run:
```
cmake --build ui/slint/build --target ZorkSlint
./ui/slint/build/ZorkSlint.exe
```
Expected: the window shows the Zork opening text ("ZORK I… West of House… There
is a small mailbox here.") and, as more text arrives, the view stays scrolled to
the bottom.

- [ ] **Step 7: Commit**

```
git add ui/slint/app/fizmo_slint_bridge.h ui/slint/app/fizmo_slint_bridge.cpp \
        ui/slint/app/zork.slint ui/slint/app/main.cpp ui/slint/CMakeLists.txt
git commit -m "Render fizmo output in the Slint transcript view"
```

---

## Task 5: Input path — submit typed commands (acceptance criteria)

**Goal:** typing `open mailbox` + Enter produces the game's response. **This is
the spike's done criteria.**

**Files:**
- Modify: `ui/slint/app/zork.slint` (add `LineEdit` + `submit` callback, lay out vertically)
- Modify: `ui/slint/app/main.cpp` (wire `on_submit`)

- [ ] **Step 1: Update `ui/slint/app/zork.slint` with input + a vertical layout**

```slint
import { ScrollView, LineEdit } from "std-widgets.slint";

export component ZorkWindow inherits Window {
    title: "Zork — Slint";
    width: 540px;
    height: 960px;
    background: #06141f;

    in property <string> transcript;
    callback submit(string);

    VerticalLayout {
        padding: 8px;
        spacing: 6px;

        sv := ScrollView {
            vertical-stretch: 1;
            viewport-y: min(0px, sv.visible-height - sv.viewport-height);

            Text {
                width: sv.visible-width;
                text: root.transcript;
                color: #6fe0c0;
                font-family: "Cascadia Mono";
                font-size: 16px;
                wrap: word-wrap;
            }
        }

        input := LineEdit {
            placeholder-text: "Type a command…";
            accepted(text) => {
                root.submit(self.text);
                self.text = "";
            }
        }
    }
}
```

- [ ] **Step 2: Wire the `submit` callback in `ui/slint/app/main.cpp`**

Insert this block immediately after `auto transcript = std::make_shared<std::string>();`
and before the `slint::Timer poll_timer;` line:
```cpp
    ui->on_submit([transcript, ui = slint::ComponentWeakHandle(ui)]
                  (const slint::SharedString &cmd) {
        std::string line(cmd);
        *transcript += "\n>";
        *transcript += line;
        *transcript += "\n";
        if (auto strong = ui.lock()) {
            (*strong)->set_transcript(slint::SharedString(*transcript));
        }
        fizmo_submit_line(line.c_str());
    });
```

- [ ] **Step 3: Build and run**

Run:
```
cmake --build ui/slint/build --target ZorkSlint
./ui/slint/build/ZorkSlint.exe
```

- [ ] **Step 4: Verify the acceptance criteria manually**

1. The opening text renders (West of House).
2. Click the input field, type `open mailbox`, press Enter.
3. Expected: the transcript shows your echoed `>open mailbox` followed by the
   game's response: *"Opening the small mailbox reveals a leaflet."*
4. Type `read leaflet` + Enter and confirm further response — confirms the
   round-trip is sustained, not a one-shot.

- [ ] **Step 5: Commit**

```
git add ui/slint/app/zork.slint ui/slint/app/main.cpp
git commit -m "Wire LineEdit input to fizmo; round-trip playable (spike done)"
```

---

## Task 6: CascadiaMono font (best-effort parity)

**Goal:** the transcript renders in CascadiaMono for visual parity with the QUL
and LVGL stacks. Best-effort — if the font API fights us, the spike already
passed in Task 5, so a default monospace fallback is acceptable.

**Files:**
- Create: `ui/slint/fonts/CascadiaCode.ttf`
- Modify: `ui/slint/app/main.cpp` (register the font)

- [ ] **Step 1: Add the Cascadia Code TTF (SIL OFL, redistributable)**

```
curl -L -o ui/slint/fonts/CascadiaCode.zip \
  https://github.com/microsoft/cascadia-code/releases/download/v2407.24/CascadiaCode-2407.24.zip
cd ui/slint/fonts && unzip -j CascadiaCode.zip "ttf/CascadiaCode.ttf" && rm CascadiaCode.zip && cd -
ls ui/slint/fonts/CascadiaCode.ttf
```
Expected: `ui/slint/fonts/CascadiaCode.ttf` exists. (If the release tag 404s,
pick the latest tag from
https://github.com/microsoft/cascadia-code/releases and update the URL.) The
font family name inside this TTF is **"Cascadia Code"** — note that for Step 2.

- [ ] **Step 2: Register the font at startup in `ui/slint/app/main.cpp`**

Add this include near the top:
```cpp
#include <slint.h>
```
(already present). Then, as the **first statements inside `main()`** (before
`fizmo_bridge_init`), register the font:
```cpp
    if (auto err = slint::register_font_from_path(
            slint::SharedString(SLINT_FONT_DIR "/CascadiaCode.ttf"))) {
        std::fprintf(stderr, "font load failed: %s\n", err->data());
    }
```
Note: verify the `slint::register_font_from_path` signature in the installed
`slint.h` (it returns an optional/expected error in 1.16.1). If it returns
`void` or a different type, adapt the call. `SLINT_FONT_DIR` is provided by CMake
in the next step.

- [ ] **Step 3: Pass the font directory to the build (`ui/slint/CMakeLists.txt`)**

Add after the `ZorkSlint` `target_include_directories(...)` call:
```cmake
target_compile_definitions(ZorkSlint PRIVATE
    SLINT_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts"
)
```

- [ ] **Step 4: Set the family name in `ui/slint/app/zork.slint`**

Change the transcript `Text`'s font line from:
```slint
                font-family: "Cascadia Mono";
```
to:
```slint
                font-family: "Cascadia Code";
```

- [ ] **Step 5: Build, run, and verify**

Run:
```
cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/slint/build --target ZorkSlint
./ui/slint/build/ZorkSlint.exe
```
Expected: the transcript renders in a monospace (Cascadia) face. If the font does
not load, leave the fallback and note it — do not block the spike on this.

- [ ] **Step 6: Commit**

```
git add ui/slint/fonts/CascadiaCode.ttf ui/slint/app/main.cpp \
        ui/slint/CMakeLists.txt ui/slint/app/zork.slint
git commit -m "Bundle and register CascadiaCode font for transcript"
```

---

## Task 7: Clean up the smoke test and document the stack

**Goal:** remove the Task 1 scaffolding that the real app supersedes, and leave a
short note so the next session knows how to build.

**Files:**
- Delete: `ui/slint/app/smoke_main.c`
- Modify: `ui/slint/CMakeLists.txt` (drop the `zork_smoke` target)
- Create: `ui/slint/README.md`

- [ ] **Step 1: Remove the smoke target from `ui/slint/CMakeLists.txt`**

Delete these lines (the smoke executable block from Task 1):
```cmake
# ---------------------------------------------------------------------------
# Headless smoke test: prove the interpreter runs under MSVC (no UI).
# ---------------------------------------------------------------------------
add_executable(zork_smoke app/smoke_main.c)
target_include_directories(zork_smoke PRIVATE "${PROJECT_ROOT}/src")
target_link_libraries(zork_smoke PRIVATE zorkfizmo Threads::Threads)
```

- [ ] **Step 2: Delete the smoke source**

```
git rm ui/slint/app/smoke_main.c
```

- [ ] **Step 3: Create `ui/slint/README.md`**

```markdown
# Zork — Slint UI (spike)

Third peer UI stack (Slint 1.16.1, C++ binding) for the ZorkForMCUs bake-off,
alongside `ui/qul` and `ui/lvgl`. Targets NXP RT1050/RT1170; desktop-first.

## Status
Integration spike: text round-trip playable on the desktop. No status bar,
compass rose, on-screen keyboard, or display-profile switch yet (see
`docs/superpowers/specs/2026-06-15-slint-ui-integration-spike-design.md`).

## Build (Windows, MSVC)
1. Install the Slint C++ MSVC package into `ui/slint/slint-install/`:
   `Slint-cpp-1.16.1-win64-MSVC-AMD64.exe /S /D=<abs>\ui\slint\slint-install`
2. From a "Developer PowerShell for VS 2022":
   ```
   cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build ui/slint/build --target ZorkSlint
   ./ui/slint/build/ZorkSlint.exe
   ```

## Tests
`ctest --test-dir ui/slint/build` runs the UTF-8 encoder unit test.
```

- [ ] **Step 4: Reconfigure to confirm the build is still green without the smoke target**

Run:
```
cmake -S ui/slint -B ui/slint/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ui/slint/build
ctest --test-dir ui/slint/build --output-on-failure
```
Expected: `ZorkSlint` and `test_utf8` build; ctest passes.

- [ ] **Step 5: Commit**

```
git add ui/slint/CMakeLists.txt ui/slint/README.md
git commit -m "Remove smoke scaffold; document Slint stack build"
```

---

## Self-review notes (completed during planning)

- **Spec coverage:** in-scope items map to tasks — scrolling output → Task 4;
  line input → Task 5; 540×960 window → Task 2; CascadiaMono → Task 6; color
  scheme → Tasks 2/4; no SDL2 / FetchContent-prebuilt / C++20 / no-Rust → Tasks
  1–2 (MSVC installer path per the spec addendum). Out-of-scope items (status
  bar, compass, on-screen keyboard, profile switch, hardware) are intentionally
  absent.
- **Toolchain bail:** Task 1 Step 7 encodes Brendan's "don't let it be a tar pit"
  guardrail with an explicit 1–2 iteration limit and MinGW fallback.
- **Type consistency:** `zucs_to_utf8(std::uint32_t, std::string&)` and
  `zork_drain_output()` are used consistently across Tasks 3–4; the `.slint`
  property `transcript` and callback `submit(string)` match their `set_transcript`
  / `on_submit` C++ uses in Tasks 4–5.
- **Known Slint-API verifications (flagged inline, not placeholders):** generated
  header name `zork.h`; `register_font_from_path` return type; the weak-handle
  type name; the auto-scroll `viewport-y` idiom. Each has a concrete fallback in
  its step.
