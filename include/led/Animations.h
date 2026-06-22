#pragma once

#include <stdint.h>

/**
 * @brief LED animation vocabulary — the single shared enum + labels.
 *
 * Lives in its own header (not inside LEDManager or AnimationEngine) so all of
 * AnimationEngine (renders them), LEDManager (selects/persists the current one),
 * and the UI/web layers (list + label them) can depend on it without a circular
 * include between LEDManager and AnimationEngine.
 *
 * Plain (unscoped) enum on purpose: the value doubles as an array index and is
 * cast to/from `int` when crossing the WebSocket / NVS boundary. ANIMATION_COUNT
 * is the sentinel terminator — keep it last.
 */
enum AnimationType : uint8_t {
    // Non-audio reactive
    RAINBOW = 0,
    CYLON,
    RGBCHASER,
    BEATSINE,
    PLASMA,
    SPARKLE,
    WAVE,
    COMET,
    // Audio reactive
    ICEWAVES,
    PURPLERAIN,
    FIRE,
    MATRIX,
    VU,
    RIPPLE,
    CONFETTI,
    ANIMATION_COUNT  ///< sentinel — number of animations; keep last
};

/// Human-readable description for one animation ("(A)" suffix = audio reactive).
/// Returns "Unknown" if @p animation is out of range.
const char* animationDescription(AnimationType animation);

/// The full description table (ANIMATION_COUNT entries, indexed by AnimationType).
const char* const* animationDescriptions();
