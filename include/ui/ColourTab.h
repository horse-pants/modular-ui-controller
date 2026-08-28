#pragma once

#include <lvgl.h>
#include <memory>
#include "ui/BrightnessSlider.h"
#include "ui/ColourWheel.h"
#include "ui/WhiteButton.h"
#include "ui/VuButton.h"

/**
 * @brief The "Colour" control tab — the main control surface.
 *
 * Owns the wheel, the brightness fader and the White/VU toggles, builds their
 * layout into a tab page, and holds the control logic that ties them to the LED
 * manager. UIManager delegates the screen/web control actions here; the widget
 * callbacks reach these via the thin delegators on UIManager.
 *
 * Layout runs top to bottom in one column: wheel, brightness, the two toggles,
 * then a bar showing the running effect. The effect PICKER lives on its own tab
 * (see EffectsTab) — the bar here only reports and, when tapped, jumps to it.
 *
 * Publishes the g_* widget globals on build() so the rest of the codebase keeps
 * its existing direct access, and clears them on destruction.
 */
class ColourTab {
public:
    ~ColourTab();

    /// Build the widgets + layout into @p parent (the Colour tab page).
    bool build(lv_obj_t* parent);

    // Control actions (called from UIManager delegators, widget callbacks, web).
    void applyCurrentColor();
    void logAndUpdateVuState(bool newState);
    void logAndUpdateWhiteState(bool newState);
    void setVuState(bool newState);
    void setWhiteState(bool newState);
    void setAnimationState(bool newState);
    void setAnimation(int animation);

    /// Update the effect bar. -1 (EffectGrid::OFF_INDEX) shows "No effect".
    void showEffect(int animation);

    /// Reflect the LEDManager's loaded state onto the widgets (first boot).
    void syncWithLed();

private:
    void buildEffectBar(lv_obj_t* parent);

    std::unique_ptr<BrightnessSlider> brightnessSlider_;
    std::unique_ptr<ColourWheel> colourWheel_;
    std::unique_ptr<WhiteButton> whiteButton_;
    std::unique_ptr<VuButton> vuButton_;

    // Effect bar: a swatch, the running effect's name, and a chevron. Tapping it
    // switches to the Effects tab.
    lv_obj_t* effectBar_ = nullptr;
    lv_obj_t* effectSwatch_ = nullptr;
    lv_obj_t* effectName_ = nullptr;
};
