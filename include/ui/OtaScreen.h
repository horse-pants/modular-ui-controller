#pragma once

#include <lvgl.h>
#include <stdint.h>

/**
 * @brief Full-screen OTA progress overlay (title + progress bar).
 *
 * Flag-driven, matching the rest of the UI's cross-task model: show() /
 * updateProgress() / hide() are called from the OTA callbacks on the AsyncTCP
 * task and only set volatile flags. apply() runs on the render task (the sole
 * lv_* owner) and actually creates / updates / destroys the LVGL objects.
 */
class OtaScreen {
public:
    void show();                       ///< request the overlay (resets to 0%)
    void updateProgress(uint8_t pct);  ///< request a progress update
    void hide();                       ///< request the overlay be removed

    bool isActive() const { return active_; }
    bool changed() const { return changed_; }

    /// Apply any pending show / update / hide to LVGL. Render task only.
    void apply();

    /// Immediately delete the LVGL objects (for UIManager teardown).
    void teardown();

private:
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* label_ = nullptr;
    lv_obj_t* bar_ = nullptr;

    // Written from the AsyncTCP task (OTA progress callbacks during a web
    // upload), read/applied on the render task. volatile so the reader sees the
    // latest value; aligned bool/uint8_t writes are atomic on ESP32, and a
    // missed "changed" edge is caught on the next render tick.
    volatile bool active_ = false;
    volatile uint8_t pendingProgress_ = 0;
    volatile bool changed_ = false;
};
