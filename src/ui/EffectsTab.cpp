#include "ui/EffectsTab.h"

#include "modular-ui.h"
#include "ui/ColourTab.h"
#include "ui/ui.h"
#include "led/LEDManager.h"

#include <Logger.h>

EffectsTab::~EffectsTab() {
    g_effectGrid = nullptr;
}

bool EffectsTab::build(lv_obj_t* parent) {
    lv_obj_set_style_pad_all(parent, UI_PADDING_LARGE, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    grid_.reset(new EffectGrid());
    if (!grid_->initialize(parent)) {
        Logger.warning("EffectsTab: grid could not be built");
        grid_.reset();
        return false;
    }
    g_effectGrid = grid_.get();

    grid_->setCallback([this](int animation) {
        applySelection(animation);
    });

    return true;
}

void EffectsTab::applySelection(int animation) {
    if (g_ledManager) {
        if (animation == EffectGrid::OFF_INDEX) {
            // Turning effects off falls back to the saved solid colour, which is
            // what setSolidColor() restores (it also clears white + animation).
            g_ledManager->setSolidColor(g_ledManager->getSolidColor());
        } else {
            g_ledManager->setCurrentAnimation(static_cast<AnimationType>(animation));
            g_ledManager->setWhiteMode(false);
            g_ledManager->setAnimationEnabled(true);
        }
    }

    // The Colour tab shows the running effect on its bottom bar, and White is
    // mutually exclusive with an animation.
    if (g_colourTab) {
        g_colourTab->showEffect(animation);
    }
    if (animation != EffectGrid::OFF_INDEX && g_whiteButton) {
        g_whiteButton->setState(false, false);
    }

    Logger.info("Effect: %s",
                animation == EffectGrid::OFF_INDEX
                    ? "off"
                    : animationDescription(static_cast<AnimationType>(animation)));

    updateWebUi();
}

void EffectsTab::setSelected(int animation) {
    if (grid_) {
        grid_->setSelected(animation);
    }
}

void EffectsTab::syncWithLed() {
    if (!grid_) return;

    const bool on = g_ledManager && g_ledManager->isAnimationEnabled();
    grid_->setSelected(on ? static_cast<int>(g_ledManager->getCurrentAnimation())
                          : EffectGrid::OFF_INDEX);
}
