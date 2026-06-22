#include "led/animations/SparkleAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // max()

void SparkleAnimation::render(RenderContext& ctx) {
    // Fade existing LEDs
    ctx.fadeAll(25);

    // Add random sparkles, scaled with total LED count
    int numSparkles = max(1, ctx.totalLeds / 25);
    for (int i = 0; i < numSparkles; i++) {
        if (random8() < 90) {  // ~35% chance per sparkle slot
            int idx = random16(ctx.totalLeds);
            if (random8() < 30) {  // ~12% white
                ctx.leds[idx] = CRGB::White;
            } else {
                ctx.leds[idx] = CHSV(hue_ + random8(160), 220, 255);
            }
        }
    }
    hue_ += 3;  // Faster hue rotation for more colour variety
}
