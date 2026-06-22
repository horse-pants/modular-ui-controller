#pragma once

#include "led/LedHelpers.h"
#include "audio/AudioFrame.h"

/**
 * @brief The shared "canvas" handed to every IAnimation each frame.
 *
 * Bundles the pixel buffer + strip geometry + latest audio snapshot, plus the
 * drawing helpers animations build on (fades, centre-out fills, snake-wired xy
 * mapping, VU-band → strip mapping). AnimationEngine owns one RenderContext and
 * refreshes its buffer/geometry/audio each tick; animations only read/write
 * through it and keep their own private running state.
 *
 * The pixel buffer is NON-owning — LEDManager allocates it (it's also used for
 * solid-colour / white / OTA fills) and hands the pointer in via the engine.
 */
struct RenderContext {
    // Pixel buffer (non-owning) + geometry
    CRGB* leds = nullptr;
    int totalLeds = 0;
    int numStrips = 0;
    int ledsPerStrip = 0;

    // Latest audio snapshot
    int vuLevels[AUDIO_BAND_COUNT] = {};
    int audioLevel = 0;

    // --- Drawing helpers ---
    void fadeAll(int amount);
    void fadeRed(int amount);
    void fadeGreen(int amount);
    int  centreOfStrip(int strip) const;
    void fillFromCentre(int strip, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3);
    void moveFromCentre(int strip);
    void moveDown();
    int  randomLed(int divisions, int division) const;
    CRGB pickColour(int led, CRGB colour1, CRGB colour2, CRGB colour3) const;
    int  xyToIndex(int x, int y) const;  // 2D coord → snake-wired linear index
    int  vuForStrip(int strip) const;    // map a VU band onto strip @p strip
};
