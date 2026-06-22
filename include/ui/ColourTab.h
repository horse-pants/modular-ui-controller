#pragma once

#include <lvgl.h>
#include <memory>
#include "ui/BrightnessSlider.h"
#include "ui/ColourWheel.h"
#include "ui/EffectsList.h"
#include "ui/WhiteButton.h"
#include "ui/VuButton.h"

/**
 * @brief The "Colour" control tab — the main control surface.
 *
 * Owns the five widgets (brightness fader, VU toggle, colour wheel, white
 * toggle, effects dropdown), builds their layout into a tab page, and holds the
 * control logic that ties them to the LED manager. UIManager delegates the
 * screen/web control actions here; the widget callbacks (VuButton/WhiteButton)
 * reach these via the thin delegators on UIManager.
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

    /// Reflect the LEDManager's loaded state onto the widgets (first boot).
    void syncWithLed();

private:
    std::unique_ptr<BrightnessSlider> brightnessSlider_;
    std::unique_ptr<ColourWheel> colourWheel_;
    std::unique_ptr<EffectsList> effectsList_;
    std::unique_ptr<WhiteButton> whiteButton_;
    std::unique_ptr<VuButton> vuButton_;
};
