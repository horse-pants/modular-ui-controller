#pragma once

#include "ui/saver/IScene.h"

/**
 * @brief A soft band of light whose upper edge is the spectrum.
 *
 * The seven levels become a smooth crest across the width; everything below it
 * falls off gently, everything above it cuts off sharply. The result reads as a
 * glowing horizon that breathes with the music — calmer than the waterfall, and
 * legible from across a room where fine detail isn't.
 *
 * The crest is computed once per frame into a 320-entry row, so the per-pixel
 * work is a subtract, a multiply-shift and a palette lookup. Cheapest of the
 * three scenes.
 *
 * The spectrum is pinned to the screen — bass left, treble right. An earlier
 * version drifted it sideways for extra motion; that made the bass wander
 * between the two edges, which reads as broken rather than alive. Don't
 * reintroduce it.
 */
class AuroraScene final : public IScene {
public:
    const char* name() const override { return "Aurora"; }

    bool start(int32_t width, int32_t height) override;
    void advance(const AudioFrame& frame) override;
    void renderStrip(uint16_t* dst, int32_t y, int32_t height) override;

private:
    static constexpr int32_t MAX_WIDTH = 320;
    /// How far the glow reaches below the crest, as a percentage of the height.
    static constexpr int32_t BELOW_PCT = 55;
    /// Sharp cut above the crest, in pixels — this is what makes it an edge.
    static constexpr int32_t ABOVE_SPAN = 26;

    /// Screen y of the crest for each column.
    int16_t crest_[MAX_WIDTH] = { 0 };

    // Reciprocals, so the per-pixel falloff is a multiply-shift not a divide.
    int32_t invBelow_ = 0;
    int32_t invAbove_ = 0;

    uint16_t palette_[256] = { 0 };
    int32_t width_ = 0;
    int32_t height_ = 0;
};
