#pragma once

#include <memory>
#include "led/LedHelpers.h"
#include "led/Animations.h"
#include "audio/AudioFrame.h"
#include "led/animations/IAnimation.h"
#include "led/animations/RenderContext.h"

/**
 * @brief Owns the animation instances and dispatches rendering by AnimationType.
 *
 * Split out of LEDManager (which was a god object). The engine holds one
 * IAnimation per AnimationType (a strategy registry, C#-style) plus a single
 * RenderContext — the shared "canvas" (buffer + geometry + audio) it refreshes
 * each tick. It does NOT own the pixel buffer: LEDManager allocates that (it's
 * also used for solid-colour / white / OTA fills) and hands a non-owning pointer
 * in via setBuffer().
 *
 * Each animation keeps its own running state, so switching animations no longer
 * leaks state through shared function-statics.
 */
class AnimationEngine {
public:
    AnimationEngine();

    /// Set the strip layout (rows x columns).
    void setStrips(int numStrips, int ledsPerStrip);

    /// Point the engine at LEDManager's pixel buffer (non-owning).
    void setBuffer(CRGB* leds, int totalLeds);

    /// Publish the latest audio snapshot for audio-reactive animations.
    void setAudio(const AudioFrame& frame);

    /// Render one frame of @p type into the buffer.
    void render(AnimationType type);

    /// Per-animation frame interval in ms (drives the update cadence).
    int frameIntervalMs(AnimationType type) const;

private:
    IAnimation* lookup(AnimationType type) const;

    RenderContext ctx_;
    std::unique_ptr<IAnimation> animations_[ANIMATION_COUNT];
};
