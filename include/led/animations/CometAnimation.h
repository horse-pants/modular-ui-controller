#pragma once

#include "IAnimation.h"

/// Up to three comets with fading tails, bouncing and hopping between rows.
class CometAnimation : public IAnimation {
public:
    void render(RenderContext& ctx) override;
    uint16_t frameIntervalMs() const override { return 20; }

private:
    int16_t pos_[3] = {0, 0, 0};      // position along path
    int8_t  row_[3] = {0, 2, 4};      // row each comet rides
    int8_t  dir_[3] = {1, -1, 1};     // travel direction
    uint8_t hue_[3] = {0, 85, 170};   // colour per comet
};
