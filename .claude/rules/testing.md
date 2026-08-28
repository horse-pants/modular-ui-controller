# Testing & Code Review

## 🚨 Adding a file forces a FULL rebuild — batch them

PlatformIO's `compute_project_checksum()` hashes the PIO Core version, `platformio.ini`, **and
the sorted list of every `.c/.cc/.cpp/.h/.hpp/.s/.S` path under `include/`, `src/` and `lib/`**.
Any mismatch and `clean_build_dir()` does `fs.rmtree(build_dir)` — the whole thing, all ~531
LVGL objects included. That is a ~7 minute rebuild on this project.

So a full rebuild is triggered by:

- **adding or deleting any header/source** under `include/` or `src/` (the usual cause, and the
  easiest to forget)
- editing `platformio.ini`
- editing `include/lv_conf.h` — this one is header dependencies, not the checksum, but it
  rebuilds all of LVGL just the same

Consequences for how to work:

- **Batch new files into one pass.** Ten files added together cost one rebuild; ten files added
  across ten turns cost ten.
- **Batch `lv_conf.h` and `platformio.ini` edits** with each other and with file additions.
- When the only change is added/removed files, the user can skip the wipe entirely:
  `pio run --disable-auto-clean` (it bypasses `clean_build_dir`). Don't use it after a
  `platformio.ini` or `lv_conf.h` change, which genuinely need the rebuild.

## After each change

1. Build succeeds (user runs the build — see CLAUDE.md)
2. Feature still works (manual test by user on the device)
3. No visual regressions on the LVGL UI or the web UI
4. No memory issues — watch free heap on long runs

## Code-review checklist

Before marking a change ready:

- [ ] File under 500 lines (target 200–400)
- [ ] No duplicated code
- [ ] Colours via `UI_COLOR_*` constants from `modular-ui.h` (no hardcoded hex)
- [ ] Callbacks unregistered in destructor (lambda capture safety)
- [ ] No blocking in callbacks (web, WebSocket, LVGL, audio)
- [ ] No `lv_obj_set_style_*` inside loops / update ticks (state-based styling instead)
- [ ] AsyncWebServer routes: specific before wildcard, GET before POST
- [ ] `Logger` used, not `Serial.println`
- [ ] Versioned dependency in `platformio.ini`, not a git URL
- [ ] Backslashes in any Windows path strings
- [ ] Zero compiler warnings
