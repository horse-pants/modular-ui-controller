#include "led/animations/FireAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // map()

void FireAnimation::render(RenderContext& ctx) {
    ctx.fadeGreen(15);
    ctx.fadeRed(7);
    for (int strip = 0; strip < ctx.numStrips; strip++) {
        ctx.moveFromCentre(strip);
        int vuLevel = ctx.vuForStrip(strip);
        int gChannel = map(vuLevel, 0, 255, 0, 255);
        ctx.leds[ctx.centreOfStrip(strip)] = CRGB(255, gChannel, 0);
    }
}
