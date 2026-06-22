#include "led/animations/PurpleRainAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // map()

void PurpleRainAnimation::render(RenderContext& ctx) {
    ctx.fadeAll(15);
    for (int strip = 0; strip < ctx.numStrips; strip++) {
        ctx.moveFromCentre(strip);
        int vuLevel = ctx.vuForStrip(strip);
        int rChannel = map(vuLevel, 0, 255, 0, 255);
        ctx.leds[ctx.centreOfStrip(strip)] = CRGB(rChannel, 0, 255);
    }
}
