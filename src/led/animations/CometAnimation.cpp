#include "led/animations/CometAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // min()

void CometAnimation::render(RenderContext& ctx) {
    ctx.fadeAll(40);

    for (int c = 0; c < min(3, ctx.numStrips); c++) {
        // Ensure comet row is valid
        if (row_[c] >= ctx.numStrips) {
            row_[c] = c % ctx.numStrips;
        }

        // Draw comet head and tail
        for (int t = 0; t < 12; t++) {
            int x = pos_[c] - (t * dir_[c]);
            if (x >= 0 && x < ctx.ledsPerStrip) {
                int idx = ctx.xyToIndex(x, row_[c]);
                if (idx >= 0) {
                    uint8_t brightness = 255 - (t * 20);
                    ctx.leds[idx] += CHSV(hue_[c], 200, brightness);
                }
            }
        }

        // Move comet
        pos_[c] += dir_[c] * 2;

        // Bounce or wrap to next row
        if (pos_[c] >= ctx.ledsPerStrip + 10) {
            pos_[c] = ctx.ledsPerStrip - 1;
            dir_[c] = -1;
            row_[c] = (row_[c] + 1) % ctx.numStrips;
            hue_[c] += 30;
        } else if (pos_[c] < -10) {
            pos_[c] = 0;
            dir_[c] = 1;
            row_[c] = (row_[c] + 1) % ctx.numStrips;
            hue_[c] += 30;
        }
    }
}
