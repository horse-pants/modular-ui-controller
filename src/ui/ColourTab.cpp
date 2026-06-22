#include "ui/ColourTab.h"
#include "modular-ui.h"
#include "ui/ui.h"
#include <Logger.h>

ColourTab::~ColourTab() {
    // Clear the widget globals before the widgets are destroyed (member
    // unique_ptrs run after this body), so nothing dangles during teardown.
    g_brightnessSlider = nullptr;
    g_colourWheel = nullptr;
    g_effectsList = nullptr;
    g_whiteButton = nullptr;
    g_vuButton = nullptr;
}

bool ColourTab::build(lv_obj_t* parent) {
    // ==========================================================================
    // COLOUR tab - Fader style layout
    // Layout: Left column (fader + VU) | Center (wheel) | Bottom (White + Effects)
    // ==========================================================================
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, UI_PADDING_MEDIUM, 0);
    lv_obj_set_style_pad_row(parent, UI_SPACING_NORMAL, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // --- Main content row: left column (fader+VU) + wheel ---
    lv_obj_t* mainRow = lv_obj_create(parent);
    lv_obj_set_size(mainRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(mainRow, 1);
    lv_obj_set_flex_flow(mainRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mainRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(mainRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mainRow, 0, 0);
    lv_obj_set_style_pad_all(mainRow, 0, 0);
    lv_obj_set_style_pad_left(mainRow, UI_PADDING_LARGE, 0);  // Push fader right a bit
    lv_obj_set_style_pad_column(mainRow, UI_SPACING_LOOSE, 0);
    lv_obj_clear_flag(mainRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mainRow, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // --- Left column: Fader + VU button (stacked vertically) ---
    lv_obj_t* leftColumn = lv_obj_create(mainRow);
    lv_obj_set_size(leftColumn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(leftColumn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(leftColumn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(leftColumn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leftColumn, 0, 0);
    lv_obj_set_style_pad_all(leftColumn, 0, 0);
    lv_obj_set_style_pad_row(leftColumn, UI_SPACING_NORMAL, 0);
    lv_obj_clear_flag(leftColumn, LV_OBJ_FLAG_SCROLLABLE);

    // Brightness fader (vertical)
    brightnessSlider_.reset(new BrightnessSlider(255));
    g_brightnessSlider = brightnessSlider_.get();
    if (brightnessSlider_) {
        brightnessSlider_->setCallback([this](int newBrightness) {
            if (g_ledManager) {
                g_ledManager->setBrightness(newBrightness);
                g_ledManager->setVuMode(false);
            }
            if (vuButton_) {
                vuButton_->setState(false);
            }
            updateWebUi();
        });

        int initialBrightness = g_ledManager ? g_ledManager->getBrightness() : 128;
        if (!brightnessSlider_->initialize(leftColumn, initialBrightness)) {
            return false;
        }
    }

    // VU button (directly under fader)
    vuButton_.reset(new VuButton());
    g_vuButton = vuButton_.get();
    if (vuButton_ && !vuButton_->initialize(leftColumn)) {
        return false;
    }
    if (vuButton_) {
        vuButton_->setCallback([this](bool newState) {
            if (g_ledManager) {
                g_ledManager->setVuMode(newState);
            }
            updateWebUi();
        });
    }

    // --- Center: Colour wheel (vertically centered with left column) ---
    lv_obj_t* wheelContainer = lv_obj_create(mainRow);
    lv_obj_set_flex_grow(wheelContainer, 1);
    lv_obj_set_height(wheelContainer, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(wheelContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wheelContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(wheelContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wheelContainer, 0, 0);
    lv_obj_set_style_pad_all(wheelContainer, 20, 0);  // Room for indicator
    lv_obj_clear_flag(wheelContainer, LV_OBJ_FLAG_SCROLLABLE);

    colourWheel_.reset(new ColourWheel());
    g_colourWheel = colourWheel_.get();
    if (colourWheel_ && !colourWheel_->initialize(wheelContainer, 180, true)) {
        return false;
    }
    if (colourWheel_) {
        // Wheel reports the picked colour, but applyCurrentColor() re-reads it from
        // the wheel itself — so the r/g/b params are intentionally unnamed (unused).
        colourWheel_->setCallback([this](uint8_t, uint8_t, uint8_t) {
            this->applyCurrentColor();
        });
    }

    // --- Bottom row: White + Effects buttons ---
    lv_obj_t* bottomRow = lv_obj_create(parent);
    lv_obj_set_size(bottomRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottomRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottomRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bottomRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottomRow, 0, 0);
    lv_obj_set_style_pad_all(bottomRow, 0, 0);
    lv_obj_set_style_pad_column(bottomRow, UI_SPACING_NORMAL, 0);
    lv_obj_clear_flag(bottomRow, LV_OBJ_FLAG_SCROLLABLE);

    // White button
    whiteButton_.reset(new WhiteButton());
    g_whiteButton = whiteButton_.get();
    if (whiteButton_ && !whiteButton_->initialize(bottomRow)) {
        return false;
    }

    // Effects dropdown
    effectsList_.reset(new EffectsList());
    g_effectsList = effectsList_.get();
    if (effectsList_ && !effectsList_->initialize(bottomRow)) {
        return false;
    }
    if (effectsList_) {
        effectsList_->setCallback([this](int effectIndex) {
            if (g_ledManager) {
                g_ledManager->setAnimationEnabled(true);
                g_ledManager->setCurrentAnimation(static_cast<AnimationType>(effectIndex));
            }

            // Animation is now active
            effectsList_->setActiveState(true);

            updateWebUi();
        });
    }

    return true;
}

void ColourTab::applyCurrentColor() {
    if (colourWheel_) {
        uint8_t r, g, b;
        colourWheel_->getColorRGB(r, g, b);

        if (g_ledManager) {
            g_ledManager->setAnimationEnabled(false);
            g_ledManager->setWhiteMode(false);
            g_ledManager->fillColor(CRGB(r, g, b));
        }

        if (whiteButton_) {
            whiteButton_->setState(false, false);
        }

        // Deactivate effects dropdown (color is now active, not animation)
        if (effectsList_) {
            effectsList_->setActiveState(false);
        }

        updateWebUi();
    }
}

void ColourTab::logAndUpdateVuState(bool newState) {
    Logger.info("VU Button - State: %s", newState ? "ON" : "OFF");

    if (g_ledManager) {
        g_ledManager->setVuMode(newState);
    }

    updateWebUi();
}

void ColourTab::logAndUpdateWhiteState(bool newState) {
    Logger.info("White Button - State: %s", newState ? "ON" : "OFF");

    if (g_ledManager) {
        g_ledManager->setWhiteMode(newState);
        if (newState) {
            g_ledManager->setAnimationEnabled(false);
            g_ledManager->fillWhite();
        }
    }

    // Deactivate effects dropdown styling when white is on
    if (newState && effectsList_) {
        effectsList_->setActiveState(false);
    }

    updateWebUi();
}

void ColourTab::setVuState(bool newState) {
    logAndUpdateVuState(newState);

    if (vuButton_) {
        vuButton_->setState(newState);
    }
}

void ColourTab::setWhiteState(bool newState) {
    logAndUpdateWhiteState(newState);

    if (whiteButton_) {
        whiteButton_->setState(newState);
    }
}

void ColourTab::setAnimationState(bool newState) {
    if (g_ledManager) {
        g_ledManager->setAnimationEnabled(newState);
        if (newState) {
            g_ledManager->setWhiteMode(false);
        }
    }

    if (newState) {
        if (whiteButton_) {
            whiteButton_->setState(false, false);
        }
    } else {
        applyCurrentColor();
    }
}

void ColourTab::setAnimation(int animation) {
    if (effectsList_) {
        effectsList_->setSelectedEffect(animation, false);
        effectsList_->setActiveState(true);  // Highlight as active
    }

    if (whiteButton_) {
        whiteButton_->setState(false, false);
    }

    if (g_ledManager) {
        g_ledManager->setCurrentAnimation(static_cast<AnimationType>(animation));
        g_ledManager->setAnimationEnabled(true);
        g_ledManager->setWhiteMode(false);
    }
}

void ColourTab::syncWithLed() {
    if (!g_ledManager) return;

    Logger.info("Syncing UI with LED state...");

    // Sync brightness slider
    if (brightnessSlider_) {
        brightnessSlider_->setBrightness(g_ledManager->getBrightness(), false, false);
    }

    // Sync VU button
    if (vuButton_) {
        vuButton_->setState(g_ledManager->isVuModeEnabled());
    }

    // Sync white button
    if (whiteButton_) {
        whiteButton_->setState(g_ledManager->isWhiteModeEnabled(), false);
    }

    // Sync animation/effects dropdown
    if (g_ledManager->isAnimationEnabled()) {
        if (effectsList_) {
            effectsList_->setSelectedEffect(static_cast<int>(g_ledManager->getCurrentAnimation()), false);
            effectsList_->setActiveState(true);
        }
    } else {
        if (effectsList_) {
            effectsList_->setActiveState(false);
        }
    }

    // Sync color wheel display with saved solid color (don't apply to LEDs, just update UI)
    if (colourWheel_) {
        CRGB color = g_ledManager->getSolidColor();
        colourWheel_->setColor(color.r, color.g, color.b, false);  // false = UI only, no LED update
    }

    // Notify web UI of current state
    updateWebUi();

    Logger.info("UI sync complete: bright=%d, vu=%d, white=%d, anim=%d",
                g_ledManager->getBrightness(),
                g_ledManager->isVuModeEnabled(),
                g_ledManager->isWhiteModeEnabled(),
                g_ledManager->isAnimationEnabled());
}
