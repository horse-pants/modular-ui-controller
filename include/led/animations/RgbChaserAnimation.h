#pragma once

#include "IAnimation.h"

/// A single pixel walks the whole chain, cycling R→G→B each lap.
class RgbChaserAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 30; }

private:
    CRGB colour_ = CRGB::Red;
    int colourCount_ = 0;
    uint8_t led_ = 0;
};
