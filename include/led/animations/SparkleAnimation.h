#pragma once

#include "IAnimation.h"

/// Twinkling coloured sparkles with occasional white accents.
class SparkleAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 30; }

private:
    uint8_t hue_ = 0;
};
