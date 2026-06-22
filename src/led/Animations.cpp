#include "led/Animations.h"

#include <stddef.h>

// Static animation descriptions ((A) = audio reactive). Indexed by AnimationType.
static const char* const kAnimationDescriptions[] = {
    // Non-audio reactive
    "Rainbow",
    "Cylon",
    "RGB Chaser",
    "Beat Sine",
    "Plasma",
    "Sparkle",
    "Wave",
    "Comet",
    // Audio reactive
    "Ice Waves (A)",
    "Purple Rain (A)",
    "Fire (A)",
    "Matrix (A)",
    "VU (A)",
    "Ripple (A)",
    "Confetti (A)",
};

static_assert(sizeof(kAnimationDescriptions) / sizeof(kAnimationDescriptions[0]) == ANIMATION_COUNT,
              "kAnimationDescriptions must have one entry per AnimationType");

const char* animationDescription(AnimationType animation) {
    const size_t index = static_cast<size_t>(animation);
    if (index < ANIMATION_COUNT) {
        return kAnimationDescriptions[index];
    }
    return "Unknown";
}

const char* const* animationDescriptions() {
    return kAnimationDescriptions;
}
