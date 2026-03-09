# ZorkForMCUs

Z-machine interpreter (Zork I) on NXP microcontrollers. Two UI stacks: Qt for MCUs (original) and LVGL (in progress, `lvgl` branch).

## Architecture

- `src/` — shared platform layer (fizmo bridge, filesystem, story embedding). UI-framework-agnostic.
- `ui/qul/` — Qt for MCUs UI (production, on `main`)
- `ui/lvgl/` — LVGL v9 UI (in development, on `lvgl` branch)
- `external/libfizmo/` — Z-machine interpreter submodule
- `external/lvgl/` — LVGL v9.4.x submodule
- `external/sdl2/` — SDL2 submodule (desktop simulator)

## Active Work

- Feature branch: `lvgl`
- Plan: `docs/LVGL_GUI_PLAN.md` (7 phases)
- Current status: Phases 0-2 largely complete (scaffold, desktop shell, fizmo integration). Game is playable in SDL simulator. UI polish and compass rose remain.

## Testing

- Manual testing via desktop SDL2 simulator for now.
- No automated test framework yet.

## Build

- CMake/Ninja build system.
- Local build instructions are in Claude auto-memory (not committed): see `build-instructions.md`.

## Branch Policy

- `main` — stable Qt for MCUs build
- `lvgl` — LVGL feature branch
- No naming conventions for intermediate/worktree branches.

Now say: "I've reviewed the project memory."
