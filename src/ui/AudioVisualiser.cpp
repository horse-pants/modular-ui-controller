#include "AudioVisualiser.h"
#include "modular-ui.h"

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
    lv_obj_add_event_cb(overlay_, dismissEvent, LV_EVENT_CLICKED, nullptr);

    vuGraph_.reset(new VuGraph());
    if (!vuGraph_ || !vuGraph_->initialize(overlay_)) {
        cleanup();
        return false;
    }
    // The overlay is full-screen; centre the VU block in it (the VuGraph's own
    // layout is tuned for the smaller VU tab and would otherwise sit top-left).
    vuGraph_->centerContentIn(LV_HOR_RES, LV_VER_RES);

    g_audioVisualiser = this;
    initialized_ = true;
    return true;
}

void AudioVisualiser::show() {
    if (!overlay_) {
        return;
    }
    lv_obj_clear_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(overlay_);
    screensaverActive_ = true;
}

void AudioVisualiser::hide() {
    if (overlay_) {
        lv_obj_add_flag(overlay_, LV_OBJ_FLAG_HIDDEN);
    }
    screensaverActive_ = false;
}

void AudioVisualiser::update() {
    if (!screensaverActive_ || !vuGraph_) {
        return;
    }
    vuGraph_->update();
}

void AudioVisualiser::cleanup() {
    if (g_audioVisualiser == this) {
        g_audioVisualiser = nullptr;
    }
    // Destroy the VuGraph (deletes its own lv objects) BEFORE the overlay, so
    // the overlay deletion can't double-free the VuGraph's child objects.
    vuGraph_.reset();
    if (overlay_) {
        lv_obj_del(overlay_);
        overlay_ = nullptr;
    }
    initialized_ = false;
}

void AudioVisualiser::dismissEvent(lv_event_t* e) {
    (void)e;
    if (g_audioVisualiser) {
        g_audioVisualiser->hide();
    }
}
