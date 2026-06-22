#pragma once

#include "IAnimation.h"

/// Swirling multi-sine plasma field across the 2D matrix.
class PlasmaAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 20; }

private:
    uint16_t time_ = 0;
};
