#include "ui/saver/BloomScene.h"

#include "ui/saver/Palette.h"

#include <math.h>
#include <stddef.h>

bool BloomScene::start(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return false;

    width_ = width;
    height_ = height;
    cx_ = width_ / 2;
    cy_ = height_ / 2;
    spin_ = 0;

    const int32_t maxR = (cx_ < cy_ ? cx_ : cy_);
    const int32_t shell = (maxR * SHELL_PCT) / 100;
    invShell_ = (255 << 16) / (shell > 0 ? shell : 1);

    for (int32_t i = 0; i < ROOT_ENTRIES; ++i) {
        rootTab_[i] = (uint16_t)(sqrtf((float)(i << R2_SHIFT)) + 0.5f);
    }

    // 65536/d, so the tangent ratio is a multiply-shift. recip_[1] would be
    // 65536 exactly; it is kept in a uint32 rather than clamped into a uint16.
    recip_[0] = 0;
    for (int32_t d = 1; d < RECIP_ENTRIES; ++d) {
        recip_[d] = (uint32_t)(65536 / d);
    }

    // Ratio (0..256, i.e. tan 0..1) -> angle (0..128, i.e. 0..45 degrees on a
    // 256-per-quadrant scale). Without this the lobes would be spaced by tangent
    // rather than by angle, bunching them towards the diagonals.
    for (int32_t t = 0; t <= 256; ++t) {
        const float a = atanf((float)t / 256.0f);          // 0 .. pi/4
        atanTab_[t] = (uint8_t)((a * 512.0f) / (float)M_PI + 0.5f);
    }

    SaverPalette::buildDefault(palette_);

    for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
        reach_[i] = (int16_t)((maxR * REACH_MIN_PCT) / 100);
        level_[i] = 0;
        smooth_[i] = 0;
    }
    return true;
}

void BloomScene::advance(const AudioFrame& frame) {
    spin_ = (uint16_t)((spin_ + 1) & (TURN - 1));

    const int32_t maxR = (cx_ < cy_ ? cx_ : cy_);
    const int32_t base = (maxR * REACH_MIN_PCT) / 100;
    const int32_t span = (maxR * REACH_SPAN_PCT) / 100;

    for (int i = 0; i < AUDIO_BAND_COUNT; ++i) {
        int32_t v = frame.bands[i];
        if (v < 0) v = 0;
        if (v > 255) v = 255;

        const int32_t target = v << 8;
        const int32_t k = (target > smooth_[i]) ? 140 : 40;   // /256: attack, release
        smooth_[i] += ((target - smooth_[i]) * k) >> 8;

        v = smooth_[i] >> 8;
        level_[i] = (uint8_t)v;
        reach_[i] = (int16_t)(base + (v * span) / 255);
    }
}

void BloomScene::renderStrip(uint16_t* dst, int32_t y0, int32_t height) {
    if (!dst || width_ <= 0) return;

    for (int32_t r = 0; r < height; ++r) {
        uint16_t* out = dst + (size_t)r * width_;
        const int32_t y = y0 + r;
        const int32_t dy = y - cy_;
        const int32_t ady = dy < 0 ? -dy : dy;
        const int32_t dy2 = dy * dy;

        for (int32_t x = 0; x < width_; ++x) {
            const int32_t dx = x - cx_;
            const int32_t adx = dx < 0 ? -dx : dx;

            // --- Angle, without a divide and without atan2 ---
            int32_t q;
            if (adx >= ady) {
                q = adx ? atanTab_[(ady * recip_[adx]) >> 8] : 0;
            } else {
                q = 256 - atanTab_[(adx * recip_[ady]) >> 8];
            }
            // Quadrant, measured from +x towards +y (screen y points down).
            int32_t ang;
            if (dx >= 0) ang = (dy >= 0) ? q : (TURN - q);
            else         ang = (dy >= 0) ? (512 - q) : (512 + q);
            ang = (ang + spin_) & (TURN - 1);

            // --- Which band owns this angle, and how far to the next ---
            const int32_t p = (ang * AUDIO_BAND_COUNT) >> 2;   // 0 .. 1790
            int32_t band = p >> 8;
            if (band >= AUDIO_BAND_COUNT) band = AUDIO_BAND_COUNT - 1;
            const int32_t frac = p & 0xFF;
            int32_t w = (frac * frac * (768 - 2 * frac)) >> 16;
            if (w > 255) w = 255;

            // >>8 rather than /255: two divides per pixel is ~40 cycles, which is
            // the entire per-pixel budget. The 1/256-vs-1/255 bias is invisible.
            const int32_t nxt = (band + 1 == AUDIO_BAND_COUNT) ? 0 : band + 1;
            const int32_t reach = reach_[band] + ((((int32_t)reach_[nxt] - reach_[band]) * w) >> 8);
            const int32_t lit = level_[band] + ((((int32_t)level_[nxt] - level_[band]) * w) >> 8);

            // --- Distance to that lobe's shell ---
            int32_t idx = (dx * dx + dy2) >> R2_SHIFT;
            if (idx >= ROOT_ENTRIES) idx = ROOT_ENTRIES - 1;
            int32_t d = (int32_t)rootTab_[idx] - reach;
            if (d < 0) d = -d;

            int32_t v = 255 - ((d * invShell_) >> 16);
            if (v <= 0) {
                out[x] = palette_[0];
                continue;
            }
            // Quiet bands still show their lobe, faintly, so the shape reads as
            // one object breathing rather than seven strobing wedges.
            v = (v * (90 + ((lit * 165) >> 8))) >> 8;
            v = (v * v) >> 8;
            v += SaverPalette::ditherAt(x, y);
            out[x] = palette_[SaverPalette::clampLevel(v)];
        }
    }
}
