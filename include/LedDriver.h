#pragma once

#include "LedHelpers.h"
#include <led_strip.h>

// Thin driver abstraction so animations / LEDManager don't depend on a
// specific LED library. Current implementation backs onto Espressif's
// led_strip component (IDF 5 RMT + DMA). When FastLED's IDF 5 RMT path is
// fixed, swap this file's .cpp for a FastLED-backed one — no other changes.
class LedDriver {
public:
    LedDriver() = default;
    ~LedDriver() { end(); }

    LedDriver(const LedDriver&) = delete;
    LedDriver& operator=(const LedDriver&) = delete;

    bool begin(int dataPin, int numPixels);
    void end();

    void setBrightness(uint8_t b) { brightness_ = b; }
    uint8_t getBrightness() const { return brightness_; }

    // Applies brightness to the buffer and pushes the frame to hardware.
    void show(const CRGB* buffer);

    // Clears and immediately refreshes (all pixels off).
    void clear();

private:
    led_strip_handle_t strip_ = nullptr;
    uint8_t brightness_ = 255;
    int numPixels_ = 0;
};
