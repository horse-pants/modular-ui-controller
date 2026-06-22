#pragma once

#include "IAnimation.h"

/// (Audio) Coloured dots flicked onto the strips on beats, fading slowly.
class ConfettiAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 10; }

private:
    uint8_t hue_ = 0;
};
