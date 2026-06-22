#pragma once

#include "IAnimation.h"

/// (Audio) Concentric rings bloom from random points on loud hits, over an
/// audio-pulsed ambient glow. Up to 8 ripples alive at once.
class RippleAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 15; }

private:
    static constexpr int MAX_RIPPLES = 8;
    uint8_t age_[MAX_RIPPLES] = {};
    int8_t  x_[MAX_RIPPLES] = {};
    int8_t  y_[MAX_RIPPLES] = {};
    uint8_t hue_[MAX_RIPPLES] = {};
    uint8_t next_ = 0;
    uint8_t baseHue_ = 0;
    uint8_t spawnCooldown_ = 0;
};
