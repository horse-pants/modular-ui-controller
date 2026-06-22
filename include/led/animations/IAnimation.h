#pragma once

#include "RenderContext.h"

/**
 * @brief Interface for a single LED animation (think C# IAnimation).
 *
 * One concrete subclass per animation, each in its own file under animations/.
 * Each implementation keeps its own running state as members and draws one frame
 * into the supplied RenderContext. AnimationEngine owns the instances and
 * dispatches to them by AnimationType.
 */
class IAnimation {
public:
    virtual ~IAnimation() = default;

    /// Draw one frame into @p ctx.
    virtual void render(RenderContext& ctx) = 0;

    /// Frame cadence for this animation, in milliseconds.
    virtual uint16_t frameIntervalMs() const = 0;
};
