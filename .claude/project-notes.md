# modular-ui-controller - Project Notes

**⚠️ KEEP THIS FILE CONCISE** - Only essential gotchas and current work.

---

## 🚨 CRITICAL RULES 🚨

### Windows File Paths
**🚨 ALWAYS use backslashes - CRITICAL, always forgotten after context compaction! 🚨**
- ❌ `D:/PlatformIO/Projects/...`
- ✅ `D:\PlatformIO\Projects\...`

### User Handles All Builds
- ❌ NEVER ask to build/upload code
- ✅ Just make changes and wait for test results

### Zero Warnings Policy
- ❌ NO compiler warnings allowed
- ✅ Fix all warnings, treat them as errors

---

## Project Overview

- **Framework**: Arduino (NOT ESP-IDF)
- **Board**: ESP32-S3-DevKitC-1
- **Display**: 320x480 touch screen with LVGL v9
- **LEDs**: FastLED multi-strip animations with audio sync
- **Web UI**: AsyncWebServer + WebSocket

---

## Related Libraries

### ESP32WifiSetup (Arduino version)
- **Location**: `D:\PlatformIO\Projects\ESP32WifiSetup`
- **Symlinked** via `lib_deps = symlink://../ESP32WifiSetup`
- Provides: WiFi setup, captive portal, web server, OTA updates

### ESP32WifiSetup-IDF (ESP-IDF version)
- **Location**: `D:\PlatformIO\Projects\ESP32WifiSetup-IDF`
- **NOT used by this project** - for ESP-IDF projects only

---

## Platform Gotchas

### AsyncWebServer / AsyncTCP
- **Registration order matters** - specific routes BEFORE wildcards
- **`server->on()` does PREFIX matching** - `/api/foo` matches `/api/foobar`!

### LVGL v9
- Search codebase before using functions - v8 APIs don't exist
- **State-based styling** - define styles ONCE, toggle with `lv_obj_add_state()`
- ❌ NEVER call `lv_obj_set_style_*` repeatedly in loops (causes freeze)
- **Tabview uses buttons, NOT btnmatrix** - `LV_PART_ITEMS` doesn't work!
  ```cpp
  // WRONG (v8 style):
  lv_obj_set_style_text_color(tab_btns, color, LV_PART_ITEMS);

  // CORRECT (v9):
  lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview_);
  for (uint32_t i = 0; i < lv_obj_get_child_count(tab_bar); i++) {
      lv_obj_t* btn = lv_obj_get_child(tab_bar, i);
      lv_obj_set_style_text_color(btn, color, 0);  // inactive
      lv_obj_set_style_text_color(btn, color, LV_STATE_CHECKED);  // active
  }
  ```

### ArduinoJson v7
- ✅ `obj["key"].is<T>()`, `obj["key"].isNull()`
- ❌ `obj.containsKey()`, `doc.as<JsonObject>()`

---

## Architecture

### Global Managers
```cpp
extern UIManager* g_uiManager;           // LVGL UI
extern WiFiSetupManager* g_wifiManager;  // WiFi
extern LEDManager* g_ledManager;         // LED animations
extern WebUIManager* g_webUIManager;     // Web interface
extern OTAManager* g_otaManager;         // Firmware updates
extern WiFiSetupBootUI* g_bootUI;        // WiFi setup display
```

### Main Loop Pattern
```cpp
loop():
  - LVGL tick updates
  - UIManager update
  - WiFiSetupManager update (DNS in AP mode)
  - LEDManager update (animations)
  - WebUIManager update (WebSocket cleanup)
  - OTAManager loop
```

---

## LVGL UI Design Notes

### Style Constants
**ALL colors and styles in `include/modular-ui.h`** - use `UI_COLOR_*` and `UI_*` constants.

### Colour Tab Layout
- Left column: vertical brightness fader + VU button (stacked)
- Center: colour wheel (with 20px container padding for indicator)
- Bottom row: White button + Effects dropdown

### Component Behaviors
- **Effects dropdown**: Lights up cyan when animation is active, dims when color/white active
- **Buttons**: Thick bright border when active (shadows/glow don't work on this hardware)
- **Color wheel indicator**: Custom `lv_colorwheel.c` with larger filled circle indicator

### Key Gotchas
- Container padding needed for color wheel indicator (not overflow flags)
- Dropdown list styling must be done in open handler (list doesn't exist until opened)
- Use `lv_obj_set_scroll_dir(list, LV_DIR_VER)` for vertical-only scroll on dropdown

---

## 🎛️ Remaining UI Work

### Phase 3: VU Meter Overhaul ✅
- [x] Segmented bar design (16 individual LED rectangles per channel)
- [x] Color gradient: green → yellow → red
- [x] Peak hold indicator with decay

### Phase 4: Polish ⬜
- [ ] Smooth animations
- [ ] Final testing on hardware
- [ ] Update Web UI to match

---
