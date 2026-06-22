#include "led/AnimationEngine.h"

// One concrete animation per include — registered into the table below.
#include "led/animations/RainbowAnimation.h"
#include "led/animations/CylonAnimation.h"
#include "led/animations/RgbChaserAnimation.h"
#include "led/animations/BeatSineAnimation.h"
#include "led/animations/PlasmaAnimation.h"
#include "led/animations/SparkleAnimation.h"
#include "led/animations/WaveAnimation.h"
#include "led/animations/CometAnimation.h"
#include "led/animations/IceWavesAnimation.h"
#include "led/animations/PurpleRainAnimation.h"
#include "led/animations/FireAnimation.h"
#include "led/animations/MatrixAnimation.h"
#include "led/animations/VuAnimation.h"
#include "led/animations/RippleAnimation.h"
#include "led/animations/ConfettiAnimation.h"

AnimationEngine::AnimationEngine() {
    // Strategy registry: one instance per AnimationType (indexed by the enum).
    animations_[RAINBOW]    = std::make_unique<RainbowAnimation>();
    animations_[CYLON]      = std::make_unique<CylonAnimation>();
    animations_[RGBCHASER]  = std::make_unique<RgbChaserAnimation>();
    animations_[BEATSINE]   = std::make_unique<BeatSineAnimation>();
    animations_[PLASMA]     = std::make_unique<PlasmaAnimation>();
    animations_[SPARKLE]    = std::make_unique<SparkleAnimation>();
    animations_[WAVE]       = std::make_unique<WaveAnimation>();
    animations_[COMET]      = std::make_unique<CometAnimation>();
    animations_[ICEWAVES]   = std::make_unique<IceWavesAnimation>();
    animations_[PURPLERAIN] = std::make_unique<PurpleRainAnimation>();
    animations_[FIRE]       = std::make_unique<FireAnimation>();
    animations_[MATRIX]     = std::make_unique<MatrixAnimation>();
    animations_[VU]         = std::make_unique<VuAnimation>();
    animations_[RIPPLE]     = std::make_unique<RippleAnimation>();
    animations_[CONFETTI]   = std::make_unique<ConfettiAnimation>();
}

void AnimationEngine::setStrips(int numStrips, int ledsPerStrip) {
    ctx_.numStrips = numStrips;
    ctx_.ledsPerStrip = ledsPerStrip;
}

void AnimationEngine::setBuffer(CRGB* leds, int totalLeds) {
    ctx_.leds = leds;
    ctx_.totalLeds = totalLeds;
}

void AnimationEngine::setAudio(const AudioFrame& frame) {
    for (int i = 0; i < AUDIO_BAND_COUNT; i++) {
        ctx_.vuLevels[i] = frame.bands[i];
    }
    ctx_.audioLevel = frame.overall;
}

IAnimation* AnimationEngine::lookup(AnimationType type) const {
    if (type >= ANIMATION_COUNT) {
        return nullptr;
    }
    return animations_[type].get();
}

void AnimationEngine::render(AnimationType type) {
    if (!ctx_.leds || ctx_.totalLeds <= 0) {
        return;
    }
    if (IAnimation* anim = lookup(type)) {
        anim->render(ctx_);
    }
}

int AnimationEngine::frameIntervalMs(AnimationType type) const {
    IAnimation* anim = lookup(type);
    return anim ? anim->frameIntervalMs() : 100;
}
