#pragma once

#include "ui/saver/IScene.h"

/**
 * @brief Seven concentric rings, one per band, breathing out from the centre.
 *
 * The mockup's version of this picked the band by ANGLE, which needs an
 * atan2/distance pair per pixel or a ~150 KB lookup table to avoid one. Rings
 * need only distance, and distance has a cheap incremental form — so this costs
 * 1.3 KB instead, and arguably suits seven bands better: bass is the inner ring,
 * treble the outer, and the whole thing reads as a target that pulses outward.
 *
 * Per pixel: one multiply for dx*dx, a shift into a square-root table, then a
 * ring lookup. No trig, no divides, no per-pixel square root.
 */
class RingsScene final : public IScene {
public:
    const char* name() const override { return "Rings"; }

    bool start(int32_t width, int32_t height) override;
    void advance(const AudioFrame& frame) override;
    void renderStrip(uint16_t* dst, int32_t y, int32_t height) override;

private:
    /// r^2 is shifted right by this much to index the root table, trading a
    /// little precision for a table that fits in a couple of KB.
    static constexpr int32_t R2_SHIFT = 6;
    static constexpr int32_t ROOT_ENTRIES = 1400;   // covers a 320x480 diagonal
    static constexpr int32_t MAX_WIDTH = 320;

    /// sqrt(index << R2_SHIFT) in PIXELS. uint16 rather than a byte on purpose:
    /// the corner of a 320x480 screen is 288 px from the centre, and squeezing
    /// that into a byte by storing r>>1 is what made every radius here wrong by
    /// 4x, putting four of the seven rings off-screen.
    uint16_t rootTab_[ROOT_ENTRIES] = { 0 };

    /// Radius (pixels) and brightness of each band's ring this frame.
    int16_t ringR_[AUDIO_BAND_COUNT] = { 0 };
    uint8_t ringLevel_[AUDIO_BAND_COUNT] = { 0 };

    uint16_t palette_[256] = { 0 };

    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t cx_ = 0;
    int32_t cy_ = 0;
    int32_t ringGap_ = 0;    ///< spacing between rings, pixels
    int32_t ringHalf_ = 0;   ///< half-thickness of a ring, pixels
    /// Brightness a ring keeps at silence, so the target reads as a structure
    /// that pulses rather than shapes appearing out of nowhere.
    static constexpr int32_t RING_FLOOR = 30;
    int32_t invRingHalf_ = 0;///< 255/ringHalf_ in 16.16, to keep divides out of the pixel loop
};
