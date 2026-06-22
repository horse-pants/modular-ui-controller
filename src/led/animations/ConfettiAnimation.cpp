#include "led/animations/ConfettiAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // map()

void ConfettiAnimation::render(RenderContext& ctx) {
    // Very gentle fade so dots linger and fade out smoothly
    ctx.fadeAll(3);

    // Gentler scaling of dots with audio level
    int numDots = 0;
    if (ctx.audioLevel > 50) {
        int spawnChance = map(ctx.audioLevel, 50, 255, 30, 120);
        if (random8() < spawnChance) {
            numDots = 1;
            if (ctx.audioLevel > 180 && random8() < 50) {
                numDots = 2;
            }
        }
    }

    uint8_t dotBright = map(ctx.audioLevel, 0, 255, 180, 255);

    for (int i = 0; i < numDots; i++) {
        int pos = random16(ctx.totalLeds);
        uint8_t sat = (ctx.audioLevel > 220 && random8() < 25) ? 0 : 255;
        ctx.leds[pos] = CHSV(hue_ + random8(96), sat, dotBright);
    }

    hue_ += 1;
}
