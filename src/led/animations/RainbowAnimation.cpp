#include "led/animations/RainbowAnimation.h"
#include "led/LedHelpers.h"

void RainbowAnimation::render(RenderContext& ctx) {
    for (int i = 0; i < ctx.totalLeds; i++) {
        ctx.leds[i] = CHSV((i * 256 / ctx.totalLeds) + hue_, 255, 255);
    }
    hue_++;
}
