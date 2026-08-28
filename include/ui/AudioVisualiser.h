#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <memory>
#include "ui/VuGraph.h"
#include "ui/saver/ScenePlayer.h"

/**
 * @brief Idle screensaver that shows the VU meter full-screen.
 *
 * After the screen has been idle, a full-screen overlay (on LVGL's top layer)
 * appears showing a VuGraph — the same segmented VU meter as the VU tab, so it
 * renders just as cheaply (dirty-region segment updates, NOT a full-frame canvas
 * redraw). A tap dismisses it back to the controls.
 *
 * Two kinds of visual live here:
 *
 * - **Scene** (default) — a procedural full-screen visual pushed straight to the
 *   panel by ScenePlayer, bypassing LVGL. This is what replaced the four
 *   PSRAM-canvas visualisations that were dropped years back: those painted a
 *   320x480 canvas in PSRAM and had LVGL read it back to blit, two QSPI
 *   round-trips of 300 KB a frame. Rendering strips out of internal RAM instead
 *   costs ~16 ms of the 33 ms frame, so the hardware was never the limit.
 * - **VuMeter** — the full-screen segmented VU meter, kept because it is cheap
 *   (dirty-region segment updates only) and still looks good.
 *
 * Runs on the render task (the sole lv_* owner).
 */
class AudioVisualiser {
public:
    AudioVisualiser();
    ~AudioVisualiser();

    AudioVisualiser(const AudioVisualiser&) = delete;
    AudioVisualiser& operator=(const AudioVisualiser&) = delete;

    /** Build the (hidden) overlay + VU meter under @p topLayer (lv_layer_top()). */
    bool initialize(lv_obj_t* topLayer);

    /// Which visual the screensaver shows.
    enum class Mode : uint8_t { Scene, VuMeter };

    void setMode(Mode mode);
    Mode mode() const { return mode_; }

    /// Cycle to the next visual, wrapping through the VU meter too — this is
    /// what the swipe uses.
    void nextScene();

    /// Flat selection for the settings UI: 0 is the VU meter, 1..N are the
    /// procedural scenes in ScenePlayer order.
    int selectionCount() const;
    const char* selectionName(int index) const;
    int selection() const;
    void setSelection(int index);

    /**
     * Touch handling: a TAP dismisses, a horizontal SWIPE changes scene. Both are
     * decided from the press and release points rather than LVGL's gesture
     * detection — the overlay is full-screen, so any swipe both starts and ends
     * inside it and LVGL counts that as a click. Relying on LV_EVENT_GESTURE
     * firing before LV_EVENT_CLICKED meant a swipe dismissed the screensaver.
     */

    /** Reveal the screensaver. */
    void show();
    /** Hide the screensaver, returning to the controls. */
    void hide();
    bool isActive() const { return screensaverActive_; }

    /** Refresh the VU meter. Called by the render task while active. */
    void update();

    bool isInitialized() const { return initialized_; }

private:
    /// Travel that separates a swipe from a tap, in pixels.
    static constexpr int32_t SWIPE_MIN_PX = 45;
    static constexpr int32_t TAP_MAX_PX = 18;

    lv_obj_t* overlay_;
    std::unique_ptr<VuGraph> vuGraph_;
    std::unique_ptr<ScenePlayer> scenePlayer_;
    Mode mode_ = Mode::Scene;
    bool initialized_;
    bool screensaverActive_;

    void cleanup();
    void applyMode();
    static void pressEvent(lv_event_t* e);
    static void releaseEvent(lv_event_t* e);

    /// Where the current touch went down, so release can tell a tap from a swipe.
    lv_point_t pressPoint_ = { 0, 0 };
};

extern AudioVisualiser* g_audioVisualiser;
