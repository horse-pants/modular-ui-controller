#pragma once

#include "ui/saver/IScene.h"

/**
 * @brief Scrolling spectrogram: the last few seconds of the seven bands.
 *
 * The newest row is at the bottom and history rises up the screen. Each row is
 * the seven band levels smoothed across the full width, colour-mapped through a
 * palette that runs deep blue → cyan → magenta → white.
 *
 * The reason this suits an MSGEQ7 specifically: **scrolling costs nothing**. The
 * history is a ring buffer of 240 rows x 7 bytes — 1.7 KB — and a frame advances
 * it by moving one index. No pixels are ever copied, no canvas is shifted. Seven
 * bands is a limitation for most effects; here it is what keeps the state small
 * enough to be free.
 *
 * It also keeps moving when the music doesn't, because it shows the last eight
 * seconds rather than the current instant.
 *
 * Rows are computed once and reused: 480 screen rows over 240 history rows means
 * each history row covers two screen rows, so half the pixel work is a memcpy.
 */
class WaterfallScene final : public IScene {
public:
    const char* name() const override { return "Waterfall"; }

    bool start(int32_t width, int32_t height) override;
    void advance(const AudioFrame& frame) override;
    void renderStrip(uint16_t* dst, int32_t y, int32_t height) override;

private:
    /// Seconds of history = HIST_ROWS / frame rate. 240 @ 30 fps = 8 s.
    static constexpr int32_t HIST_ROWS = 240;
    static constexpr int32_t MAX_WIDTH = 320;

    uint8_t  hist_[HIST_ROWS * AUDIO_BAND_COUNT] = { 0 };
    int32_t  head_ = 0;

    // Per-column band interpolation, precomputed so the inner loop is a plain
    // lerp: which band is to the left, and how far between it and the next.
    // The smoothstep curve is baked into the weight at table-build time.
    uint8_t  xBand_[MAX_WIDTH] = { 0 };
    uint8_t  xWeight_[MAX_WIDTH] = { 0 };

    /// level (after gamma) → RGB565, native byte order.
    uint16_t palette_[256] = { 0 };

    int32_t width_ = 0;
    int32_t height_ = 0;
};
