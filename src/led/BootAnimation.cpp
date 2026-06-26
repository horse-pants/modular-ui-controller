#include "led/BootAnimation.h"

#include "led/LedDriver.h"
#include "led/AnimationEngine.h"
#include <Arduino.h>
#include <memory>
#include <string.h>

namespace {
// Linear blend a→b, t in [0,1] (0 = a). Int math so uint8_t channels don't wrap.
CRGB lerpRGB(const CRGB& a, const CRGB& b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return CRGB(
        static_cast<uint8_t>(a.r + (static_cast<int>(b.r) - a.r) * t),
        static_cast<uint8_t>(a.g + (static_cast<int>(b.g) - a.g) * t),
        static_cast<uint8_t>(a.b + (static_cast<int>(b.b) - a.b) * t));
}
}  // namespace

void BootAnimation::runDispersion(LedDriver& driver, AnimationEngine& engine, CRGB* leds,
                                  int numStrips, int ledsPerStrip, int totalLeds,
                                  uint8_t brightness, bool showAnimation, bool whiteMode,
                                  AnimationType currentAnimation, const CRGB& solidColor) {
    if (!leds || totalLeds <= 0) {
        return;
    }

    const int N = numStrips;
    const int M = ledsPerStrip;

    // Normalise the diagonal so the wavefront reaches every corner. With a single
    // strip or single LED the grid collapses to one axis — guard the divisors.
    const float dmax = ((M > 1) ? 1.0f : 0.0f) + ((N > 1) ? 1.0f : 0.0f);
    const float invDmax = (dmax > 0.001f) ? (1.0f / dmax) : 1.0f;

    // Build the target frame we settle into (the saved state). For animations we
    // snapshot one rendered frame; the normal update() loop continues from there.
    std::unique_ptr<CRGB[]> targetBuf;
    CRGB targetSolid(CRGB::Red);
    if (showAnimation) {
        engine.render(currentAnimation);
        targetBuf.reset(new CRGB[totalLeds]);
        memcpy(targetBuf.get(), leds, static_cast<size_t>(totalLeds) * sizeof(CRGB));
    } else if (whiteMode) {
        targetSolid = CRGB::White;
    } else {
        targetSolid = solidColor;
        if (targetSolid.r == 0 && targetSolid.g == 0 && targetSolid.b == 0) {
            targetSolid = CRGB::Red;  // first-boot default
        }
    }

    // Keep the wipe vivid even when the saved brightness is low; we ramp down to the
    // saved value during the settle.
    const uint8_t wipeBrightness = static_cast<uint8_t>(max(static_cast<int>(brightness), 130));

    const int STEPS = 120;
    const float DISP = 0.6f;    // 60% disperse, 40% settle
    const float EDGE = 0.10f;   // width of the white leading highlight

    for (int step = 0; step <= STEPS; step++) {
        const float t = static_cast<float>(step) / STEPS;
        const float hueDrift = t * 90.0f;  // colours flow as the wipe advances
        const bool disperse = (t <= DISP);

        float wf = 0.0f, fp = 0.0f;
        if (disperse) {
            wf = t / DISP;
            wf = 1.0f - (1.0f - wf) * (1.0f - wf);  // ease-out: snappy front, soft tail
        } else {
            fp = (t - DISP) / (1.0f - DISP);
            const float u = -2.0f * fp + 2.0f;
            fp = (fp < 0.5f) ? (2.0f * fp * fp) : (1.0f - u * u / 2.0f);  // ease-in-out
        }

        for (int s = 0; s < N; s++) {
            const float ny = (N > 1) ? static_cast<float>(s) / (N - 1) : 0.0f;  // top→bottom
            for (int l = 0; l < M; l++) {
                // Serpentine: even strips are wired reversed (matches showOTAProgress()),
                // so the diagonal reads as a straight line in physical space.
                const int col = (s % 2 == 0) ? (M - 1 - l) : l;
                const float nx = (M > 1) ? static_cast<float>(col) / (M - 1) : 0.0f;  // left→right
                const int idx = s * M + l;
                const float d = (nx + ny) * invDmax;
                // Hue cycles in BOTH axes plus a time drift → a rich, flowing rainbow
                // field rather than a few flat colour bands.
                const uint8_t hue = static_cast<uint8_t>(nx * 220.0f + ny * 120.0f + hueDrift);

                if (disperse) {
                    const float reveal = wf - d;
                    if (reveal < 0.0f) {
                        leds[idx] = CRGB::Black;  // wavefront hasn't arrived yet
                    } else {
                        const CRGB base = CRGB(CHSV(hue, 255, 255));
                        leds[idx] = (reveal < EDGE)
                            ? lerpRGB(CRGB(CRGB::White), base, reveal / EDGE)  // glowing edge
                            : base;
                    }
                } else {
                    const CRGB disp = CRGB(CHSV(hue, 255, 255));
                    const CRGB target = showAnimation ? targetBuf[idx] : targetSolid;
                    leds[idx] = lerpRGB(disp, target, fp);
                }
            }
        }

        if (disperse) {
            driver.setBrightness(wipeBrightness);
        } else {
            driver.setBrightness(static_cast<uint8_t>(
                wipeBrightness + (static_cast<int>(brightness) - wipeBrightness) * fp));
        }
        driver.show(leds);
        delay(15);
    }

    // Settle exactly on the saved state at the saved brightness.
    if (!showAnimation) {
        for (int i = 0; i < totalLeds; i++) {
            leds[i] = targetSolid;
        }
    }
    driver.setBrightness(brightness);
    driver.show(leds);
}
