#include "led/animations/RippleAnimation.h"
#include "led/LedHelpers.h"

#include <Arduino.h>  // map(), abs()

void RippleAnimation::render(RenderContext& ctx) {
    // Slower fade to keep ripples visible longer
    ctx.fadeAll(12);

    // Ambient background glow that pulses with audio
    uint8_t ambientBright = 15 + (ctx.audioLevel / 10);
    for (int i = 0; i < ctx.totalLeds; i++) {
        ctx.leds[i] += CHSV(baseHue_ + 128, 255, ambientBright);
    }
    baseHue_++;

    if (spawnCooldown_ > 0) spawnCooldown_--;

    // Spawn new ripple on audio - lower threshold and faster spawning
    if (ctx.audioLevel > 60 && spawnCooldown_ == 0) {
        int spawnChance = map(ctx.audioLevel, 60, 255, 30, 200);
        if (random8() < spawnChance) {
            x_[next_] = random8(ctx.ledsPerStrip);
            y_[next_] = random8(ctx.numStrips);
            hue_[next_] = baseHue_ + random8(64);
            age_[next_] = 1;
            next_ = (next_ + 1) % MAX_RIPPLES;
            spawnCooldown_ = 3;
        }
    }

    // Draw and age all active ripples
    for (int r = 0; r < MAX_RIPPLES; r++) {
        if (age_[r] > 0 && age_[r] < 60) {
            int radius = age_[r];
            uint8_t brightness = 255 - (age_[r] * 4);

            for (int y = 0; y < ctx.numStrips; y++) {
                for (int x = 0; x < ctx.ledsPerStrip; x++) {
                    int dx = x - x_[r];
                    int dy = (y - y_[r]) * 3;  // Scale Y since rows are fewer
                    int dist = sqrt16(dx * dx + dy * dy);

                    if (dist >= radius - 2 && dist <= radius + 2) {
                        int idx = ctx.xyToIndex(x, y);
                        if (idx >= 0) {
                            uint8_t fade = 255 - abs(dist - radius) * 50;
                            ctx.leds[idx] += CHSV(hue_[r], 255, scale8(brightness, fade));
                        }
                    }
                }
            }
            age_[r]++;
        }
    }
}
