# modular-ui-controller

ESP32-S3 LED controller with a 320×480 LVGL touch UI, FastLED multi-strip animations with audio sync, and an AsyncWebServer + WebSocket web UI. Built on **Arduino** framework (NOT ESP-IDF).

---

## 🚨 Critical Workflow Rules 🚨

### User handles all builds
- ❌ NEVER ask to build/upload code
- ✅ Make changes; wait for the user's test results

### Zero warnings policy
- ❌ NO compiler warnings allowed
- ✅ Fix all warnings; treat them as errors

### Windows file paths — ALWAYS backslashes
**Critical, often forgotten after context compaction.**
- ❌ `D:/PlatformIO/Projects/...`
- ✅ `D:\PlatformIO\Projects\...`

### Single-user project — no migration paths

This is the user's personal device. **Don't write NVS migration code, backwards-compatibility shims, deprecated-key fallbacks, or "preserve old settings" logic.** When a field renames or moves, just rename/move it — the user will reflash + reconfigure as needed.

---

## Project Overview

- **Framework**: Arduino (NOT ESP-IDF)
- **Board**: ESP32-S3-DevKitC-1 (`platformio.ini` env `esp32-s3-devkitc-1`)
- **Display**: 320×480 touch screen driven by LovyanGFX + LVGL v9
- **LEDs**: FastLED multi-strip with audio-reactive animations
- **Web UI**: AsyncWebServer + WebSocket (served from LittleFS)
- **OTA**: via `ESP32WifiSetup` library

---

## Project layout

```
include/                — headers
  modular-ui.h          — UI theme constants (colors, spacing) — single source of truth
  lv_conf.h             — LVGL configuration
  UIManager.h           — top-level LVGL screen orchestrator
  NetworkManager.h      — WiFi + boot UI wrapper around ESP32WifiSetup
  LEDManager.h          — FastLED strip + animations
  WebUIManager.h        — AsyncWebServer routes + WebSocket
  AudioAnalyzer.h       — mic input → FFT → audio sync source
  ColourWheel.h, VuGraph.h, VuButton.h,
  WhiteButton.h, EffectsList.h, BrightnessSlider.h
                        — LVGL component classes used by UIManager
  lv_colorwheel.h       — custom LVGL colour wheel with larger indicator
src/                    — implementation (mirrors include/ tree)
  ui/                   — LVGL component .cpp files + UIManager.cpp
  main.cpp              — setup() + loop(); delegation-only
data/                   — static assets served by WebUIManager (LittleFS)
scripts/pre_build_littlefs.py — pre-build step
platformio.ini          — env: esp32-s3-devkitc-1
```

---

## Architecture

### Global managers (declared in `main.cpp`)

```cpp
extern UIManager*      g_uiManager;      // LVGL UI
extern NetworkManager* g_networkManager; // WiFi + boot UI wrapper
extern LEDManager*     g_ledManager;     // LED strips + animations
extern WebUIManager*   g_webUIManager;   // Web interface
extern OTAManager*     g_otaManager;     // Firmware updates
```

### Main loop pattern

`loop()` is a sequence of `Update()` calls — `lv_tick_inc`, `UIManager::update`, `NetworkManager::update`, `LEDManager::update`, `WebUIManager::update`, `OTAManager::loop`. Don't inline logic between them; put work on the manager being invoked.

`setup()` initialises UI first, then network. If the network manager enters setup mode (no saved WiFi), the LED + full UI init are skipped — only the boot screen is up. Keep this branch shape; the boot UI assumes LEDs aren't initialised yet.

---

## Related libraries

### ESP32WifiSetup (Arduino fork — what this project uses)
- **Location**: `D:\PlatformIO\Projects\ESP32WifiSetup`
- **Symlinked** via `lib_deps = symlink://../ESP32WifiSetup` in `platformio.ini`
- Provides WiFi captive portal, web server, OTA, boot UI. Fixes go here.

### ESP32WifiSetup-IDF (ESP-IDF fork — NOT used by this project)
- **Location**: `D:\PlatformIO\Projects\ESP32WifiSetup-IDF`
- Sister library for ESP-IDF projects. Keep changes mirrored if they affect shared HTML/CSS/JS (see EverythingControl's `dependencies.md` for the sync contract).

---

## Where to look

- **Rules & gotchas** → `.claude\rules\` (read these before editing — hard-won constraints)
- **Coding standards** → `.claude\rules\general.md` + `.claude\rules\cpp-embedded.md`
- **Settings** → `.claude\settings.local.json`

### Rule files

- `general.md` — file size, SRP, DRY, encapsulation, naming
- `cpp-embedded.md` — memory, types, RAII, strings, logging
- `lvgl.md` — LVGL v9 specifics, tabview styling, state-based styling
- `arduinojson.md` — v7 API quirks
- `async-events.md` — AsyncWebServer route ordering, never-block rule
- `theming.md` — `modular-ui.h` constants, no hardcoded colours
- `testing.md` — manual test strategy, code-review checklist
