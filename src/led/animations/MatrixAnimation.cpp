#include "led/animations/MatrixAnimation.h"
#include "led/LedHelpers.h"

void MatrixAnimation::render(RenderContext& ctx) {
    ctx.moveDown();
    ctx.fadeGreen(15);

    for (int strip = 0; strip < ctx.numStrips; strip++) {
        int vuLevel = ctx.vuForStrip(strip);
        if (vuLevel > 180) {
            ctx.leds[ctx.randomLed(ctx.numStrips, strip)] = CRGB::Green;
        }
    }
}
