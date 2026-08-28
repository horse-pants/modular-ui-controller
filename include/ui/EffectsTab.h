#pragma once

#include <lvgl.h>
#include <memory>
#include "ui/EffectGrid.h"

/**
 * @brief The "Effects" tab — the animation picker and nothing else.
 *
 * Split out of the Colour tab, where the old dropdown was both the loudest
 * control on screen and the least used. Giving effects their own page freed the
 * Colour tab's layout and gave the fifteen animations room to be shown rather
 * than listed.
 *
 * Thin by design: it owns the grid and turns a selection into LED state, then
 * hands off to UIManager so the Colour tab's effect bar and the web UI follow.
 */
class EffectsTab {
public:
    ~EffectsTab();

    /// Build the grid into @p parent (the Effects tab page).
    bool build(lv_obj_t* parent);

    /// Reflect the LED manager's current animation. Never applies it back.
    void syncWithLed();

    /// Show a selection made elsewhere (web, MQTT, the Colour tab).
    void setSelected(int animation);

private:
    void applySelection(int animation);

    std::unique_ptr<EffectGrid> grid_;
};
