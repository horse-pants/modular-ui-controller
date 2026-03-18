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

## 🎛️ Current Work: UI Redesign (Synth/Audio Gear Aesthetic)

**Goal**: Transform primitive LVGL UI into professional-looking synth/audio hardware style

### ⚠️ Maintainability Rule
**ALL colors and styles defined in `include/modular-ui.h`**.
No hardcoded hex values in component files - always use `UI_COLOR_*` and `UI_*` constants.

### Design Principles
- Dark backgrounds with neon accents (cyan primary)
- Hardware-like panels with distinct modules
- LED-style indicators and illuminated buttons
- Segmented VU meter (green→yellow→red)
- Glow effects on active elements

### Phase 1: Structure & Layout ✅
- [x] Centralized colors + layout constants in `modular-ui.h`
- [x] Reusable panel styles in `UIManager` (static styles)
- [x] Components don't position themselves (parent layout controls)
- [x] Consistent spacing via `UI_PADDING_*` / `UI_SPACING_*` constants

### Phase 1.5: Fader-Style Layout ✅
- [x] Removed Effects tab (reduced to 2 tabs: Colour, VU)
- [x] Made BrightnessSlider vertical (24px wide, 200px tall)
- [x] Converted EffectsList from roller to dropdown (drop-up direction)
- [x] Restructured Colour tab layout:
  - Left side: vertical fader
  - Right side: colour wheel (flex-grow fills space)
  - Bottom row: VU + White + Effects dropdown

### Phase 2: Component Styling ✅
- [x] Buttons: Illuminated hardware style with glow when active (VU=cyan, White=white glow)
- [x] Slider: Metallic knob, recessed track, glowing cyan indicator
- [x] Tab bar: Hardware selector style with glow on active tab
- [x] Effects dropdown: Hardware look with glowing popup list
- [x] Added `UI_BTN_COMPACT_WIDTH` (56px) for narrow buttons

### Phase 3: VU Meter Overhaul ⬜
- [ ] Segmented bar design (individual LED rectangles)
- [ ] Color gradient: green → yellow → red
- [ ] Peak hold indicator

### Phase 4: Polish ⬜
- [ ] Glow/shadow effects
- [ ] Smooth animations
- [ ] Test on hardware

---
