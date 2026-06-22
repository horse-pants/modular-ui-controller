#include "led/animations/CylonAnimation.h"
#include "led/LedHelpers.h"

void CylonAnimation::render(RenderContext& ctx) {
    ctx.leds[led_] = CRGB::Red;
    if (ctx.numStrips >= 2) {
        ctx.leds[((ctx.ledsPerStrip * 2) - 1) - led_] = CRGB::Red;
    }
    if (ctx.numStrips >= 3) {
        ctx.leds[led_ + (ctx.ledsPerStrip * 2)] = CRGB::Red;
    }
    if (ctx.numStrips >= 4) {
        ctx.leds[((ctx.ledsPerStrip * 4) - 1) - led_] = CRGB::Red;
    }
    if (ctx.numStrips >= 5) {
        ctx.leds[led_ + (ctx.ledsPerStrip * 4)] = CRGB::Red;
    }

    ctx.fadeAll(35);

    if (goingLeft_) {
        led_++;
        if (led_ == ctx.ledsPerStrip - 1) {
            goingLeft_ = false;
        }
    } else {
        led_--;
        if (led_ == 0) {
            goingLeft_ = true;
        }
    }
}
