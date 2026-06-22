#pragma once

#include "IAnimation.h"

/// (Audio) Green "rain" scrolling down the rows, seeded by loud bands.
class MatrixAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 50; }
};
