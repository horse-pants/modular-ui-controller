#include "ui/ColourTab.h"
#include "modular-ui.h"
#include "led/LEDManager.h"
#include "ui/EffectGrid.h"
#include "ui/UIManager.h"
#include "ui/ui.h"
#include <Logger.h>

namespace {

// The wheel, minus the room its knob needs to overhang at full saturation.
constexpr int WHEEL_SIZE = 196;
constexpr int WHEEL_OVERHANG = 14;

// Deferred one LVGL cycle so the tabview isn't mutated from inside a child's
// event dispatch. The flick-back itself is fixed in showEffectsTab() — see the
// comment there; it is the scroll ANIMATION that causes it, not the ordering.
void switchToEffectsTab(void* /*user_data*/) {
    if (g_uiManager) {
        g_uiManager->showEffectsTab();
    }
}

void effectBarClicked(lv_event_t* /*event*/) {
    // The bar reports the running effect; the picker is a tab away.
    lv_async_call(switchToEffectsTab, nullptr);
}

}  // namespace

ColourTab::~ColourTab() {
    // Clear the widget globals before the widgets are destroyed (member
    // unique_ptrs run after this body), so nothing dangles during teardown.
    g_brightnessSlider = nullptr;
    g_colourWheel = nullptr;
    g_whiteButton = nullptr;
    g_vuButton = nullptr;
    g_colourTab = nullptr;
}

bool ColourTab::build(lv_obj_t* parent) {
    g_colourTab = this;

    // ==========================================================================
    // COLOUR tab — one column: wheel, brightness, White/VU, effect bar.
    // The wheel takes the slack, so the layout adapts to whatever height the tab
    // page actually has rather than depending on a hand-totalled column.
    // ==========================================================================
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, UI_PADDING_LARGE, 0);
    lv_obj_set_style_pad_row(parent, UI_SPACING_NORMAL, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // --- Colour wheel, centred and absorbing the spare height ---
    lv_obj_t* wheelWrap = lv_obj_create(parent);
    lv_obj_set_width(wheelWrap, LV_PCT(100));
    lv_obj_set_flex_grow(wheelWrap, 1);
    lv_obj_set_flex_flow(wheelWrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wheelWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(wheelWrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wheelWrap, 0, 0);
    // The knob hangs over the wheel's square at full saturation.
    lv_obj_set_style_pad_all(wheelWrap, WHEEL_OVERHANG, 0);
    lv_obj_remove_flag(wheelWrap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wheelWrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    colourWheel_.reset(new ColourWheel());
    if (!colourWheel_->initialize(wheelWrap, WHEEL_SIZE, UI_COLOR_SURFACE)) {
        // The wheel needs a contiguous PSRAM block and a fragmented heap can
        // refuse it. Losing colour picking is survivable; losing the whole tab
        // isn't — so carry on without it.
        Logger.warning("Colour wheel unavailable - continuing without it");
        colourWheel_.reset();
    }
    g_colourWheel = colourWheel_.get();
    if (colourWheel_) {
        // The wheel reports the picked colour, but applyCurrentColor() re-reads it
        // from the wheel — so the r/g/b params are intentionally unnamed (unused).
        colourWheel_->setCallback([this](uint8_t, uint8_t, uint8_t) {
            this->applyCurrentColor();
        });
    }

    // --- Brightness (horizontal, with a live percentage) ---
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

        const int initialBrightness = g_ledManager ? g_ledManager->getBrightness() : 128;
        if (!brightnessSlider_->initialize(parent, initialBrightness)) {
            return false;
        }
    }

    // --- White / VU: two equal pills, both a comfortable thumb tall ---
    lv_obj_t* toggleRow = lv_obj_create(parent);
    lv_obj_set_size(toggleRow, LV_PCT(100), UI_BTN_HEIGHT_LARGE);
    lv_obj_set_flex_flow(toggleRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(toggleRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(toggleRow, 0, 0);
    lv_obj_set_style_pad_all(toggleRow, 0, 0);
    lv_obj_set_style_pad_column(toggleRow, UI_SPACING_NORMAL, 0);
    lv_obj_remove_flag(toggleRow, LV_OBJ_FLAG_SCROLLABLE);

    whiteButton_.reset(new WhiteButton());
    g_whiteButton = whiteButton_.get();
    if (whiteButton_ && !whiteButton_->initialize(toggleRow)) {
        return false;
    }

    vuButton_.reset(new VuButton());
    g_vuButton = vuButton_.get();
    if (vuButton_ && !vuButton_->initialize(toggleRow)) {
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

    buildEffectBar(parent);

    return true;
}

void ColourTab::buildEffectBar(lv_obj_t* parent) {
    effectBar_ = lv_button_create(parent);
    lv_obj_set_size(effectBar_, LV_PCT(100), UI_BTN_HEIGHT);
    lv_obj_set_flex_flow(effectBar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(effectBar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(effectBar_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
    lv_obj_set_style_radius(effectBar_, UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_pad_hor(effectBar_, UI_PADDING_MEDIUM, 0);
    lv_obj_set_style_pad_ver(effectBar_, 0, 0);
    lv_obj_set_style_pad_column(effectBar_, UI_SPACING_NORMAL, 0);
    lv_obj_set_style_border_color(effectBar_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(effectBar_, UI_BORDER_THIN, 0);
    // Running: the cyan border that marks "active" everywhere else in this UI.
    // The background MUST be set for LV_STATE_CHECKED too — leave it out and
    // LVGL's default theme fills the state with its own secondary colour, which
    // is red.
    lv_obj_set_style_bg_color(effectBar_, lv_color_hex(UI_COLOR_SURFACE_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(effectBar_, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(effectBar_, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
    lv_obj_set_style_border_width(effectBar_, UI_BORDER_NORMAL, LV_STATE_CHECKED);
    lv_obj_remove_flag(effectBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(effectBar_, effectBarClicked, LV_EVENT_CLICKED, nullptr);

    effectSwatch_ = lv_obj_create(effectBar_);
    lv_obj_set_size(effectSwatch_, 22, 22);
    lv_obj_set_style_radius(effectSwatch_, UI_RADIUS_SMALL, 0);
    lv_obj_set_style_border_width(effectSwatch_, 0, 0);
    lv_obj_set_style_pad_all(effectSwatch_, 0, 0);
    lv_obj_remove_flag(effectSwatch_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(effectSwatch_, LV_OBJ_FLAG_SCROLLABLE);

    effectName_ = lv_label_create(effectBar_);
    lv_obj_set_flex_grow(effectName_, 1);
    lv_obj_set_style_text_color(effectName_, lv_color_hex(UI_COLOR_TEXT), 0);

    lv_obj_t* chevron = lv_label_create(effectBar_);
    lv_label_set_text(chevron, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(chevron, lv_color_hex(UI_COLOR_PRIMARY), 0);

    showEffect(EffectGrid::OFF_INDEX);
}

void ColourTab::showEffect(int animation) {
    if (!effectBar_) return;

    const bool on = (animation != EffectGrid::OFF_INDEX);

    lv_label_set_text(effectName_,
                      on ? animationDescription(static_cast<AnimationType>(animation))
                         : "No effect");

    lv_obj_set_style_bg_color(
        effectSwatch_,
        lv_color_hex(on ? EffectGrid::signatureColor(animation) : UI_COLOR_SURFACE_DARK), 0);

    if (on) lv_obj_add_state(effectBar_, LV_STATE_CHECKED);
    else    lv_obj_remove_state(effectBar_, LV_STATE_CHECKED);
}

void ColourTab::applyCurrentColor() {
    if (!colourWheel_) return;

    uint8_t r, g, b;
    colourWheel_->getColorRGB(r, g, b);

    if (g_ledManager) {
        // setSolidColor() also clears animation + white mode, fills the strips
        // and persists the colour.
        g_ledManager->setSolidColor(CRGB(r, g, b));
    }

    if (whiteButton_) {
        whiteButton_->setState(false, false);
    }

    // Picking a colour ends any running animation.
    showEffect(EffectGrid::OFF_INDEX);
    if (g_effectGrid) {
        g_effectGrid->setSelected(EffectGrid::OFF_INDEX);
    }

    updateWebUi();
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

    if (newState) {
        showEffect(EffectGrid::OFF_INDEX);
        if (g_effectGrid) {
            g_effectGrid->setSelected(EffectGrid::OFF_INDEX);
        }
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
        showEffect(EffectGrid::OFF_INDEX);
        if (g_effectGrid) {
            g_effectGrid->setSelected(EffectGrid::OFF_INDEX);
        }
        applyCurrentColor();
    }
}

void ColourTab::setAnimation(int animation) {
    if (g_effectGrid) {
        g_effectGrid->setSelected(animation);
    }
    showEffect(animation);

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

    if (brightnessSlider_) {
        brightnessSlider_->setBrightness(g_ledManager->getBrightness(), false, false);
    }

    if (vuButton_) {
        vuButton_->setState(g_ledManager->isVuModeEnabled());
    }

    if (whiteButton_) {
        whiteButton_->setState(g_ledManager->isWhiteModeEnabled(), false);
    }

    showEffect(g_ledManager->isAnimationEnabled()
                   ? static_cast<int>(g_ledManager->getCurrentAnimation())
                   : EffectGrid::OFF_INDEX);

    // Sync the wheel with the saved solid colour (UI only, no LED update).
    if (colourWheel_) {
        CRGB color = g_ledManager->getSolidColor();
        colourWheel_->setColor(color.r, color.g, color.b, false);
    }

    updateWebUi();

    Logger.info("UI sync complete: bright=%d, vu=%d, white=%d, anim=%d",
                g_ledManager->getBrightness(),
                g_ledManager->isVuModeEnabled(),
                g_ledManager->isWhiteModeEnabled(),
                g_ledManager->isAnimationEnabled());
}
