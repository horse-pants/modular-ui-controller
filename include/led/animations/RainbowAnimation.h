#pragma once

#include "IAnimation.h"

/// Full-strip hue sweep that scrolls over time.
class RainbowAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 10; }

private:
    uint8_t hue_ = 0;
};
