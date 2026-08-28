#include "ui/saver/AuroraScene.h"

#include "ui/saver/Palette.h"

#include <stddef.h>

bool AuroraScene::start(int32_t width, int32_t height) {
    if (width <= 1 || height <= 0 || width > MAX_WIDTH) return false;

    width_ = width;
    height_ = height;

    const int32_t belowSpan = (height_ * BELOW_PCT) / 100;
    invBelow_ = (255 << 16) / (belowSpan > 0 ? belowSpan : 1);
    invAbove_ = (255 << 16) / ABOVE_SPAN;

    SaverPalette::buildDefault(palette_);

    for (int32_t x = 0; x < width_; ++x) crest_[x] = (int16_t)height_;
    return true;
}

void AuroraScene::advance(const AudioFrame& frame) {
    // The spectrum is FIXED to the screen: bass on the left, treble on the
    // right, always. This drifted sideways at first, to keep the shape moving
    // during steady music. Two problems, in order of discovery: wrapping put
    // band 6 next to band 0 and that seam travelled across as a hard line; and
    // once mirrored to remove the seam, the bass slid between the left and right
    // edges, which is worse — you cannot read a spectrum whose axis moves.
    // The crest still animates vertically with the music, which is enough.
    for (int32_t x = 0; x < width_; ++x) {
        const int32_t sx = x;

        // Smooth the seven levels across the width, same smoothstep the
        // waterfall uses. Done per column per frame (320 iterations), not per
        // pixel, so the cost is irrelevant.
        const int32_t p = (sx * (AUDIO_BAND_COUNT - 1) * 256) / (width_ - 1);
        int32_t band = p >> 8;
        int32_t frac = p & 0xFF;
        if (band >= AUDIO_BAND_COUNT - 1) {
            band = AUDIO_BAND_COUNT - 2;
            frac = 255;
        }
        int32_t w = (frac * frac * (768 - 2 * frac)) >> 16;
        if (w > 255) w = 255;

        const int32_t a = frame.bands[band];
        const int32_t b = frame.bands[band + 1];
        int32_t level = a + (((b - a) * w) >> 8);
        if (level < 0) level = 0;
        if (level > 255) level = 255;

        // Rest the crest 30% up the screen, rising to 82% when the band is full.
        const int32_t lift = (height_ * 30) / 100 + (level * ((height_ * 52) / 100)) / 255;
        crest_[x] = (int16_t)(height_ - lift);
    }
}

void AuroraScene::renderStrip(uint16_t* dst, int32_t y0, int32_t height) {
    if (!dst || width_ <= 0) return;

    for (int32_t r = 0; r < height; ++r) {
        uint16_t* out = dst + (size_t)r * width_;
        const int32_t y = y0 + r;

        for (int32_t x = 0; x < width_; ++x) {
            const int32_t dy = y - crest_[x];

            int32_t v;
            if (dy >= 0) {
                v = 255 - ((dy * invBelow_) >> 16);       // the long glow below
            } else {
                v = 255 - (((-dy) * invAbove_) >> 16);    // the sharp edge above
            }
            if (v <= 0) {
                out[x] = palette_[0];
                continue;
            }

            v = (v * v) >> 8;                              // squared response
            v += SaverPalette::ditherAt(x, y);             // break RGB565 banding
            out[x] = palette_[SaverPalette::clampLevel(v)];
        }
    }
}
