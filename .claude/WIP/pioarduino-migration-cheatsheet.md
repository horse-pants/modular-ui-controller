# pioarduino migration cheatsheet

Portable reference for migrating an ESP32 Arduino-framework PlatformIO project from PlatformIO Labs' `espressif32` platform to the community-maintained `pioarduino/platform-espressif32` fork.

Motivation: PlatformIO Labs' `espressif32` is effectively unmaintained. pioarduino tracks current Arduino-ESP32 (v3.x) and bundles modern ESP-IDF (v5.5+). It self-identifies as `espressif32`, so libraries declaring `"platforms": "espressif32"` remain compatible.

Order the steps in this doc — each one can surface the next error.

---

## 1. Update `platform =` line

```ini
[env:...]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.38-1/platform-espressif32.zip
```

Get the latest tag from https://github.com/pioarduino/platform-espressif32/releases. Use the exact zip URL — pioarduino isn't on PlatformIO's registry as a regular platform name.

If the project symlinks a sibling library (e.g. a private WiFi-setup library), update that library's own `platformio.ini` to the same pioarduino URL so its IntelliSense env matches.

---

## 2. PlatformIO Core ≥ 6.1.19 required

pioarduino `55.03.x` needs PIO Core ≥ 6.1.19. Check with `pio --version`.

If `pio --version` still reports < 6.1.19 after `pip install -U platformio`:

```bash
# Find where platformio is actually loading from
'<penv>/Scripts/python.exe' -c "import platformio; print(platformio.__version__); print(platformio.__file__)"
```

**Common trap:** `pip show platformio` reports the new version (metadata updated) but `__init__.py` still has the old `VERSION = (...)` tuple. pip wrote dist-info but didn't replace the source files — usually because AV or a file lock interfered during upgrade. Also tells you if pioarduino's own installer overwrote the penv `platformio/` package files (the `__title__` will read `"pioarduino core"` instead of `"PlatformIO Core"` — that's the giveaway).

Fix:
```bash
'<penv>/Scripts/python.exe' -m pip install --force-reinstall --no-deps platformio==6.1.19
```

`--no-deps` keeps it fast and avoids churning unrelated deps.

---

## 3. Evict the cached old platform install

After bumping the `platform =` line, the old install is still on disk:

```bash
pio pkg uninstall -g -p espressif32
```

Next `pio run` will pull pioarduino fresh.

---

## 4. LovyanGFX: `gpio_hal_iomux_func_sel` undefined

```
error: 'gpio_hal_iomux_func_sel' was not declared in this scope; did you mean 'gpio_hal_func_sel'?
```

LovyanGFX < 1.2.19 doesn't know about the IDF v5.5 GPIO HAL rename. Bump the constraint floor (a caret range like `^1.2.7` won't re-resolve since the older version is already cached):

```ini
lib_deps =
    lovyan03/LovyanGFX@^1.2.21   ; was ^1.2.7
```

---

## 5. ESPAsyncWebServer pulls RPAsyncTCP / ESPAsyncTCP into the build

```
.pio/libdeps/.../RPAsyncTCP/src/RPAsyncTCP.cpp: error:
  'ip_addr_t' {aka 'struct ip_addr'} has no member named 'addr'; did you mean 'u_addr'?
```

**Why this happens:**
- Newer ESPAsyncWebServer (3.9.x+) added Raspberry Pi Pico support and declares `RPAsyncTCP` (Pico-only) and `ESPAsyncTCP` (ESP8266-only) as platform-scoped siblings of `AsyncTCP` in its `library.json`.
- PlatformIO Core has a long-standing bug ([platformio-core#4338](https://github.com/platformio/platformio-core/issues/4338)) where the LDF ignores `"platforms"` filters on transitive deps. Marked closed against milestone 6.1.0 but still broken as of 6.1.19.
- IDF v5.5 turned `ip_addr_t` from a struct-with-`.addr` into a tagged union (`u_addr.ip4.addr`). Pre-migration, the wrong-platform code compiled silently (wasted disk). Post-migration with IDF 5.5, it errors loudly.

**Fix (the community-accepted workaround — there is no clean upstream fix):**

```ini
lib_ignore =
    RPAsyncTCP
    ESPAsyncTCP
```

That's it. Don't add explicit `esp32async/AsyncTCP` / `ESPAsyncWebServer` pins to `lib_deps` if they're already pulled in transitively (e.g. by a symlinked WiFi-setup library declaring them in its own `library.json`) — `lib_ignore` alone solves the problem. If you *do* depend on them directly, bump those library's declared floors instead of pinning at the project level.

While you're there, bump the floors in any sibling library that declares these as deps — older library.json caret ranges (e.g. `^3.7.3`) won't force re-resolution if an older copy is cached. Bumping to current (e.g. `^3.11.0` for ESPAsyncWebServer, `^3.4.9` for AsyncTCP) keeps things deterministic.

**Note on parallel ESP-IDF-framework forks:** if you maintain a sister library that uses the ESP-IDF `esp_http_server` component directly (not AsyncTCP), this section does NOT apply to it — those projects don't pull AsyncTCP at all, so the bug can't manifest.

---

## 6. Name collision: project-defined `NetworkManager` class

```
include/NetworkManager.h: error: redefinition of 'class NetworkManager'
note: previous definition at
  framework-arduinoespressif32/libraries/Network/src/NetworkManager.h
```

Arduino-ESP32 v3 introduced a framework class called `NetworkManager` (in `Network.h`, transitively pulled in by `WiFi.h`). Any project class with the same name collides.

**Fix:** rename your class. Don't bother with `using` aliases or `#define` shims — those don't help when the framework header gets included by other headers in your own tree. A direct rename is cleanest.

In this project, `NetworkManager` → `WifiBootManager` (it wraps ESP32WifiSetup + boot UI — the new name describes the actual job).

Touch:
- Rename `include/NetworkManager.h` → `include/WifiBootManager.h` (use `git mv` to preserve history)
- Rename `src/NetworkManager.cpp` → `src/WifiBootManager.cpp`
- Class name, global pointer (`g_networkManager` → `g_wifiBootManager`)
- Includes in every file that referenced it (umbrella headers + main.cpp)
- Docs that name the class

Watch out for other Arduino-ESP32 v3 framework names that might collide with project classes — common ones to grep for: `NetworkManager`, `NetworkClient`, `NetworkServer`, `NetworkEvents`, `NetworkInterface`.

---

## 7. LittleFS toolchain

The PlatformIO Labs `espressif32` platform was missing `mklittlefs` in newer versions and required a `pre:scripts/pre_build_littlefs.py` hook to locate one on PATH. **pioarduino bundles `tool-mklittlefs` correctly** — the hook is no longer needed.

Test: comment out the line, run `pio run -t buildfs`. If the LittleFS image builds, delete the script and the `extra_scripts` line.

---

## 8. Binary size jump — partition table likely too small

```
Error: The program size (~1.8 MB) is greater than maximum allowed (1572864 bytes)
Flash: [==========]  115.1% (used 1810331 bytes from 1572864 bytes)
```

Arduino-ESP32 v3 / pioarduino produces noticeably larger binaries than v2 — community-reported jumps of 200–500 KB are normal, sometimes more. Cumulative causes:
- IDF v5 baseline is bigger than v4 (panic handler, log infra, mbedtls defaults)
- New `Network*` class hierarchy compiles in unconditionally
- IPv6 in lwIP enabled by default
- pioarduino bundles its own sdkconfig — some features that were off may now be on

**Before resizing partitions, do a quick sanity check** that nothing accidental got pulled in:
```bash
# Section breakdown (look for unexpected .flash.text growth)
xtensa-esp-elf-size --format=sysv .pio/build/<env>/firmware.elf

# Top symbols by size (look for BT_*/bluedroid/nimble — should be absent if you don't use BT)
xtensa-esp-elf-nm --size-sort --print-size --reverse-sort .pio/build/<env>/firmware.elf | head -40
```
If you see Bluetooth, mbedtls cipher tables, or wide-char printf you didn't intend, that's actionable. If it's just LVGL fonts, framework code, and lwIP — it's real v3 growth, just accept and resize.

**Resize the partition table.** ESP32-S3-DevKitC-1 boards typically ship with 8 MB flash but Arduino's default partition layouts only use 4 MB. Doubling each OTA slot to 2.5 MB and growing the filesystem gives plenty of headroom:

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x280000,
app1,     app,  ota_1,   0x290000,0x280000,
spiffs,   data, spiffs,  0x510000,0x2F0000,
```

(Label `spiffs` is a SubType convention — Arduino-ESP32 uses the same SubType for LittleFS partitions.)

**⚠ Critical:** a partition layout change requires a **full USB flash**, not OTA. An OTA from the old layout would write the larger firmware into the old smaller slot and brick the device. After the first full flash with the new table, OTA works normally with the new slot size.

If you don't have room to grow (e.g. 4MB board), other levers:
- `-DCORE_DEBUG_LEVEL=0` (saves 50–100 KB of log strings)
- `build_unflags = -fexceptions` + `build_flags = -fno-exceptions -fno-rtti` (saves 20–80 KB)
- Audit `lv_conf.h` for unused fonts (often the biggest single win)

---

## 9. Arduino-ESP32 v3 API surface

Most code is unchanged but watch for:
- `analogWrite` / `ledcSetup` / `ledcAttachPin` — `ledc*` API was restructured in Arduino-ESP32 v3.0. New API is `ledcAttach(pin, freq, resolution)` then `ledcWrite(pin, duty)`. Old `ledcSetup` + `ledcAttachPin` calls won't compile.
- Direct `esp_wifi_*` / IDF calls — some signatures shifted in IDF v5.
- `WiFiClientSecure` certificate APIs — bundle/inline cert handling changed in some helpers.

Most app code that only touches Arduino-level APIs (`pinMode`, `digitalRead`, `Serial`, `WiFi`, etc.) is unaffected.

---

## 10. FastLED + IDF 5 RMT timing: phantom LEDs, varying count, animation glitches

**Symptoms:** colors on the LEDs are correct, but the *count* of lit LEDs is wrong and varies between frames. Often described as "lighting up a few extra LEDs past where it should stop." Animations look "squirrelly" or framing tears.

**Cause:** Arduino-ESP32 v3 / IDF 5 rewrote the RMT driver. FastLED talks to it via either the legacy compatibility shim (FastLED < 3.9) or the new RMT5 backend (FastLED ≥ 3.9). Both are vulnerable to interrupt preemption during `FastLED.show()` — when WiFi / LVGL / async-tcp interrupts fire mid-frame, the RMT FIFO can underrun, the reset gap shortens below the WS2812's 50µs threshold, and the next chip latches what should have been the gap as the start of more bits. Hence "extra LEDs," and the count varies because it depends on what was interrupting at that moment.

**The "obvious" fix (FastLED I2S parallel driver) often isn't available on ESP32-S3.** `-DFASTLED_ESP32_I2S=1` uses the LCD peripheral on S3. If your display is a parallel LCD (e.g. LovyanGFX `Bus_Parallel8` / `Bus_RGB`), you've already claimed that peripheral — they collide. SPI displays don't have this conflict.

**Practical mitigations, in order of safety/effort:**

1. **Pin down RMT (low risk, first thing to try):**
   ```ini
   build_flags =
       -DFASTLED_RMT_BUILTIN_DRIVER=1   ; keeps RMT memory mapping pinned in IRAM
       -DFASTLED_ESP32_FLASH_LOCK=1     ; pauses flash ops during show()
   ```
   Doesn't eliminate every glitch but fixes the bulk. Cost is a tiny IRAM bump and microsecond pauses to NVS/LittleFS writes during `show()`.

2. **Switch to I2S/LCD backend** (`-DFASTLED_ESP32_I2S=1 -DFASTLED_ESP32_I2S_NUM_DMA_BUFFERS=4`) — **only if you don't use a parallel LCD display**. DMA-driven, immune to interrupt preemption. Same FastLED API, same GPIO pin (S3's GPIO Matrix routes any peripheral signal to any pin).

3. **Clockless SPI backend** (`-DFASTLED_ESP32_USE_CLOCKLESS_SPI`) — uses an SPI controller (SPI2/SPI3), also DMA-driven, sidesteps the LCD peripheral. Less mature than I2S but worth trying when both RMT pinning and I2S are off the table.

4. **Pin `FastLED.show()` to a dedicated core** — if WiFi/LVGL run on core 1, pin LED rendering to core 0. Reduces interrupt collisions. Pairs well with #1.

See [FastLED issue #2082 — Flicker Under Wi-Fi Load](https://github.com/FastLED/FastLED/issues/2082) for the canonical discussion.

---

## 11. Package bumps that pair well with the migration

After the platform is healthy, consider:
- `lvgl/lvgl@^9.5.0` (was `^9.4.0`)
- `fastled/FastLED@^3.10.3` (was `^3.7.0`)
- `bblanchon/ArduinoJson@^7.4.2` — already current

Bump one at a time and rebuild — LVGL minor versions can shift style/API behaviour. FastLED 3.10 added RMT5 support for ESP32 which is the new default driver on IDF 5.x.

---

## Quick checklist for a fresh migration

1. [ ] Update `platform =` to pioarduino release zip URL
2. [ ] `pio --version` ≥ 6.1.19 (force-reinstall in penv if not)
3. [ ] `pio pkg uninstall -g -p espressif32` to evict old cache
4. [ ] Bump `lovyan03/LovyanGFX@^1.2.21`
5. [ ] Bump `esp32async/AsyncTCP` + `esp32async/ESPAsyncWebServer` floors in whichever library.json declares them (don't pin at project level if transitive)
6. [ ] Add `lib_ignore = RPAsyncTCP, ESPAsyncTCP`
7. [ ] Grep for project classes named `NetworkManager` / `NetworkClient` / `NetworkServer` / `NetworkEvents` / `NetworkInterface` — rename any collisions
8. [ ] Try `pio run -t buildfs` without the LittleFS pre-build hook
9. [ ] Build, fix any `ledc*` API breakages
10. [ ] If link succeeds but partition overflow — grow `partitions.csv` (2.5 MB OTA slots is a good default for 8MB boards). **Requires full USB flash, not OTA.**
11. [ ] If using FastLED with WiFi: add `-DFASTLED_RMT_BUILTIN_DRIVER=1 -DFASTLED_ESP32_FLASH_LOCK=1` to `build_flags` (or switch backend if your display doesn't claim the LCD/SPI peripheral)
12. [ ] Optional: bump LVGL / FastLED to current
