#include "led/animations/RgbChaserAnimation.h"
#include "led/LedHelpers.h"

void RgbChaserAnimation::render(RenderContext& ctx) {
    ctx.leds[led_] = colour_;
    led_++;
    if (led_ == ctx.totalLeds) {
        led_ = 0;

        colourCount_++;
        if (colourCount_ == 3) colourCount_ = 0;
        switch (colourCount_) {
            case 0: colour_ = CRGB::Red; break;
            case 1: colour_ = CRGB::Green; break;
            case 2: colour_ = CRGB::Blue; break;
            default: break;
        }
    }
}
