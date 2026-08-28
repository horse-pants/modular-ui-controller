#include "ui/saver/WaterfallScene.h"

#include "ui/saver/Palette.h"

#include <string.h>


bool WaterfallScene::start(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0 || width > MAX_WIDTH) return false;

    width_ = width;
    height_ = height;
    head_ = 0;
    memset(hist_, 0, sizeof(hist_));

    // --- Column → band mapping. Smoothstep is folded into the stored weight so
    //     the per-pixel path is one multiply and one shift.
    for (int32_t x = 0; x < width_; ++x) {
        const int32_t p = (x * (AUDIO_BAND_COUNT - 1) * 256) / (width_ - 1);
        int32_t band = p >> 8;
        int32_t frac = p & 0xFF;
        if (band >= AUDIO_BAND_COUNT - 1) {   // the last column lands exactly on the end
            band = AUDIO_BAND_COUNT - 2;
            frac = 255;
        }
        // smoothstep f*f*(3-2f), with f = frac/256, scaled back to 0..255.
        // frac^2 * (768 - 2*frac) peaks near 2^24, so the shift is 16 — at 17 the
        // weight capped at 127, the blend only ever travelled HALF way to the next
        // band, and every band boundary showed a hard step of half the delta.
        const int32_t s = (frac * frac * (768 - 2 * frac)) >> 16;
        xBand_[x] = (uint8_t)band;
        xWeight_[x] = (uint8_t)(s > 255 ? 255 : s);
    }

    SaverPalette::buildDefault(palette_);

    return true;
}

void WaterfallScene::advance(const AudioFrame& frame) {
    // The whole "scroll": one index. Nothing is copied.
    head_ = (head_ + 1) % HIST_ROWS;
    uint8_t* row = &hist_[head_ * AUDIO_BAND_COUNT];
    for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
        const int v = frame.bands[i];
        row[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
}

void WaterfallScene::renderStrip(uint16_t* dst, int32_t y0, int32_t height) {
    if (!dst || width_ <= 0) return;

    const size_t rowBytes = (size_t)width_ * sizeof(uint16_t);
    int32_t lastAge = -1;
    uint16_t* prevRow = nullptr;

    for (int32_t r = 0; r < height; ++r) {
        uint16_t* out = dst + (size_t)r * width_;
        const int32_t y = y0 + r;

        // Newest at the bottom, history rising. Two screen rows share a history
        // row at 480/240, so the second is a straight copy of the first.
        const int32_t age = ((height_ - 1 - y) * HIST_ROWS) / height_;
        if (age == lastAge && prevRow) {
            memcpy(out, prevRow, rowBytes);
            continue;
        }
        lastAge = age;
        prevRow = out;

        const int32_t idx = ((head_ - age) % HIST_ROWS + HIST_ROWS) % HIST_ROWS;
        const uint8_t* bands = &hist_[idx * AUDIO_BAND_COUNT];

        for (int32_t x = 0; x < width_; ++x) {
            const uint8_t band = xBand_[x];
            const int32_t a = bands[band];
            const int32_t b = bands[band + 1];
            int32_t v = a + (((b - a) * (int32_t)xWeight_[x]) >> 8);
            // Squared response: keeps room tone dark instead of a permanent haze.
            v = (v * v) >> 8;
            v += SaverPalette::ditherAt(x, y);
            out[x] = palette_[SaverPalette::clampLevel(v)];
        }
    }
}
