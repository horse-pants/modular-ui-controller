#include "led/animations/WaveAnimation.h"
#include "led/LedHelpers.h"

void WaveAnimation::render(RenderContext& ctx) {
    offset_ += 3;

    for (int y = 0; y < ctx.numStrips; y++) {
        uint16_t rowPhase = offset_ + (y * 40);

        for (int x = 0; x < ctx.ledsPerStrip; x++) {
            uint8_t wave = sin8(x * 8 + rowPhase);

            uint8_t hue = wave + (y * 30);
            uint8_t brightness = 150 + (wave / 3);

            int idx = ctx.xyToIndex(x, y);
            if (idx >= 0) {
                ctx.leds[idx] = CHSV(hue, 255, brightness);
            }
        }
    }
}
