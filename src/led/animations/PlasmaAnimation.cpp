#include "led/animations/PlasmaAnimation.h"
#include "led/LedHelpers.h"

void PlasmaAnimation::render(RenderContext& ctx) {
    time_ += 1;

    for (int y = 0; y < ctx.numStrips; y++) {
        for (int x = 0; x < ctx.ledsPerStrip; x++) {
            uint8_t v1 = sin8(x * 10 + time_);
            uint8_t v2 = sin8(y * 15 + time_ * 2);
            uint8_t v3 = sin8((x + y) * 8 + time_);
            uint8_t v4 = sin8(sqrt16((x * x + y * y) * 64) + time_ * 3);

            uint8_t hue = (v1 + v2 + v3 + v4) / 4;
            uint8_t brightness = 200 + sin8(time_ + x * 5) / 5;

            int idx = ctx.xyToIndex(x, y);
            if (idx >= 0) {
                ctx.leds[idx] = CHSV(hue, 255, brightness);
            }
        }
    }
}
