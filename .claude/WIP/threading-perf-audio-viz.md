# WIP: Threading rework for performance + audio visualisations

**Goal:** lift the project off its single cooperative `loop()` onto a task-based
architecture so (a) the UI repaints at full rate regardless of audio/web/LED work,
(b) audio is sampled at a *steady, high, jitter-free* cadence (the prerequisite for
good spectrum/beat visualisations), and (c) we kill a latent cross-task LVGL race
that already exists today.

This ports the **discipline** from the EverythingControl S3 work
(`D:\PlatformIO\Projects\EverythingControl\.claude\rules\ui-ownership.md`) — NOT the
hardware-specific bits. See "What does NOT transfer" below so we don't chase the
wrong things.

> Status: **threading rework COMPLETE and working on hardware (2026-06-21).** Steps
> 1–4 (race fix, tick decouple, render task, ~200 Hz audio task + `AudioBus` seqlock)
> all confirmed. Step 5 (DMA flush) investigated → no-op, skipped. Step 6 (viz): the
> fancy canvas visualisations were **dropped for performance** — final result is a
> **VU-meter idle screensaver** (same cheap rendering as the VU tab). Also fixed an
> OTA task-watchdog (suspend audio task during OTA). The **architecture is the win**:
> single LVGL owner, steady audio task, clean queue/seqlock hand-offs.
> **Step 5 (DMA flush) investigated and SKIPPED** — premise was wrong, the flush is
> already GDMA-driven (see step 5 below). **Step 6 (visualisations) — fancy canvas
> viz DROPPED for performance; final = VU-meter idle screensaver** (idle 30 s →
> full-screen `VuGraph`, tap to dismiss; same perf as the VU tab). PSRAM enabled
> (`qio_qspi`) but now unused. All steps 1–6 working on hardware as of 2026-06-21.
>
> **Pick-up-tomorrow notes:** (a) flash the VU-screensaver build + confirm LEDs keep
> animating while it's up (the heavy canvas viz had starved LED compute on core 1);
> (b) decide whether to keep PSRAM enabled or strip it; (c) optional polish: centre/size
> the screensaver VuGraph in the full-screen overlay, idle-timeout tuning; (d) the
> deferred bonus — LED animations matched to audio — was never started.
>
> **Update (2026-06-22):** (c) DONE. The screensaver VuGraph now centres in the
> full-screen overlay via `VuGraph::centerContentIn(LV_HOR_RES, LV_VER_RES)` (called
> from `AudioVisualiser::initialize`). It repositions ONLY the screensaver instance's
> canvas (the bars are absolutely positioned for the VU tab); the VU tab is untouched.
> The screensaver is now also a **web setting**: the web "LEDs" nav item is renamed
> **Settings**, and that page gained an *Idle Screensaver* card (enable + idle-timeout
> seconds). Persisted in NVS namespace `ui-settings` (`scrn_en`/`scrn_sec`), loaded in
> `UIManager::loadScreensaverConfig()`, updated live via `setScreensaverConfig()` (REST
> `GET /get-settings` + `POST /save-settings`). `screensaverEnabled_`/`screensaverIdleMs_`
> are volatile members (web writes on AsyncTCP, render task reads in `update()`); a
> live-disable dismisses the screensaver on the next render tick. Idle-timeout tuning
> (still open) is now adjustable from the web UI.

---

## Hardware reality — pins are SETTLED (read before proposing any pin change)

Physical board is a **WT32-SC01 Plus** (the PlatformIO `board = esp32-s3-devkitc-1`
in `platformio.ini` is just a generic S3 target; the real hardware is the SC01 Plus).
That board breaks out **very few free GPIOs**, and the user's wiring (display, touch,
backlight, the MSGEQ7 strobe=13 / RST=21 / analog=12, the LED data pin) is **settled.**
Treat the pin map as fixed and **exhaust every software mitigation first.** A pin move
is the **very last resort** — not impossible, but extremely annoying to the user, so
only land there once software options are genuinely exhausted and you've said so
explicitly. This tempers the earlier draft, which reached for "move the audio pin to
ADC1" too readily; see the ADC2 note in Gotchas.

## What does NOT transfer from EverythingControl (don't chase these)

EC's S3 gains were fixes for driving an **RGB-parallel panel from a PSRAM
framebuffer with a DMA bounce-refill ISR**. This board has none of that — the
WT32-SC01 Plus is an **ST7796 320×480 over an 8-bit 8080 parallel bus via
LovyanGFX** (matches `UIManager.cpp:75-152`), flushed from a tiny 6.4 KB
*internal-RAM* partial buffer (`UIManager.cpp:161,164-174,788`). So:

- ❌ **Render task pinned to core 0** (EC did this to avoid PSRAM-bus contention with
  the bounce ISR). No bounce ISR here → pin the render task to **core 1** (or 0;
  it's a free choice, see below), not for that reason.
- ❌ **Double framebuffer + DIRECT mode** — no panel-owned full framebuffer to
  pointer-flip. We stay in PARTIAL mode.
- ❌ Internal-DRAM render stack mandate, `SPIRAM_*` levers, mbedtls-in-PSRAM, the
  Y-shift/tearing/cyan fixes — all RGB-from-PSRAM specific. Irrelevant.
- ❌ The "≈2 fps → ≈31 fps single-owner" number. That was un-serialising `lv_*`
  calls that were **contending across tasks**. Here everything is already on one
  task, so there is **no free 15×** sitting there — our wins come from decoupling
  cadences, not from removing contention.

What **does** transfer is the rule set: **one task owns `lv_*`, everything else hands
it plain data over a queue, no LVGL mutex.**

---

## Current architecture (as-built, cite-checked)

Single Arduino `loop()` on core 1 (`ARDUINO_RUNNING_CORE` default), everything
serialized (`src/main.cpp:57-87`):

```
loop():
  lv_tick_inc(...)                         // main.cpp:60-63
  g_uiManager->update()                    // main.cpp:67
     ├─ OTA screen housekeeping (lv_*)     // UIManager.cpp:350-402
     ├─ vuGraph_->update()                 // UIManager.cpp:405-407
     │    ├─ readFrequencies()  → BLOCKING MSGEQ7 read (~0.5 ms)  // VuGraph.cpp:315-338
     │    │     └─ g_ledManager->updateVuLevels(...)              // VuGraph.cpp:336
     │    ├─ updateVuBars()     → lv_obj_set_style_* per segment  // VuGraph.cpp:256-313
     │    └─ getVuLevels()
     └─ lv_timer_handler()                 // UIManager.cpp:410  (render + touch indev)
  g_wifiBootManager->update()              // main.cpp:75
  g_ledManager->update()                   // main.cpp:76  (animation compute + driver_.show DMA)
  g_webUIManager->update()                 // main.cpp:77  (ws client cleanup)
  g_otaManager->loop()                     // main.cpp:78
```

Key facts:

- **Display:** single partial buffer `buf[320*10]` = 6.4 KB internal RAM, PARTIAL
  mode, **blocking** flush (`lcd.startWrite()/pushPixels(...,true)/endWrite()` —
  `pushPixels`' `true` is byte-swap, **not** DMA). `UIManager.cpp:161,164-174,783-792`.
- **Touch:** polled inside `lv_timer_handler` via `touchpadRead` indev cb
  (`UIManager.cpp:177-188,794-799`) → already runs on whatever task owns LVGL. Fine.
- **Audio:** DFRobot MSGEQ7-style analyzer, strobe=13 / RST=21 / analog=12
  (`VuGraph.cpp:12`). `Analyzer::ReadFreq` busy-waits **~546 µs** per call
  (7 bands × `delayMicroseconds(10+50+18)` — `AudioAnalyzer.cpp:74-82`) **plus** 7×
  `analogRead`. Called every loop from the render path → **sample rate == frame rate
  (~30–60 Hz), and it jitters** whenever web/LED/OTA work lands in the same loop.
- **LEDs:** `led_strip` RMT+DMA (`driver_.show` — already offloaded from CPU).
  Animation *compute* is on the loop (`LEDManager.cpp:96-120`).

### ⚠ THE LANDMINE — a cross-task LVGL race already exists

`WebUIManager`'s WebSocket handlers run in the **AsyncTCP task** (separate task,
not the loop — and `CONFIG_ASYNC_TCP_RUNNING_CORE` is **not** set, so it can be on
either core). They call straight into LVGL-touching code:

- `handleVuMessage` → `g_uiManager->setVuState()` → `vuButton_->setState()` → `lv_*`
  (`WebUIManager.cpp:396-401`, `UIManager.cpp:466-472`)
- `handleWhiteMessage` → `setWhiteState()` → `lv_*` (`WebUIManager.cpp:403-408`)
- `handleAnimationMessage` → `setAnimation()/setAnimationState()` → `lv_*`
  (`WebUIManager.cpp:423-432`, `UIManager.cpp:499-514`)
- `handleColorMessage` → `g_colourWheel->setColor()` → `lv_*` (`WebUIManager.cpp:434-440`)
- `handleBrightnessMessage` → `g_brightnessSlider->setBrightness()` → `lv_*`
  (`WebUIManager.cpp:410-421`)

With `LV_OS_NONE` and **no lock**, this is a genuine data race against the loop's
`lv_timer_handler` today. It "works" only because messages are infrequent and LVGL
ops are short. **The moment we add a render task this becomes a hard concurrency
bug — and it must be fixed regardless.** It is the central constraint of this rework,
not an afterthought.

(Also note `updateWebUi()` → `webSocket_.textAll()` is called from UI callbacks on
the loop *and* indirectly from web handlers — `WebUIManager.cpp:451-455`,
`128-135`. `textAll` is comparatively safe cross-task but keep it on one side once
we restructure.)

---

## Target architecture (the EC pattern, ported)

**Invariant 1 — single LVGL owner.** Exactly one task calls `lv_*`. Nothing else,
ever. Keep `LV_OS_NONE`, add **no** LVGL mutex. If you reach for a lock, you're
about to put an `lv_*` call on the wrong task — fix that instead.

**Invariant 2 — cross-task traffic is plain data over queues**, never shared
mutable LVGL/state. Two channels only:

| Channel | Producer | Consumer | Payload |
|---|---|---|---|
| `AudioFrame` (double-buffer or queue, latest-wins) | Audio task | Render task **and** LED animation | `{ bands[7], overall, beat? , seq }` — POD |
| UI command queue (FreeRTOS `QueueHandle_t`) | AsyncTCP web handlers | Render task | `{ UiCmd kind, int/bool/colour payload }` — POD |

### Proposed task layout (decision points flagged)

```
Core 0 ── Audio task (NEW)      : fixed-cadence MSGEQ7 read + ExponentialFilter,
                                  publish AudioFrame. Steady period (e.g. 5–10 ms
                                  = 100–200 Hz), vTaskDelayUntil for no jitter.
        ── WiFi / AsyncTCP       : (already its own task; web handlers post UiCmds)

Core 1 ── Render task (NEW, SOLE lv_* owner):
                                  owns lv_tick + lv_timer_handler; drains UI command
                                  queue; reads latest AudioFrame; drives VU widget +
                                  future viz. Priority above loopTask so it preempts.
        ── Arduino loopTask      : LEDManager::update, WebUIManager::update,
                                  OTAManager::loop  (NO lv_* — see migration)
```

**Decision points for next session (pick during design, note in code comments):**

1. **Where does LED animation compute live?** It needs the same `AudioFrame` the
   render task does. Options: (a) leave on `loopTask` reading `AudioFrame` (simplest);
   (b) give it its own core-0 task next to audio (best if animation compute grows).
   Recommend (a) first, measure, escalate to (b) only if LED stutters.
2. **Render task core.** Core 1 (preempt loopTask) is the obvious default. Core 0 is
   fine too (no bounce-ISR reason to avoid it here) but then it competes with WiFi —
   prefer core 1.
3. **`lv_tick`:** move `lv_tick_inc` out of `loop()` into the render task, **or** set
   `LV_TICK_CUSTOM` to `millis()` in `lv_conf.h` (cleaner — tick source becomes
   task-agnostic). Recommend `LV_TICK_CUSTOM`.
4. **AudioFrame hand-off mechanism:** a 2-slot double buffer with an atomic
   `seq`/index flip is lighter than a queue for "latest value wins" and is what the
   render+LED consumers want (they don't need every sample, just the freshest).
   A `QueueHandle_t` of length 1 with overwrite also works. Pick one.

---

## Migration steps (suggested order — each is independently testable)

1. ✅ **DONE (2026-06-21, shipped via OTA).** Fixed the existing race first, before
   any new task. Implemented as:
   - New POD header `include/UiCommand.h` — `UiCommand { UiCommandType type; bool
     boolValue; int intValue; char colour[8]; }` (trivially copyable, sent by value
     through the queue).
   - `UIManager` owns a `QueueHandle_t uiCommandQueue_` (`xQueueCreate(16, …)` in the
     ctor, `vQueueDelete` in dtor, handle transferred in both move ops). New public
     `postUiCommand()` (non-blocking `xQueueSend(…,0)`, drops on overflow) + private
     `drainUiCommands()`/`applyUiCommand()`. `update()` drains the queue **before**
     any `lv_*`.
   - All five `WebUIManager` handlers (`vu`/`white`/`brightness`/`animation`/`colour`)
     now just build a `UiCommand` and post it — **zero `lv_*` reachable from the
     AsyncTCP task**. `applyUiCommand()` reproduces the prior per-handler behaviour
     (incl. the `g_brightnessSlider` vs `g_ledManager` fallback and where
     `updateWebUi()` fires).
   - Net effect: producer (AsyncTCP) → POD over FreeRTOS queue → consumer (loop).
     The queue's internal critical sections make the cross-task hand-off safe (the
     old direct `setX()` calls lacked exactly that). Behaviour unchanged; web control
     changes now land on the next `update()` tick instead of synchronously.
   - Verified on hardware: web controls drive the UI + reflect to other clients,
     touch still works.
   - ⚠ Note for later: in captive-portal/setup mode `update()` returns before the
     drain (`!initialized_`), so posted commands accumulate and are dropped past 16.
     Harmless today (no main-control WS in setup mode), but revisit if the render
     task ever needs to drain independently of `initialized_`.
2. ✅ **DONE (2026-06-21).** Decoupled the tick from `loop()`. Note: this lv_conf.h
   is **LVGL v9, which has no `LV_TICK_CUSTOM` macro** — v9 replaced it with a runtime
   callback `lv_tick_set_cb(uint32_t(*)(void))`. Implemented as:
   - `UIManager.cpp` file-local `static uint32_t lvglTickCallback() { return millis(); }`
     (wrapped, not raw `millis`, because `millis()` returns `unsigned long` ≠ `uint32_t`
     and the fn-ptr types must match exactly — zero-warnings).
   - Registered via `lv_tick_set_cb(lvglTickCallback)` right after `lv_init()` in
     `initializeScreen()` (runs first in `setup()`, before any `lv_timer_handler`).
   - Removed the `lv_tick_inc` + `lastTick` bookkeeping from `loop()`.
   - Safe vs the ESP32WifiSetup library if it still calls `lv_tick_inc`: when a tick
     cb is set, v9's `lv_tick_get()` uses the cb and ignores the inc'd counter.
3. ✅ **DONE (2026-06-21).** Spun up the render task as the sole `lv_*` owner.
   Implemented as:
   - `UIManager::startRenderTask()` → `xTaskCreatePinnedToCore(renderTaskTrampoline,
     "lvgl_render", 10240 bytes, this, prio 2, core 1)`. The trampoline loops
     `update()` + `vTaskDelay(5ms)` (~200 Hz cap). Handle in `renderTaskHandle_`
     (nullptr until started); `isRenderTaskRunning()` exposes it; dtor/move ops
     `vTaskDelete` it before tearing down lv_* objects.
   - The **whole** `UIManager::update()` moved to the render task (UI-cmd drain +
     OTA-screen block + `vuGraph_->update()` + `lv_timer_handler`). Nothing changed
     inside `update()` itself — only *who calls it*.
   - `main.cpp`: render task is started at the **end of `setup()`**, only in normal
     mode, **after** `syncWithLEDState()` so no startup lv_* races the task. `loop()`
     now skips all LVGL when `isRenderTaskRunning()`; in setup mode (no render task)
     it drives LVGL on the loop exactly as before, keeping the boot UI alive.
   - OTA flags (`otaScreenActive_`/`otaPendingProgress_`/`otaProgressChanged_`) marked
     `volatile` — written from AsyncTCP (OTA progress during web upload), read on the
     render task. The OTA *screen objects* are only ever touched on the render task.
   - `loop()` yields `vTaskDelay(1)` in the render-owned branch so loopTask can't
     100%-spin core 1 / starve IDLE1 when no LEDs are configured (when they are,
     `driver_.show()`'s RMT tx-done already paces it).
   - **Verified `lv_*` ownership:** `WifiBootManager.cpp` and `LEDManager.cpp` have
     **zero** `lv_*`; web handlers post commands (step 1). So in normal mode the only
     `lv_*` caller is the render task. ✓
   - ⚠ **KNOWN BENIGN TRANSIENT (closed by step 4 + a possible LED-control step).**
     Moving `vuGraph_->update()` and web-command application onto the render task
     means **LEDManager state is now touched from two tasks**: the render task writes
     it (VU levels via `updateVuLevels`; control flags/`leds_` via UI callbacks +
     drained web cmds) while `LEDManager::update()` reads it on loopTask. All of it is
     plain POD (`int vuLevels_[7]`, `int audioLevel_`, bool/enum flags, the fixed
     `leds_` buffer — never realloc'd at runtime), so on ESP32 these are atomic
     aligned accesses with a forgiving consumer: worst case a **one-frame visual
     glitch, no crash**. Step 4's `AudioFrame` hand-off closes the VU half; routing
     LED *control* through a queue (or moving LED compute to consume commands) would
     close the rest if it ever proves visible — measure first.
   Test: UI renders smoothly with web + LED running; touch + web controls both drive
   the UI; OTA screen still works; no IDLE1 watchdog panic. ✅ **Confirmed on hardware
   2026-06-21** (along with step 2's tick decouple — they were flashed together).
4. ✅ **DONE (2026-06-21).** Spun up the audio task on core 0; VU widget + LEDs are
   now pure consumers. Implemented as:
   - New `include/AudioFrame.h` — POD `{ int bands[7]; int overall; uint32_t seq; }`.
   - New `AudioBus` (`include/AudioBus.h` + `src/AudioBus.cpp`) — a **seqlock**
     (single producer, multi consumer, latest-wins). `publish()` from the audio task,
     `latest()` from render + loop. Reader retries if a publish straddled its copy, so
     a returned frame is never a torn mix of old/new bands. `extern AudioBus g_audioBus;`
     (chose the seqlock over a 2-slot index flip — same idea, provably tear-free for a
     ~40-byte struct, cheap retry).
   - New audio task (`include/AudioTask.h` + `src/AudioTask.cpp`): owns the `Analyzer`
     (pins 13/21/12) + the 7 band `ExponentialFilter`s + overall filter (the filters'
     **one home** — consumers must not re-filter). `vTaskDelayUntil` at **5 ms / ~200 Hz**
     steady. Core 0, prio 1, 4 KB stack. Started in `main.cpp` setup() (normal mode),
     **before** the render task so frames flow first.
   - `VuGraph` gutted to a consumer: removed `Analyzer`/`filters_`/`audioFilter_`/
     `audioLevel_` and the dead `readFrequencies`/`getOverallVolume`/`getVuLevels*`/
     `getVuValue` (the strip-remap getters were dead code — they overwrote `vuValues_`
     after it had already been pushed, never read). `update()` now does
     `g_audioBus.latest()` → `vuValues_[i] = frame.bands[i]` → `updateVuBars()`.
     Dropped the `AudioAnalyzer.h`/`Filter.h` includes from `VuGraph.h`.
   - `LEDManager`: removed `updateVuLevels()`; `update()` now pulls `g_audioBus.latest()`
     into `vuLevels_`/`audioLevel_` at the top — every animation reads them unchanged.
   - **Behaviour preserved exactly** (verified by tracing the old push path): the audio
     task publishes the same 7 raw-filtered bands + same overall-volume calc the old
     code pushed to the LED and drew on the meter.
   - ⚠ **This closes the step-3 benign VU race** — the AudioFrame seqlock is now the
     formal hand-off for the audio→{render,LED} data. (LED *control* state from
     UI/web is a separate, still-benign case; revisit only if visible.)
   - ⚠ **Tuning watch:** filters now update at ~200 Hz instead of the old ~30–60 Hz
     frame rate, so their effective smoothing time-constant shrank ~3–6× → the meter
     will feel **snappier/jumpier**. If it's too jittery, bump the `ExponentialFilter`
     weight (more smoothing) or lower `SAMPLE_PERIOD_MS`. Decide on hardware.
   - ⚠ **ADC2 gotcha — CONFIRMED via OTA watchdog (2026-06-21).** GPIO12 = ADC2, shared
     with WiFi. Normal use is fine, BUT during **OTA** (WiFi saturated) the audio task's
     blocking `analogRead(ADC2)` on **core 0** contends badly and, with the flash-write
     IPC stalls, **starved IDLE0 → task watchdog → reboot → failed OTA** (`task_wdt:
     IDLE0 (CPU 0)`, `CPU 0: ipc0`). This is a regression from moving audio sampling to
     core 0 (it used to run on the loop/core 1). **Fix:** `pauseAudioTask()` /
     `resumeAudioTask()` (vTaskSuspend/Resume the audio task) called from the OTA
     start/end callbacks in `WebUIManager.cpp` (suspend after `setOTAMode(true)` so no
     one reads `g_audioBus` while it's parked). The steady-state hold-last-sample tier-1
     mitigation still isn't needed for normal operation.
   Test: VU meter + audio-reactive LED animations track audio smoothly and
   independently of frame rate; no audio dropouts with WiFi active. ✅ **Confirmed on
   hardware 2026-06-21** — meter feel good at 200 Hz (no filter retune needed), ADC2
   sampling fine alongside WiFi (tier-1 hold-last not needed).
5. ⛔ **INVESTIGATED — premise was wrong; not implemented (2026-06-21).** The plan
   assumed `lcd.pushPixels(...,true)` is a non-DMA blocking write and switching to
   `pushPixelsDMA` would be a win. **Cite-checked the installed LovyanGFX and that's
   false for this path:**
   - `displayFlush` uses swap=true → `LGFXBase::writePixels` → `Panel_LCD::writePixels`
     with `param->no_convert == false` (conversion needed) → falls through to
     `_bus->writePixels()` (`Panel_LCD.cpp:279`).
   - `Bus_Parallel8::writePixels` (esp32s3) **already DMAs**: it converts pixels in
     chunks into `_cache` flip buffers and calls `writeBytes(..., use_dma=true)`, which
     sets up **GDMA descriptor links** (`_setup_dma_desc_links` / `DMA_OUTLINK_START`,
     `Bus_Parallel8.cpp:343,377-378`). Conversion of chunk N+1 pipelines with the GDMA
     of chunk N (`while (... & LCD_CAM_LCD_START){}` gate at line 371).
   - The panel-level `use_dma` flag (`writePixelsDMA` vs `writePixels`) is **ignored
     when conversion is required** — it only matters for `no_convert` direct blits. With
     our mandatory byte-swap, `writePixelsDMA` is a **no-op change**.
   - `endTransaction()`/`wait()` busy-wait on `LCD_CAM_LCD_START` (`Bus_Parallel8.cpp:209-219`)
     → the flush is inherently synchronous, but the CPU isn't idle — it's doing the
     pipelined swap-conversion that feeds the DMA.
   So the only theoretical remaining win is overlapping LVGL's *render of the next area*
   with the *transfer of the current area*, which needs an invasive persistent-transaction
   + double-buffer + immediate-`flush_ready` async restructure (busy-wait `endTransaction`
   fights it) with real display-corruption risk. **Verdict: skip.** A safe, low-risk
   micro-win if flush overhead ever matters: enlarge the partial buffer
   (`buf[screenWidth * 40]` ≈ 25.6 KB vs 6.4 KB) to amortize per-flush `setAddrWindow`
   command overhead across fewer, larger DMA bursts — but measure first; not done.
6. ✅ **DONE — VU-meter idle screensaver (2026-06-21).** Scope evolved several times;
   **FINAL DECISION: the fancy canvas visualisations were DROPPED.** The full-screen
   PSRAM canvas (4 styles) couldn't perform — full-frame redraw every frame through
   **QSPI PSRAM** is too slow, even at quarter-res. The screensaver now shows the
   **VU meter (`VuGraph`) exactly as on the VU tab**, which renders cheaply (dirty-region
   segment updates) — same perf as the VU tab.
   **FINAL design:**
   - `AudioVisualiser` is now a thin **full-screen idle screensaver on `lv_layer_top()`**
     hosting its own `VuGraph` instance (a 2nd VuGraph; the tab keeps `g_vuGraph`).
   - **Idle trigger:** `s_lastTouchMs` in `touchpadRead`; `UIManager::update` shows it
     after **30 s** untouched (suppressed during OTA). **Tap = dismiss** (one viz now, so
     no swipe). While active, the tab's VuGraph update is skipped (it's covered).
   - **No canvas, no PSRAM use** for the screensaver anymore. The `update()`/`show()`/
     `hide()`/`isActive()` interface UIManager calls is unchanged.
   - ⚠ **LED-animations-stopped finding (user-reported):** while the heavy canvas viz was
     up, LED animations froze. Cause: the **render task (core 1, prio 2) starved the
     Arduino loopTask (core 1, prio 1)** which runs `LEDManager::update` — heavy per-frame
     render work on core 1 left no time for LED compute. The light VU-meter screensaver
     should not do this (same cost as the VU tab, which is fine). If LED starvation ever
     recurs under load, the fix is WIP decision #1(b): **move LED compute to its own
     core-0 task** consuming `g_audioBus`, or lower the render task priority.
   - ⚠ **PSRAM is now enabled (`qio_qspi`) but UNUSED** — it was only for the dropped
     canvas. Harmless (boots fine, gives headroom); remove from platformio.ini if desired.
   - **Superseded canvas/screensaver design notes (kept for reference):**
   - `AudioVisualiser` is now a **full-screen overlay on `lv_layer_top()`**, hidden
     until the screen is idle. The Visualiser **tab was removed** (back to Colour + VU).
   - **Idle trigger:** `touchpadRead` stamps `s_lastTouchMs`; `UIManager::update` shows
     the overlay after **30 s** with no screen touch (suppressed during OTA).
   - **Gestures:** swipe left/right = next/prev style; **tap = dismiss** to controls.
     NB: LVGL's GESTURE-vs-CLICKED event ordering proved unreliable (every swipe read
     as a tap and dismissed), so this is done manually from `LV_EVENT_PRESSED`/
     `LV_EVENT_RELEASED` press/release coordinates: horizontal travel ≥45 px = swipe,
     <22 px = tap, else ignored. A style-name label shows for 1.5 s on activate/change.
   - **PSRAM enabled** (`board_build.arduino.memory_type = qio_qspi` + `-DBOARD_HAS_PSRAM`
     in platformio.ini) so the **full-res 320×480 RGB565 canvas (~300 KB) lives in
     PSRAM** (`MALLOC_CAP_SPIRAM`), allocated lazily on first show. **NB: this board's
     module is QUAD (QSPI) PSRAM, not octal** — `qio_opi` failed at boot with
     `octal_psram: PSRAM chip is not connected` / `size=0`; `qio_qspi` is correct.
     `main.cpp` logs `PSRAM: found=… size=…` at boot for confirmation. No upscaling → the
     image-transform cost that made the earlier scaled version "rough" is gone.
   - **Efficient drawing:** direct RGB565 buffer writes, a precomputed bottom-green→
     top-red **gradient palette** (no per-pixel HSV), `memset` clears, `memmove` scroll.
     ~33 fps cap. Four styles unchanged in spirit (Spectrum/Oscilloscope/Waterfall/Radial).
   - ⚠ **PERF (2026-06-21): native full-res 320×480 was too slow** (user: "isn't
     performant" vs the smooth VU tab). Root cause: full-screen redraw every frame
     `memset`s + composites ~300 KB through **QSPI PSRAM** (much slower than internal
     SRAM). **Fix:** render the canvas at **quarter res (160×240) and upscale 2×
     nearest-neighbor** (`lv_image_set_scale(512)` + `lv_image_set_antialias(false)`) →
     ~4× less PSRAM traffic (clear 300 KB→77 KB; composite source 4× smaller). If still
     not smooth enough, next step is **80×120 in *internal* RAM scaled 4×** (internal
     SRAM ~10× faster than QSPI PSRAM; chunky-but-very-smooth, no PSRAM at all).
   - ⚠ **PSRAM RISK (first flash):** assumes the module is an **R8/OPI** part (WT32-SC01
     Plus = N16R8, almost certainly). If it **boot-loops**, the memory_type is wrong —
     revert `board_build.arduino.memory_type` to the board default. The visualiser also
     logs `PSRAM free …` on first show and falls back to a "needs PSRAM" label if the
     `MALLOC_CAP_SPIRAM` alloc returns null.
   - ⚠ **Other verify items:** (a) swipe gestures register on the overlay; (b) full-res
     PSRAM redraw actually holds ~30 fps (PSRAM bandwidth — tune fps/res if rough);
     (c) tap dismisses cleanly; (d) screensaver shows after 30 s and not over OTA.
   - **Earlier tab-based cut (superseded):**
   - New `AudioVisualiser` component (`include/AudioVisualiser.h` + `src/ui/AudioVisualiser.cpp`),
     mirroring the VuGraph component pattern. Self-manages `g_audioVisualiser`.
   - Lives on a **new third tab "Visualiser"** (`tab3_` in UIManager) alongside Colour + VU.
   - Renders to **one `lv_canvas`** (native 160×190 RGB565 ≈ 60 KB, dynamically
     allocated from internal RAM with a **graceful "low memory" fallback label**),
     scaled 2× via `lv_image_set_scale` to fill the tab (320×380).
   - **Four switchable styles** (selector = row of 4 buttons, active highlighted on
     change only — no per-frame style calls): **Bars** (spectrum + falling peak caps),
     **Scope** (waveform synthesized from the 7 bands as harmonics), **Wave**
     (scrolling spectrogram via buffer `memmove` + new top row), **Ring** (radial rays
     blooming from centre + pulsing core).
   - Driven by `g_audioBus.latest()` on the render task (sole `lv_*` owner — no locks).
     `update()` is **frame-capped to ~40 fps** (render task calls it at 200 Hz) and is
     **gated to only run when the Visualiser tab is active** (`lv_tabview_get_tab_active==2`).
   - ⚠ **RAM REALITY (first flash, 2026-06-21): internal RAM is ~65 KB free and there
     is NO PSRAM configured in platformio.ini.** The first cut's 60 KB canvas allocated
     fine but then **starved the render task's 10 KB stack** → "Failed to create LVGL
     render task" (UI fell back to running on the loop — graceful, but the threading
     win was lost that boot). Fix: canvas dropped to **100×120 RGB565 ≈ 24 KB scaled 3x**,
     plus a **free-heap guard** in `createCanvas()` that refuses to allocate (shows the
     fallback label) unless `heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >= bufSize +
     24 KB reserve`, so it can never starve the render task again. It also logs the real
     free-heap number. **Upgrade path for full-res viz: enable the board's 8 MB PSRAM**
     (`board_build.arduino.memory_type = qio_opi` + `-DBOARD_HAS_PSRAM`; confirm the
     module is an R8 OPI part first) and allocate the canvas with `MALLOC_CAP_SPIRAM` —
     then VIZ_W/H can be much larger. Not done; needs a build-config change + module
     confirmation.
   - ⚠ **Hardware-verify list (next flash):** (a) the render task now starts (watch the
     log for the free-heap line + no "Failed to create LVGL render task"); (b) the `lv_image_set_scale`
     layout fills the tab correctly without clipping/offset (the one uncertain LVGL
     bit — quick fix if wrong); (c) frame rate / smoothness of the scaled 320×380
     redraw at 40 fps (drop fps or scale if heavy); (d) visual feel of each style
     (colours, sensitivity, peak decay — all easy to tune).
   - **Still TODO after screen viz is liked:** matching LED animations (bonus the user
     asked for — e.g. spectrum/waterfall on the strips echoing the screen). The LED
     side already has the 2D matrix (`xyToIndex`) + `vuLevels_`/`audioLevel_` from the
     audio bus, so a matched LED viz is straightforward to add next.

---

## Gotchas / verification items (don't skip)

- **ADC2 + WiFi (pin is FIXED — software-mitigate, don't move it).** Audio analog
  pin is **GPIO12 = ADC2_CH1** on the S3, and it's settled on the WT32-SC01 Plus.
  ADC2 is shared with the WiFi radio, so `analogRead` on ADC2 can intermittently
  fail/return stale while WiFi is active. **But it already works today** — the device
  ships with audio-reactive modes sampling GPIO12 on the loop with WiFi up — so the
  task split shouldn't *regress* it; the risk is only that moving sampling into its
  own task changes the timing relative to WiFi activity. Order of response if it
  degrades under the task split:
  1. **Tolerate it in software** — the `ExponentialFilter` already smooths; on a
     failed/implausible read, **hold the last good sample** (skip the filter update)
     instead of feeding a 0/garbage spike. Cheap, no wiring change.
  2. **Retry / pace the read** — a couple of quick retries on failure, and/or keep the
     audio-task cadence off the WiFi beacon interval; consider the IDF `adc_oneshot`
     API (better ADC2-with-WiFi behaviour on S3 than Arduino `analogRead`).
  3. **Last resort only:** move the analog input to an **ADC1** pin (GPIO1–10). This
     means re-soldering on settled hardware — extremely annoying to the user. Only
     after 1–2 are proven insufficient, and call it out explicitly before doing it.
  Verify on hardware early so we know which tier we're in before building the viz on
  top.
- **No `lv_*` outside the render task — enforce it.** After step 3, grep the tree:
  the only files calling `lv_*` should be the render task + UI component classes it
  invokes *on that task*. `WebUIManager.cpp` and `LEDManager.cpp` must call **zero**
  `lv_*` (today `WebUIManager` violates this transitively via `UIManager` setters —
  step 1 fixes it).
- **`delayMicroseconds` in the audio task is fine** (it's that task's own time now),
  but prefer `vTaskDelayUntil` for the *frame period* so the cadence is jitter-free;
  keep the per-band `delayMicroseconds` (MSGEQ7 settle timing — load-bearing,
  `AudioAnalyzer.cpp:76-81`).
- **Stack sizes:** render task needs a real stack for LVGL (start ~8–12 KB, watch
  `uxTaskGetStackHighWaterMark`). Audio task is small (~3–4 KB).
- **`ExponentialFilter` state** moves with the sampling into the audio task; the
  render/LED consumers must not also filter (double-filtering = laggy meter). Decide
  one home for the filters (audio task).
- **Watch the `cpp-embedded.md` rule:** callbacks unregistered in dtors, `Logger`
  not `Serial`, no blocking in async/web callbacks (posting a `UiCmd` is non-blocking
  — good). See `.claude\rules\async-events.md` + `lvgl.md`.
- **OTA→UI path is ALREADY the good pattern — and a shared-library decision.**
  The OTA screen callbacks (`WebUIManager.cpp:78-88`) don't touch `lv_*` directly:
  `showOTAScreen/updateOTAProgress/hideOTAScreen` only **set flags**
  (`otaScreenActive_/otaPendingProgress_/otaProgressChanged_`,
  `UIManager.cpp:845-863`) that get applied inside `UIManager::update` on the UI task
  (`UIManager.cpp:350-402`). That is exactly the post-data-then-apply-on-UI-task model
  the web *control* handlers must adopt — **use OTA as the worked example**, don't
  rewrite it. After the rework, the only hardening it needs is making those flags
  cross-task-safe (mark `volatile`/atomic): they're written from the **AsyncTCP task**
  (web-upload OTA fires progress from there) and read from the **render task**.
  - **Where any OTA fix belongs:** `OTAManager` lives in the shared **ESP32WifiSetup**
    library (`D:\PlatformIO\Projects\ESP32WifiSetup`, angle-include `<OTAManager.h>`),
    not this repo. If we end up needing more than volatile flags — e.g. a formal
    thread-safe progress contract so any consumer reads OTA progress safely from any
    task — **do that in ESP32WifiSetup**, since the bespoke per-app flag plumbing is a
    smell the library could own once. The app keeps only its UI wiring. ⚠ But weigh
    the **fork-sync cost**: ESP32WifiSetup has an Arduino fork (this project) and an
    IDF sibling (`ESP32WifiSetup-IDF`); a public-API change to `OTAManager` may need
    mirroring per the dependencies sync contract. For "just make progress readable
    safely" the change is likely small enough to not warrant a library API change —
    decide deliberately, don't reflexively put it in either place.
  - The `delay(200)` red-flash loop in the OTA end callback (`WebUIManager.cpp:96-101`)
    runs on AsyncTCP and only touches LEDs (not `lv_*`) — leave it, but confirm it
    doesn't fight the render task / LED task for the LED driver.
  - **Contrast — the web *control* handlers are app-local and stay here.** The race
    fix in step 1 (UI command queue + converting `handleVuMessage` etc.) is purely
    `WebUIManager`/`UIManager` and does **not** belong in any library.

---

## Quick reference — files this touches

- `src/main.cpp` — task creation, strip LVGL out of `loop()`.
- `src/ui/UIManager.cpp` / `include/UIManager.h` — render task body, UI command queue
  drain, OTA-screen block (`350-402`), `update()` (`344-411`).
- `src/ui/VuGraph.cpp` — split sample (→ audio task) from draw (→ render task).
- `src/AudioAnalyzer.cpp` / `.h` — MSGEQ7 read, now called from the audio task.
- `src/WebUIManager.cpp` — convert handlers (`396-440`) to post `UiCmd`s.
- `src/LEDManager.cpp` — consume `AudioFrame` instead of being pushed VU via
  `updateVuLevels` from the sample path.
- `include/lv_conf.h` — `LV_TICK_CUSTOM` (decision #3).
- New: an `AudioFrame` POD + a `UiCmd` POD (small headers).

Cross-project source of the pattern (read for the "why"):
`D:\PlatformIO\Projects\EverythingControl\.claude\rules\ui-ownership.md`.
