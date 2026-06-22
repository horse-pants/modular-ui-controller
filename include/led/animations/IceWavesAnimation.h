#pragma once

#include "IAnimation.h"

/// (Audio) Cyan→white pulses pushed outward from each strip centre by the VU.
class IceWavesAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 20; }
};
