#pragma once

#include "IAnimation.h"

/// (Audio) Red→yellow flames pushed outward from each strip centre by the VU.
class FireAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 20; }
};
