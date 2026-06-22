#include "led/animations/VuAnimation.h"
#include "led/LedHelpers.h"

void VuAnimation::render(RenderContext& ctx) {
    ctx.fadeAll(20);
    for (int strip = 0; strip < ctx.numStrips; strip++) {
        ctx.fillFromCentre(strip, ctx.vuForStrip(strip), CRGB::Green, CRGB::Orange, CRGB::Red);
    }
}
