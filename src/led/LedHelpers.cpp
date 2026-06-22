#include "led/LedHelpers.h"
#include <esp_random.h>

// Algorithms adapted from FastLED (MIT-licensed) — see LedHelpers.h header.

// ---- CRGB ↔ CHSV ----

CRGB::CRGB(const CHSV& hsv) {
    hsv2rgb_rainbow(hsv, *this);
}

CRGB& CRGB::operator=(const CHSV& hsv) {
    hsv2rgb_rainbow(hsv, *this);
    return *this;
}

CRGB& CRGB::operator+=(const CRGB& rhs) {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    return *this;
}

CRGB& CRGB::nscale8(uint8_t scale) {
    r = ::scale8(r, scale);
    g = ::scale8(g, scale);
    b = ::scale8(b, scale);
    return *this;
}

// ---- Trig ----

static const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};

uint8_t sin8(uint8_t theta) {
    uint8_t offset = theta;
    if (theta & 0x40) {
        offset = uint8_t(255 - offset);
    }
    offset &= 0x3F;

    uint8_t secoffset = offset & 0x0F;
    if (theta & 0x40) {
        ++secoffset;
    }

    uint8_t section = offset >> 4;
    const uint8_t* p = b_m16_interleave + (section * 2);
    uint8_t b_ = *p++;
    uint8_t m16 = *p;

    uint8_t mx = (m16 * secoffset) >> 4;
    int8_t y = int8_t(mx) + int8_t(b_);
    if (theta & 0x80) {
        y = -y;
    }
    return uint8_t(y + 128);
}

int16_t sin16(uint16_t theta) {
    static const uint16_t base[]  = {0, 6393, 12539, 18204, 23170, 27245, 30273, 32137};
    static const uint8_t  slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

    uint16_t offset = (theta & 0x3FFF) >> 3;
    if (theta & 0x4000) {
        offset = 2047 - offset;
    }

    uint8_t section = offset / 256;
    uint16_t b_ = base[section];
    uint8_t  m  = slope[section];

    uint8_t secoffset8 = uint8_t(offset) / 2;
    uint16_t mx = m * secoffset8;
    int16_t y = int16_t(mx + b_);
    if (theta & 0x8000) {
        y = -y;
    }
    return y;
}

uint8_t sqrt16(uint16_t x) {
    if (x <= 1) {
        return uint8_t(x);
    }
    uint8_t low = 1;
    uint8_t hi  = (x > 7904) ? 255 : uint8_t((x >> 5) + 8);
    uint8_t mid;
    do {
        mid = (low + hi) >> 1;
        if (uint16_t(mid * mid) > x) {
            hi = mid - 1;
        } else {
            if (mid == 255) {
                return 255;
            }
            low = mid + 1;
        }
    } while (hi >= low);
    return low - 1;
}

// ---- BPM waves ----

uint16_t beat16(uint16_t bpm, uint32_t timebase) {
    // Convert plain BPM to Q8.8 if caller passed an integer-ish value.
    uint32_t bpm88 = (bpm < 256) ? (uint32_t(bpm) << 8) : uint32_t(bpm);
    return ((millis() - timebase) * bpm88 * 280) >> 16;
}

uint16_t beatsin16(uint16_t bpm, uint16_t lowest, uint16_t highest,
                   uint32_t timebase, uint16_t phase_offset) {
    uint16_t beat = beat16(bpm, timebase);
    uint16_t beatsin = uint16_t(sin16(uint16_t(beat + phase_offset)) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = (uint32_t(beatsin) * (1 + uint32_t(rangewidth))) >> 16;
    return lowest + scaledbeat;
}

uint8_t beatsin8(uint16_t bpm, uint8_t lowest, uint8_t highest,
                 uint32_t timebase, uint8_t phase_offset) {
    uint8_t beat = beat8(bpm, timebase);
    uint8_t beatsin = sin8(uint8_t(beat + phase_offset));
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8(beatsin, rangewidth);
    return lowest + scaledbeat;
}

// ---- RNG ----

uint8_t random8() { return uint8_t(esp_random() & 0xFF); }
uint8_t random8(uint8_t lim) {
    return uint8_t((uint16_t(esp_random() & 0xFF) * lim) >> 8);
}
uint16_t random16() { return uint16_t(esp_random() & 0xFFFF); }
uint16_t random16(uint16_t lim) {
    return uint16_t((uint32_t(esp_random() & 0xFFFF) * lim) >> 16);
}

// ---- Bulk helpers ----

void fill_rainbow(CRGB* targetArray, int numToFill, uint8_t initialhue, uint8_t deltahue) {
    CHSV hsv;
    hsv.h = initialhue;
    hsv.s = 240;
    hsv.v = 255;
    for (int i = 0; i < numToFill; ++i) {
        targetArray[i] = hsv;
        hsv.h += deltahue;
    }
}

void fadeToBlackBy(CRGB* leds, uint16_t numLeds, uint8_t fadeBy) {
    uint8_t scale = 255 - fadeBy;
    for (uint16_t i = 0; i < numLeds; ++i) {
        leds[i].nscale8(scale);
    }
}

// ---- HSV → RGB (FastLED rainbow) ----
// Ported faithfully from FastLED's hsv2rgb.cpp `hsv2rgb_rainbow` (MIT).
// Uses Y1 mode (default), G2 / Gscale disabled.

#define K255 255
#define K171 171
#define K170 170
#define K85  85

void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
    uint8_t hue = hsv.h;
    uint8_t sat = hsv.s;
    uint8_t val = hsv.v;

    uint8_t offset = hue & 0x1F;          // 0..31 within section
    uint8_t offset8 = uint8_t(offset << 3); // 0..248
    uint8_t third = scale8(offset8, (256 / 3));  // max ≈ 83

    uint8_t r, g, b;

    if (!(hue & 0x80)) {
        if (!(hue & 0x40)) {
            if (!(hue & 0x20)) {
                // section 0: R -> O
                r = K255 - third;
                g = third;
                b = 0;
            } else {
                // section 1: O -> Y
                r = K171;
                g = K85 + third;
                b = 0;
            }
        } else {
            if (!(hue & 0x20)) {
                // section 2: Y -> G
                uint8_t twothirds = scale8(offset8, ((256 * 2) / 3));
                r = K171 - twothirds;
                g = K170 + third;
                b = 0;
            } else {
                // section 3: G -> A
                r = 0;
                g = K255 - third;
                b = third;
            }
        }
    } else {
        if (!(hue & 0x40)) {
            if (!(hue & 0x20)) {
                // section 4: A -> B
                uint8_t twothirds = scale8(offset8, ((256 * 2) / 3));
                r = 0;
                g = K171 - twothirds;
                b = K85 + twothirds;
            } else {
                // section 5: B -> P
                r = third;
                g = 0;
                b = K255 - third;
            }
        } else {
            if (!(hue & 0x20)) {
                // section 6: P -> K
                r = K85 + third;
                g = 0;
                b = K171 - third;
            } else {
                // section 7: K -> R
                r = K170 + third;
                g = 0;
                b = K85 - third;
            }
        }
    }

    if (sat != 255) {
        if (sat == 0) {
            r = 255; g = 255; b = 255;
        } else {
            uint8_t desat = 255 - sat;
            desat = scale8_video(desat, desat);
            uint8_t satscale = 255 - desat;

            // FASTLED_SCALE8_FIXED path: plain scale8 (no `+1`, no zero-skip).
            // The old AVR-legacy `if (r) r = scale8(r, satscale) + 1;` path
            // overflows uint8_t when `scale8(255, ~255) + 1 + brightness_floor`
            // wraps to 0, which shows as random missing pixels in fill_rainbow.
            r = scale8(r, satscale);
            g = scale8(g, satscale);
            b = scale8(b, satscale);

            uint8_t brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }

    if (val != 255) {
        val = scale8_video(val, val);
        if (val == 0) {
            r = 0; g = 0; b = 0;
        } else {
            r = scale8(r, val);
            g = scale8(g, val);
            b = scale8(b, val);
        }
    }

    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}
