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

> Status: **investigation done, no code written.** Device was offsite when this was
> written; start here next session. Author the change, then the user builds/flashes
> and we test on hardware.

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

1. **Fix the existing race FIRST, before any new task** (de-risks everything and is
   correct on its own). Add a UI command queue drained in `UIManager::update()` on
   the loop. Convert every WebSocket handler in `WebUIManager.cpp` from calling
   `g_uiManager->setX()/g_colourWheel->...->setColor()/g_brightnessSlider->...` to
   **posting a `UiCmd`**. The loop drains and applies (still single-task at this
   stage, so still safe). Now *all* `lv_*` mutation originates on the loop. Test:
   web controls still drive the UI; touch still works.
2. **Introduce `LV_TICK_CUSTOM` (or move `lv_tick_inc`)** so the tick no longer
   depends on `loop()`.
3. **Spin up the render task** as the sole `lv_*` owner: move `lv_timer_handler` +
   the UI-command drain + the VU-widget refresh into it; pin core 1, priority >
   loopTask. Remove `lv_timer_handler`/`vuGraph_->update()` LVGL bits from the loop
   path. The OTA-screen block in `UIManager::update` (`UIManager.cpp:350-402`) is
   `lv_*` → it moves onto the render task too (it's already flag-driven —
   `showOTAScreen/updateOTAProgress/hideOTAScreen` just set flags, perfect for this).
   Test: UI renders smoothly with web + LED running.
4. **Spin up the audio task** on core 0: move the MSGEQ7 read + filtering out of
   `VuGraph::readFrequencies` into the task; publish `AudioFrame`. `VuGraph`/LED now
   *consume* the frame instead of sampling. `vuGraph_` widget update reads the frame
   on the render task; `g_ledManager->updateVuLevels` reads it wherever LED compute
   lands. Test: VU meter + audio-reactive LED animations track audio smoothly and
   independently of frame rate.
5. **Only then** consider the orthogonal display win: **double draw buffer + DMA
   flush** (`lcd.pushPixelsDMA` + a second `buf`, or LovyanGFX async write) so render
   overlaps bus transfer instead of blocking on it. Independent of the threading
   work; do it after the task split is stable so you can measure its effect alone.
6. **New visualisations** (the actual goal): with a steady 100–200 Hz `AudioFrame`
   you can add beat detection, spectrum history/waterfall, particle/peak effects, etc.
   — on core 0 (analysis) feeding POD to the render task (draw). Keep the invariant:
   analysis tasks never touch `lv_*`.

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
