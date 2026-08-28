#include "ui/saver/ScenePlayer.h"

#include "audio/AudioBus.h"
#include "ui/Display.h"
#include "ui/saver/AuroraScene.h"
#include "ui/saver/BloomScene.h"
#include "ui/saver/RingsScene.h"
#include "ui/saver/WaterfallScene.h"

#include <Arduino.h>
#include <Logger.h>
#include <stddef.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

ScenePlayer::~ScenePlayer() {
    stop();
}

bool ScenePlayer::begin() {
    // Scene registry. Adding one is a line here plus the class. Order is the
    // order next() cycles through.
    scenes_[sceneCount_++].reset(new WaterfallScene());
    scenes_[sceneCount_++].reset(new AuroraScene());
    scenes_[sceneCount_++].reset(new BloomScene());
    // Rings stays as the cheap fallback: it needs no angle at all, so if Bloom
    // costs too much on the bus it is the one to fall back to.
    scenes_[sceneCount_++].reset(new RingsScene());
    return sceneCount_ > 0;
}

bool ScenePlayer::start() {
    if (running_) return true;
    if (sceneCount_ == 0) return false;

    width_ = lv_display_get_horizontal_resolution(nullptr);
    height_ = lv_display_get_vertical_resolution(nullptr);
    if (width_ <= 0 || height_ <= 0) return false;

    if (!strip_) {
        // Internal RAM on purpose: this is written per pixel and read straight
        // out to the bus, which is exactly the traffic PSRAM is bad at.
        const size_t bytes = (size_t)width_ * STRIP_ROWS * sizeof(uint16_t);
        strip_ = (uint16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL);
        if (!strip_) {
            Logger.warning("ScenePlayer: %u byte strip buffer alloc failed",
                           (unsigned)bytes);
            return false;
        }
    }

    startCurrentScene();
    lastFrameMs_ = millis();
    running_ = true;
    return true;
}

void ScenePlayer::startCurrentScene() {
    IScene* scene = scenes_[current_].get();
    if (!scene) return;
    if (!scene->start(width_, height_)) {
        Logger.warning("ScenePlayer: scene '%s' refused to start", scene->name());
    }
}

void ScenePlayer::stop() {
    const bool wasRunning = running_;
    if (wasRunning && scenes_[current_]) {
        scenes_[current_]->stop();
    }
    running_ = false;

    if (strip_) {
        heap_caps_free(strip_);
        strip_ = nullptr;
    }

    if (!wasRunning) return;   // nothing was painted over; also the teardown path

    // LVGL has no idea the panel was painted over, so nothing would repaint on
    // its own. Invalidate both layers to hand the screen back intact.
    if (lv_screen_active()) lv_obj_invalidate(lv_screen_active());
    if (lv_layer_top())     lv_obj_invalidate(lv_layer_top());
}

void ScenePlayer::update() {
    if (!running_ || !strip_) return;

    IScene* scene = scenes_[current_].get();
    if (!scene) return;

    const uint32_t interval = scene->frameIntervalMs();
    const uint32_t now = millis();
    if ((uint32_t)(now - lastFrameMs_) < (interval ? interval : FRAME_MS)) return;
    lastFrameMs_ = now;

    // Sampled once for the whole frame: per-strip sampling would tear the
    // picture into bands from different moments.
    const AudioFrame frame = g_audioBus.latest();
    scene->advance(frame);

    Display::beginDirect();
    for (int32_t y = 0; y < height_; y += STRIP_ROWS) {
        const int32_t rows = (y + STRIP_ROWS <= height_) ? STRIP_ROWS : (height_ - y);
        scene->renderStrip(strip_, y, rows);
        Display::pushStrip(y, rows, strip_);
    }
    Display::endDirect();
}

void ScenePlayer::next() {
    if (sceneCount_ <= 1) return;
    if (running_ && scenes_[current_]) scenes_[current_]->stop();
    current_ = (current_ + 1) % sceneCount_;
    if (running_) startCurrentScene();
    Logger.info("Screensaver scene: %s", currentName());
}

const char* ScenePlayer::nameAt(int index) const {
    if (index < 0 || index >= sceneCount_ || !scenes_[index]) return "none";
    return scenes_[index]->name();
}

void ScenePlayer::setCurrent(int index) {
    if (index < 0 || index >= sceneCount_ || index == current_) return;
    if (running_ && scenes_[current_]) scenes_[current_]->stop();
    current_ = index;
    if (running_) startCurrentScene();
    Logger.info("Screensaver scene: %s", currentName());
}

const char* ScenePlayer::currentName() const {
    const IScene* scene = scenes_[current_].get();
    return scene ? scene->name() : "none";
}
