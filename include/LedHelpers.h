#pragma once

#include <Arduino.h>
#include <stdint.h>

// FastLED-compatible API shim — the subset of FastLED used by this project's
// animations (CRGB / CHSV, hsv2rgb_rainbow, 8-bit math, time-based waves, RNG).
// Algorithms adapted from FastLED (MIT-licensed): https://github.com/FastLED/FastLED

using fract8 = uint8_t;

struct CHSV;

struct CRGB {
    union {
        struct {
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t raw[3];
    };

    // HTML/CSS standard color codes — matches FastLED's HTMLColorCode subset.
    enum HTMLColorCode : uint32_t {
        Red    = 0xFF0000,
        Green  = 0x008000,
        Blue   = 0x0000FF,
        White  = 0xFFFFFF,
        Black  = 0x000000,
        Orange = 0xFFA500,
    };

    constexpr CRGB() : r(0), g(0), b(0) {}
    constexpr CRGB(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
    constexpr CRGB(HTMLColorCode code)
        : r((uint32_t(code) >> 16) & 0xFF),
          g((uint32_t(code) >> 8) & 0xFF),
          b(uint32_t(code) & 0xFF) {}

    CRGB(const CHSV& hsv);
    CRGB& operator=(const CHSV& hsv);

    CRGB& operator+=(const CRGB& rhs);
    CRGB& nscale8(uint8_t scale);
};

struct CHSV {
    uint8_t h;
    uint8_t s;
    uint8_t v;
    constexpr CHSV() : h(0), s(0), v(0) {}
    constexpr CHSV(uint8_t h_, uint8_t s_, uint8_t v_) : h(h_), s(s_), v(v_) {}
};

// 8-bit / 16-bit math primitives
inline uint8_t scale8(uint8_t i, fract8 scale) {
    return (uint16_t(i) * (1 + uint16_t(scale))) >> 8;
}
inline uint8_t scale8_video(uint8_t i, fract8 scale) {
    return ((uint16_t(i) * uint16_t(scale)) >> 8) + ((i && scale) ? 1 : 0);
}
inline uint8_t qadd8(uint8_t a, uint8_t b) {
    uint16_t t = uint16_t(a) + uint16_t(b);
    return t > 255 ? 255 : uint8_t(t);
}
inline uint8_t qsub8(uint8_t a, uint8_t b) {
    return a > b ? uint8_t(a - b) : 0;
}

// 8-bit sin/cos — 256 entries / period, output range 0..255.
uint8_t sin8(uint8_t theta);
inline uint8_t cos8(uint8_t theta) { return sin8(theta + 64); }

// 16-bit sin/cos — 65536 entries / period, output range -32768..32767.
int16_t sin16(uint16_t theta);

// Integer square root, 16-bit input → 8-bit output.
uint8_t sqrt16(uint16_t x);

// BPM sawtooth/sine waves driven by millis().
uint16_t beat16(uint16_t bpm, uint32_t timebase = 0);
inline uint8_t beat8(uint16_t bpm, uint32_t timebase = 0) {
    return beat16(bpm, timebase) >> 8;
}
uint16_t beatsin16(uint16_t bpm,
                   uint16_t lowest = 0,
                   uint16_t highest = 65535,
                   uint32_t timebase = 0,
                   uint16_t phase_offset = 0);
uint8_t beatsin8(uint16_t bpm,
                 uint8_t lowest = 0,
                 uint8_t highest = 255,
                 uint32_t timebase = 0,
                 uint8_t phase_offset = 0);

// RNG — backed by esp_random().
uint8_t random8();
uint8_t random8(uint8_t lim);
uint16_t random16();
uint16_t random16(uint16_t lim);

// Bulk helpers
void fill_rainbow(CRGB* targetArray, int numToFill, uint8_t initialhue, uint8_t deltahue);
void fadeToBlackBy(CRGB* leds, uint16_t numLeds, uint8_t fadeBy);

// HSV → RGB using FastLED's rainbow algorithm (yellow gets more visual weight
// than standard HSV). Animations are tuned against this curve — don't substitute.
void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb);
