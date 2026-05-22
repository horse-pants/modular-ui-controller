# Testing & Code Review

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
