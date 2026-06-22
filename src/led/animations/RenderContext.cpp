#include "led/animations/RenderContext.h"

#include <Arduino.h>  // map(), max()

void RenderContext::fadeAll(int amount) {
    for (int i = 0; i < totalLeds; i++) {
        CRGB colour = leds[i];
        int red = colour.r - amount;
        if (red <= 0) red = 0;

        int green = colour.g - amount;
        if (green <= 0) green = 0;

        int blue = colour.b - amount;
        if (blue <= 0) blue = 0;

        colour.r = red;
        colour.g = green;
        colour.b = blue;
        leds[i] = colour;
    }
}

void RenderContext::fadeRed(int amount) {
    for (int i = 0; i < totalLeds; i++) {
        CRGB colour = leds[i];
        int red = colour.r - amount;
        if (red <= 0) red = 0;

        colour.r = red;
        leds[i] = colour;
    }
}

void RenderContext::fadeGreen(int amount) {
    for (int i = 0; i < totalLeds; i++) {
        CRGB colour = leds[i];
        int green = colour.g - amount;
        if (green <= 0) green = 0;

        colour.g = green;
        leds[i] = colour;
    }
}

int RenderContext::centreOfStrip(int strip) const {
    return (strip * ledsPerStrip) + (ledsPerStrip / 2);
}

void RenderContext::fillFromCentre(int strip, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3) {
    int ledVu = map(vuValue, 0, 255, 0, (ledsPerStrip + 1) / 2);
    int centre = centreOfStrip(strip);
    leds[centre] = colour1;
    for (int i = 1; i <= ledVu; i++) {
        leds[centre + i] = pickColour(i, colour1, colour2, colour3);
        leds[centre - i] = pickColour(i, colour1, colour2, colour3);
    }
}

void RenderContext::moveFromCentre(int strip) {
    int centre = centreOfStrip(strip);
    int ledsPerSide = ledsPerStrip / 2;

    for (int i = ledsPerSide; i >= 0; i--) {
        leds[centre + i] = leds[centre + i - 1];
        leds[centre - i] = leds[centre - i + 1];
    }
}

void RenderContext::moveDown() {
    for (int strip = numStrips - 1; strip > 0; strip--) {
        for (int led = 0; led < ledsPerStrip; led++) {
            if (strip % 2 == 1) {
                leds[(ledsPerStrip * strip) + (ledsPerStrip - led) - 1] =
                    leds[(ledsPerStrip * (strip - 1)) + led];
            } else {
                leds[led + (ledsPerStrip * strip)] =
                    leds[(ledsPerStrip * (strip - 1)) + (ledsPerStrip - led) - 1];
            }
        }
    }
}

int RenderContext::randomLed(int divisions, int division) const {
    int range = (ledsPerStrip + 1) / divisions;
    int led = random16(range);
    return led + (range * division);
}

CRGB RenderContext::pickColour(int led, CRGB colour1, CRGB colour2, CRGB colour3) const {
    if (led >= 0 && led <= 7) {
        return colour1;
    } else if (led >= 8 && led <= 11) {
        return colour2;
    } else {
        return colour3;
    }
}

int RenderContext::xyToIndex(int x, int y) const {
    // Convert 2D coordinates to linear index accounting for snake wiring.
    // y = row (strip), x = column position within strip.
    // Even rows (0, 2, 4...) are wired right-to-left (reversed);
    // odd rows (1, 3, 5...) are wired left-to-right (normal).
    if (x < 0 || x >= ledsPerStrip || y < 0 || y >= numStrips) {
        return -1;  // Out of bounds
    }

    int index;
    if (y % 2 == 0) {
        index = (y * ledsPerStrip) + (ledsPerStrip - 1 - x);  // reversed
    } else {
        index = (y * ledsPerStrip) + x;                       // normal
    }
    return index;
}

int RenderContext::vuForStrip(int strip) const {
    if (numStrips <= 0 || ledsPerStrip <= 0 || strip < 0 || strip >= numStrips) {
        return 0;
    }

    // For AUDIO_BAND_COUNT (7) or fewer strips, each strip owns a contiguous,
    // non-overlapping range of VU bands [lo, hi] and shows the max across it.
    // Table indexed [numStrips-1][strip]; only the first numStrips entries of each
    // row are populated (the rest stay {0,0} and are never read).
    if (numStrips <= AUDIO_BAND_COUNT) {
        struct BandRange { uint8_t lo, hi; };
        static const BandRange kStripBands[AUDIO_BAND_COUNT][AUDIO_BAND_COUNT] = {
            /* 1 strip  */ {{0, 6}},
            /* 2 strips */ {{0, 3}, {4, 6}},
            /* 3 strips */ {{0, 1}, {2, 4}, {5, 6}},
            /* 4 strips */ {{0, 1}, {2, 3}, {4, 5}, {6, 6}},
            /* 5 strips */ {{0, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 6}},
            /* 6 strips */ {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 6}},
            /* 7 strips */ {{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}},
        };
        const BandRange r = kStripBands[numStrips - 1][strip];
        int level = 0;
        for (int b = r.lo; b <= r.hi; b++) {
            level = max(level, vuLevels[b]);
        }
        return level;
    }

    // More strips than VU bands — linearly interpolate between adjacent bands.
    float vuIndex = (float)strip * (AUDIO_BAND_COUNT - 1) / (numStrips - 1);
    int lowIndex = (int)vuIndex;
    int highIndex = lowIndex + 1;
    if (highIndex >= AUDIO_BAND_COUNT) {
        return vuLevels[AUDIO_BAND_COUNT - 1];
    }
    float fraction = vuIndex - lowIndex;
    return (int)(vuLevels[lowIndex] * (1.0f - fraction) + vuLevels[highIndex] * fraction);
}
