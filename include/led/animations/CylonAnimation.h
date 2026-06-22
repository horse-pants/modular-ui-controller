#pragma once

#include "IAnimation.h"

/// Bouncing red eye mirrored across strips (the classic Cylon sweep).
class CylonAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 30; }

private:
    bool goingLeft_ = true;
    uint8_t led_ = 0;
};
