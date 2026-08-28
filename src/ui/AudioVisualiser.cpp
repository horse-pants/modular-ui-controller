#include "ui/AudioVisualiser.h"
#include "modular-ui.h"
#include <Logger.h>

// Single shared instance — used by the static dismiss (tap) callback.
AudioVisualiser* g_audioVisualiser = nullptr;

AudioVisualiser::AudioVisualiser()
    : overlay_(nullptr)
    , vuGraph_(nullptr)
    , initialized_(false)
    , screensaverActive_(false)
{
}

AudioVisualiser::~AudioVisualiser() {
    cleanup();
}

bool AudioVisualiser::initialize(lv_obj_t* topLayer) {
    if (initialized_) {
        return true;
    }
    if (!topLayer) {
        return false;
    }

    overlay_ = lv_obj_create(topLayer);
    if (!overlay_) {
        return false;
    }
    lv_obj_set_size(overlay_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(overlay_, 0, 0);
    lv_obj_set_style_bg_color(overlay_, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(overlay_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_opa(overlay_, LV_OPA_0, 0);
    lv_obj_set_style_pad_all(overlay_, 0, 0);
    lv_obj_set_style_radius(overlay_, 0, 0);
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);      // shown only when idle
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);   // tap to dismiss
    lv_obj_add_event_cb(overlay_, pressEvent, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(overlay_, releaseEvent, LV_EVENT_RELEASED, nullptr);

    vuGraph_.reset(new VuGraph());
    if (!vuGraph_ || !vuGraph_->initialize(overlay_)) {
        cleanup();
        return false;
    }
    // The overlay is full-screen; centre the VU block in it (the VuGraph's own
    // layout is tuned for the smaller VU tab and would otherwise sit top-left).
    vuGraph_->centerContentIn(LV_HOR_RES, LV_VER_RES);

    scenePlayer_.reset(new ScenePlayer());
    if (scenePlayer_ && !scenePlayer_->begin()) {
        // No scenes available — fall back to the VU meter rather than a blank
        // screensaver.
        scenePlayer_.reset();
        mode_ = Mode::VuMeter;
    }
    applyMode();

    g_audioVisualiser = this;
    initialized_ = true;
    return true;
}

void AudioVisualiser::applyMode() {
    // In Scene mode the VU objects would be drawn by LVGL onto the overlay and
    // then immediately painted over, so hide them rather than pay for them.
    if (vuGraph_ && vuGraph_->getLvglObject()) {
        if (mode_ == Mode::VuMeter) {
            lv_obj_remove_flag(vuGraph_->getLvglObject(), LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(vuGraph_->getLvglObject(), LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void AudioVisualiser::setMode(Mode mode) {
    if (mode_ == mode) return;
    if (mode == Mode::Scene && !scenePlayer_) return;   // nothing to switch to

    // Stop whatever is running before the visuals swap, so a scene can't push a
    // frame over the VU meter after the handover.
    if (screensaverActive_ && scenePlayer_) scenePlayer_->stop();
    mode_ = mode;
    applyMode();
    if (screensaverActive_ && mode_ == Mode::Scene && scenePlayer_) scenePlayer_->start();
}

int AudioVisualiser::selectionCount() const {
    return 1 + (scenePlayer_ ? scenePlayer_->count() : 0);   // slot 0 is the VU meter
}

const char* AudioVisualiser::selectionName(int index) const {
    if (index <= 0) return "VU Meter";
    return scenePlayer_ ? scenePlayer_->nameAt(index - 1) : "none";
}

int AudioVisualiser::selection() const {
    if (mode_ == Mode::VuMeter || !scenePlayer_) return 0;
    return scenePlayer_->current() + 1;
}

void AudioVisualiser::setSelection(int index) {
    if (index <= 0) {
        setMode(Mode::VuMeter);
        return;
    }
    if (!scenePlayer_) return;

    // Point the player at the scene BEFORE switching mode, so entering Scene
    // mode starts the one that was asked for rather than the previous one.
    scenePlayer_->setCurrent(index - 1);
    setMode(Mode::Scene);
}

void AudioVisualiser::nextScene() {
    // Wrap through the VU meter as well, so a swipe reaches every option.
    const int n = selectionCount();
    if (n <= 1) return;
    setSelection((selection() + 1) % n);
}

void AudioVisualiser::show() {
    if (!overlay_) {
        return;
    }
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_);
    screensaverActive_ = true;

    if (mode_ == Mode::Scene && scenePlayer_ && !scenePlayer_->start()) {
        // Out of internal RAM for the strip buffer — degrade to the VU meter
        // rather than showing nothing.
        Logger.warning("Screensaver: scene unavailable, falling back to VU meter");
        mode_ = Mode::VuMeter;
        applyMode();
    }
}

void AudioVisualiser::hide() {
    // Stop the scene FIRST: it invalidates the screen so LVGL repaints the real
    // UI over everything the direct pushes drew.
    if (scenePlayer_) {
        scenePlayer_->stop();
    }
    if (overlay_) {
        lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    }
    screensaverActive_ = false;
}

void AudioVisualiser::update() {
    if (!screensaverActive_) {
        return;
    }
    if (mode_ == Mode::Scene) {
        if (scenePlayer_) scenePlayer_->update();
    } else if (vuGraph_) {
        vuGraph_->update();
    }
}

void AudioVisualiser::cleanup() {
    if (g_audioVisualiser == this) {
        g_audioVisualiser = nullptr;
    }
    // Hand the panel back before anything is torn down.
    scenePlayer_.reset();
    // Destroy the VuGraph (deletes its own lv objects) BEFORE the overlay, so
    // the overlay deletion can't double-free the VuGraph's child objects.
    vuGraph_.reset();
    if (overlay_) {
        lv_obj_del(overlay_);
        overlay_ = nullptr;
    }
    initialized_ = false;
}

void AudioVisualiser::pressEvent(lv_event_t* e) {
    (void)e;
    if (!g_audioVisualiser) return;
    lv_indev_t* indev = lv_indev_active();
    if (indev) lv_indev_get_point(indev, &g_audioVisualiser->pressPoint_);
}

void AudioVisualiser::releaseEvent(lv_event_t* e) {
    (void)e;
    if (!g_audioVisualiser) return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;

    lv_point_t up;
    lv_indev_get_point(indev, &up);
    const int32_t dx = up.x - g_audioVisualiser->pressPoint_.x;
    const int32_t dy = up.y - g_audioVisualiser->pressPoint_.y;
    const int32_t adx = dx < 0 ? -dx : dx;
    const int32_t ady = dy < 0 ? -dy : dy;

    // Mostly sideways and far enough: change scene, stay in the screensaver.
    if (adx >= SWIPE_MIN_PX && adx > ady) {
        g_audioVisualiser->nextScene();
        return;
    }

    // Anything that didn't travel: a tap, so dismiss. A vertical drag or an
    // ambiguous smear falls through and does nothing, which is better than
    // dropping out of the screensaver by accident.
    if (adx <= TAP_MAX_PX && ady <= TAP_MAX_PX) {
        g_audioVisualiser->hide();
    }
}
