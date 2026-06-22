#pragma once

#include "IAnimation.h"

/// (Audio) Classic VU meter: green→orange→red bars filling out from centre.
class VuAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 5; }
};
