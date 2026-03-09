# LVGL UI — Next Steps (2026-03-09)

## What's working
- All three display profiles build and run: RT1050 (480x272), RT1170 (720x1280), RT1170_SCALED (540x960)
- CascadiaMono font at profile-matched sizes (14/18/24px) with Montserrat fallback for symbols
- Compass rose renders via compile-time C array, positioned in output area bottom-right
- Keyboard: always visible on RT1170/RT1170_SCALED, tap-to-show on RT1050
- Game is playable on all profiles via physical keyboard and on-screen keyboard

## Issues to fix

### RT1050 keyboard overlay
- When keyboard pops up, it covers the compass rose and most of the output area
- QUL handles this by anchoring the input area above the keyboard and letting the output area shrink
- In LVGL flex layout, the keyboard is a flex child so the output area (flex-grow) should already shrink — but the compass position is calculated at init time and doesn't update when the keyboard shows/hides
- **Fix**: Recalculate compass position when keyboard visibility toggles, or move compass into the output textarea as a child

### RT1050 keyboard dismiss
- Currently the keyboard toggles on tap of the textarea (LV_EVENT_FOCUSED)
- Since the textarea is already focused at startup, the first tap doesn't trigger (focus is already held)
- Need a way to dismiss the keyboard too (QUL hides it after pressing GO/Enter)
- **Fix**: Hide keyboard on LV_EVENT_READY (Enter pressed), and use LV_EVENT_CLICKED on the textarea to show it

### RT1050 keyboard key truncation
- Some keys on the bottom-right row appear truncated at 480px width
- May need slightly smaller key padding or font size for the RT1050 keyboard

### RT1170 profile (untested)
- RT1170 (720x1280) has not been visually tested yet — build and verify

## Future work (from LVGL_GUI_PLAN.md)
- Phase 3: MCU cross-compilation (RT1050/RT1170 board targets)
- Phase 4: Touch input refinement, accessibility
- Phase 5: Performance optimization for MCU constraints
