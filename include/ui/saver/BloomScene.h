#pragma once

#include "ui/saver/IScene.h"

/**
 * @brief The angular bloom: angle picks the band, radius picks the falloff.
 *
 * A seven-lobed shape that reaches further where its band is loud, rotating
 * slowly. This is the one the mockup showed, and the one the concentric-ring
 * version was standing in for.
 *
 * The mockup put its cost at ~150 KB of lookup table, and a quadrant-symmetry
 * version would still have been ~38 KB. Neither is needed. The angle comes from
 * two small tables instead:
 *
 *  - a reciprocal table, so the tangent ratio is a multiply rather than a divide
 *    (a divide per pixel would be ~20 cycles, which is most of the frame budget);
 *  - a 257-entry arctangent table that turns that ratio into a true angle, so the
 *    seven lobes come out evenly spaced rather than bunched at the diagonals.
 *
 * Together that is under 4 KB, and per pixel it is a compare, a multiply, a
 * shift, two lookups and some sign fixups.
 */
class BloomScene final : public IScene {
public:
    const char* name() const override { return "Bloom"; }

    bool start(int32_t width, int32_t height) override;
    void advance(const AudioFrame& frame) override;
    void renderStrip(uint16_t* dst, int32_t y, int32_t height) override;

    /// 20 fps. Every pixel of this scene moves each frame (the lobes rotate
    /// and breathe), so a partially-written frame is visible as a tear. At a
    /// lower rate the screen is coherent between frames instead of being swept
    /// continuously, which is what actually makes the tear go away.
    uint32_t frameIntervalMs() const override { return 50; }

private:
    static constexpr int32_t R2_SHIFT = 6;
    static constexpr int32_t ROOT_ENTRIES = 1400;   // covers a 320x480 diagonal
    static constexpr int32_t RECIP_ENTRIES = 512;   // >= the longer half-axis
    /// Full circle in table units. A power of two so the wrap is a mask.
    static constexpr int32_t TURN = 1024;
    /// Thickness of the lit shell, as a percentage of the short half-axis.
    static constexpr int32_t SHELL_PCT = 30;
    /// Where a silent band's lobe sits, as a percentage of the short half-axis.
    static constexpr int32_t REACH_MIN_PCT = 18;
    static constexpr int32_t REACH_SPAN_PCT = 78;

    uint16_t rootTab_[ROOT_ENTRIES] = { 0 };   ///< r^2 >> R2_SHIFT  → r in pixels
    uint32_t recip_[RECIP_ENTRIES] = { 0 };    ///< 65536 / d
    uint8_t  atanTab_[257] = { 0 };            ///< tan ratio (0..256) → angle (0..128)

    /// Per-band reach in pixels and brightness, refreshed each frame.
    int16_t reach_[AUDIO_BAND_COUNT] = { 0 };
    uint8_t level_[AUDIO_BAND_COUNT] = { 0 };
    /// Smoothed levels, 8.8. Fast attack so transients still land, slow release
    /// so the shape doesn't lurch between frames — less frame-to-frame change is
    /// the other half of hiding the tear, and it looks better anyway.
    int32_t smooth_[AUDIO_BAND_COUNT] = { 0 };

    uint16_t palette_[256] = { 0 };

    int32_t width_ = 0;
    int32_t height_ = 0;
    int32_t cx_ = 0;
    int32_t cy_ = 0;
    int32_t invShell_ = 0;   ///< 255/shell thickness, 16.16
    uint16_t spin_ = 0;
};
