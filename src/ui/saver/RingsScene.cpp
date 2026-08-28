#include "ui/saver/RingsScene.h"

#include "ui/saver/Palette.h"

#include <stddef.h>

#include <math.h>

bool RingsScene::start(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0 || width > MAX_WIDTH) return false;

    width_ = width;
    height_ = height;
    cx_ = width_ / 2;
    cy_ = height_ / 2;

    // Rings are spaced so the outermost sits just past the shorter half-axis;
    // the corners stay dark, which frames the thing rather than cropping it.
    const int32_t maxR = (cx_ < cy_ ? cx_ : cy_);
    ringGap_ = maxR / AUDIO_BAND_COUNT;         // pixels; outermost lands on maxR
    ringHalf_ = ringGap_ / 2;
    invRingHalf_ = (255 << 16) / (ringHalf_ > 0 ? ringHalf_ : 1);

    // sqrt table: index is r^2 >> R2_SHIFT, value is the radius in pixels.
    // One-off at start(), ~2.8 KB.
    for (int32_t i = 0; i < ROOT_ENTRIES; ++i) {
        rootTab_[i] = (uint16_t)(sqrtf((float)(i << R2_SHIFT)) + 0.5f);
    }

    SaverPalette::buildDefault(palette_);

    for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
        ringR_[i] = (int16_t)(ringGap_ * (i + 1));
        ringLevel_[i] = 0;
    }
    return true;
}

void RingsScene::advance(const AudioFrame& frame) {
    for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
        int32_t level = frame.bands[i];
        if (level < 0) level = 0;
        if (level > 255) level = 255;
        ringLevel_[i] = (uint8_t)level;

        // A loud band pushes its ring outward a little as well as brightening
        // it, so the pattern moves rather than just flickering in place.
        const int32_t base = ringGap_ * (i + 1);
        ringR_[i] = (int16_t)(base + ((level * ringGap_) >> 10));
    }
}

void RingsScene::renderStrip(uint16_t* dst, int32_t y0, int32_t height) {
    if (!dst || width_ <= 0) return;

    for (int32_t r = 0; r < height; ++r) {
        uint16_t* out = dst + (size_t)r * width_;
        const int32_t y = y0 + r;
        const int32_t dy = y - cy_;
        const int32_t dy2 = dy * dy;

        for (int32_t x = 0; x < width_; ++x) {
            const int32_t dx = x - cx_;
            int32_t idx = (dx * dx + dy2) >> R2_SHIFT;
            if (idx >= ROOT_ENTRIES) idx = ROOT_ENTRIES - 1;
            const int32_t rad = rootTab_[idx];          // half-pixels

            // Brightest ring wins, rather than summing — overlapping rings
            // otherwise wash out to a flat bright blob at high volume.
            int32_t best = 0;
            for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
                int32_t d = rad - ringR_[i];
                if (d < 0) d = -d;
                if (d >= ringHalf_) continue;
                const int32_t falloff = 255 - ((d * invRingHalf_) >> 16);
                const int32_t lit = RING_FLOOR +
                                    ((ringLevel_[i] * (255 - RING_FLOOR)) >> 8);
                const int32_t v = (falloff * lit) >> 8;
                if (v > best) best = v;
            }

            if (best <= 0) {
                out[x] = palette_[0];
                continue;
            }
            best = (best * best) >> 8;
            best += SaverPalette::ditherAt(x, y);
            out[x] = palette_[SaverPalette::clampLevel(best)];
        }
    }
}
