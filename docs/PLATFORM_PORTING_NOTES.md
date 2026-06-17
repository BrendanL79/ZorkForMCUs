# Platform Porting & ABI Notes

Hard-won lessons about this repo's platform-compatibility shims and the ABI
hazards hiding in them. **Read this before** editing the shim layers
(`src/msvc_shims/`, `src/sys/`, `src/dirent.h`, `src/posix_stubs.c`,
`src/fizmo_msvc_compat.h`, `src/fizmo_embedded_compat.h`) or starting a port to a
new toolchain — especially the MCU hardware port.

## Why the shims exist

`external/libfizmo` is a **pinned upstream dependency we never modify** (see the
project CLAUDE.md). It includes POSIX headers (`<sys/stat.h>`, `<dirent.h>`,
`<unistd.h>`, …) and calls POSIX functions. On platforms that lack those headers
or functions, we satisfy the includes/symbols with compatibility shims under
`src/`, branched by toolchain macro:

- **`__ARM_EABI__`** — bare-metal ARM / FreeRTOS (the MCU target). Minimal stubs
  for `<sys/stat.h>`, `<dirent.h>`, and POSIX functions (`posix_stubs.c`). The
  embedded build uses `fizmo_filesys_hybrid.c` + FatFS for file I/O and
  **excludes `filesys_c.c`**, so `stat`/`fstat`/`opendir` are *declared but never
  called*.
- **`_MSC_VER`** — Windows / MSVC desktop (the Slint stack, ARM64). Shims for
  `<unistd.h>`, `<strings.h>`, `<dirent.h>` (+ a Win32-backed `dirent_msvc.c`), a
  `gettimeofday`, `getuid`/`getpwuid`, and a `<sys/stat.h>` that aliases the
  POSIX names to the UCRT's `_stat64`/`_fstat64`.

`src/` sits **ahead of the system headers** on the include path, so these shims
*shadow* the real `<sys/stat.h>` etc. That shadowing is the root of the hazard
below.

## The ABI lesson — the `struct stat` bug (2026-06)

**What happened.** The MSVC `<sys/stat.h>` shim originally hand-rolled
`struct stat` with a 32-bit `long st_size`. The UCRT's real `_stat64` uses a
**64-bit** `st_size`, which shifts every following field (`st_mtime`, `st_mode`,
…). When the CRT's `stat`/`fstat` writes the real 88-ish-byte struct into our
84-ish-byte buffer, it corrupts adjacent memory and we read garbage timestamps /
mode bits.

**Why nothing caught it:**

- **It compiled.** A header is a *promise*, not a verified fact. Within
  `filesys_c.c` our struct + prototypes were self-consistent, and the compiler
  never sees the real UCRT layout (we shadowed it). Separate compilation checks
  each TU only against the headers it pulled in.
- **It linked.** Linking resolves symbol *names*, not layouts. We declared
  `stat`/`fstat` with no definition; the UCRT provides POSIX-named entry points,
  so the references resolved. Symbol tables carry names and addresses, never
  field offsets.
- **Tests passed.** `stat`/`fstat` are only reached on libfizmo's SAVE/RESTORE
  metadata path; the smoke test and normal play never trigger it, so the corrupt
  read never executed.

A latent ABI bug: invisible to the compiler, the linker, and every test that
doesn't walk the one path that uses it. It would first bite the day someone saves
a game.

**The fix.** On MSVC, alias the POSIX names to the real `_stat64`/`_fstat64` and
declare the exact UCRT layout, so the struct is *defined by the CRT*, not
hand-asserted. (See `src/sys/stat.h`.)

**The principle.** *A stub is only as safe as the assumptions that make it
unobservable:* (1) nothing else provides a conflicting real implementation, and
(2) the code path is dead. **Those assumptions do not travel when you copy the
pattern to a new toolchain.** The `__ARM_EABI__` stat stub honored both (no real
`stat` on bare metal; `filesys_c.c` excluded). The `_MSC_VER` copy quietly broke
both (the UCRT *has* a real `stat`; `filesys_c.c` is live on desktop). The
problem was never "stubs are bad" — it was transplanting a stub across the
assumption boundary without rechecking it.

## Watch-list for the MCU hardware port

The UCRT-specific bug above cannot recur on the MCU (no UCRT, and the embedded
build doesn't use POSIX `stat` — it uses FatFS via the hybrid filesys). But the
*general* class — asserting a layout that disagrees with what a library reads/
writes — has these embedded flavors, roughly by likelihood:

1. **FatFS config drift (`ffconf.h`).** `FATFS` / `FIL` / `FILINFO` / `DIR`
   layouts are defined by macros (`FF_USE_LFN`, `FF_MAX_SS`, `FF_FS_EXFAT`,
   `FF_LFN_BUF`, …). Every consumer — `ff.c`, `fizmo_filesys_hybrid.c`,
   `diskio_stub.c`, any SDK middleware — must compile against the **one**
   `src/ffconf.h`. Controlled today because `ff.c` is built from source; the risk
   is linking a *prebuilt* FatFS configured differently.
2. **Float ABI uniformity.** `-mfloat-abi` / `-mfpu` / `-mcpu` must be identical
   across all objects and libs; mixing `hard`/`softfp` corrupts float/double
   arguments. Usually the GNU linker *does* catch this (ELF `Tag_ABI_VFP_args`
   attributes) — the danger is a prebuilt vendor blob with a different ABI.
3. **FreeRTOS / NXP SDK config-dependent structs.** `FreeRTOSConfig.h` and SDK
   feature macros shape TCBs, queues, peripheral configs. Build from source
   against one config so every TU agrees. (Memory-mapped *register* structs are
   lower risk — the silicon + SDK headers define them.)
4. **The dormant `__ARM_EABI__` stat/dirent stubs.** Safe **only while
   `filesys_c.c` stays out of the embedded build** (the FatFS hybrid path is
   used). If anyone wires `filesys_c.c` into the MCU build, those hand-rolled
   stubs become the next `struct stat` bug. Either keep the hybrid path, or
   delete the stubs when it's permanent.

## Cheap, durable defense

At any boundary where you assert a layout you don't own, add a compile-time
guard so a silent runtime ABI bug becomes a loud build error:

```c
#include <assert.h>
static_assert(sizeof(struct stat) == EXPECTED, "stat layout drifted");
static_assert(offsetof(struct stat, st_mtime) == EXPECTED_OFFSET, "st_mtime moved");
```

This single line would have turned the 2026-06 `stat` corruption into a failed
build instead of a latent defect.
