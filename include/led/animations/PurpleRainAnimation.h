#pragma once

#include "IAnimation.h"

/// (Audio) Magenta pulses pushed outward from each strip centre by the VU.
class PurpleRainAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 20; }
};
