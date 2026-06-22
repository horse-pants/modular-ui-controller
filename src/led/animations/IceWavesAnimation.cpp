#include "led/animations/IceWavesAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // map()

void IceWavesAnimation::render(RenderContext& ctx) {
    ctx.fadeAll(15);
    for (int strip = 0; strip < ctx.numStrips; strip++) {
        ctx.moveFromCentre(strip);
        int vuLevel = ctx.vuForStrip(strip);
        int rChannel = map(vuLevel, 0, 255, 100, 0);
        int gChannel = map(vuLevel, 0, 255, 0, 255);
        ctx.leds[ctx.centreOfStrip(strip)] = CRGB(rChannel, gChannel, 255);
    }
}
