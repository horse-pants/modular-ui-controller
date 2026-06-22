#include "led/animations/BeatSineAnimation.h"
#include "led/LedHelpers.h"

void BeatSineAnimation::render(RenderContext& ctx) {
    uint16_t beatA = beatsin16(30, 0, 255);
    uint16_t beatB = beatsin16(20, 0, 255);
    fill_rainbow(ctx.leds, ctx.totalLeds, (beatA + beatB) / 2, 2);
}
