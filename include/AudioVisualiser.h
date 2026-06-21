#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <memory>
#include "VuGraph.h"

/**
 * @brief Idle screensaver that shows the VU meter full-screen.
 *
 * After the screen has been idle, a full-screen overlay (on LVGL's top layer)
 * appears showing a VuGraph — the same segmented VU meter as the VU tab, so it
 * renders just as cheaply (dirty-region segment updates, NOT a full-frame canvas
 * redraw). A tap dismisses it back to the controls.
 *
 * (Earlier this hosted four PSRAM-canvas visualisations; those were dropped —
 * full-screen per-frame redraw through QSPI PSRAM couldn't keep up. The VU meter
 * performs like the VU tab because it only repaints changed segments.)
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

    /** Reveal the screensaver. */
    void show();
    /** Hide the screensaver, returning to the controls. */
    void hide();
    bool isActive() const { return screensaverActive_; }

    /** Refresh the VU meter. Called by the render task while active. */
    void update();

    bool isInitialized() const { return initialized_; }

private:
    lv_obj_t* overlay_;
    std::unique_ptr<VuGraph> vuGraph_;
    bool initialized_;
    bool screensaverActive_;

    void cleanup();
    static void dismissEvent(lv_event_t* e);
};

extern AudioVisualiser* g_audioVisualiser;
