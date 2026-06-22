#pragma once

#include "IAnimation.h"

/// Phase-offset sine waves travelling across each row.
class WaveAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 25; }

private:
    uint16_t offset_ = 0;
};
