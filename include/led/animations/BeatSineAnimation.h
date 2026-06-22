#pragma once

#include "IAnimation.h"

/// Two beat-synced sine waves blended into a moving rainbow fill.
class BeatSineAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 50; }
};
