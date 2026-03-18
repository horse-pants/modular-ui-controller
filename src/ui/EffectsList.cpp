#include "EffectsList.h"
#include "modular-ui.h"
#include "ui.h"
#include "WhiteButton.h"

EffectsList::EffectsList()
    : dropdown_(nullptr)
    , callback_(nullptr)
    , initialized_(false)
    , activeState_(false)
{
}

EffectsList::~EffectsList() {
    cleanup();
}

EffectsList::EffectsList(EffectsList&& other) noexcept
    : dropdown_(other.dropdown_)
    , callback_(std::move(other.callback_))
    , initialized_(other.initialized_)
    , activeState_(other.activeState_)
{
    other.dropdown_ = nullptr;
    other.callback_ = nullptr;
    other.initialized_ = false;
    other.activeState_ = false;
}

EffectsList& EffectsList::operator=(EffectsList&& other) noexcept {
    if (this != &other) {
        cleanup();

        dropdown_ = other.dropdown_;
        callback_ = std::move(other.callback_);
        initialized_ = other.initialized_;
        activeState_ = other.activeState_;

        other.dropdown_ = nullptr;
        other.callback_ = nullptr;
        other.initialized_ = false;
        other.activeState_ = false;
    }
    return *this;
}

bool EffectsList::initialize(lv_obj_t* parent) {
    if (initialized_) {
        return true;
    }

    if (!parent) {
        return false;
    }

    try {
        // Create dropdown
        dropdown_ = lv_dropdown_create(parent);
        if (!dropdown_) {
            return false;
        }

        // Build options string and set
        String options = buildOptionsString();
        lv_dropdown_set_options(dropdown_, options.c_str());

        // Size - wide enough for longest effect names
        lv_obj_set_size(dropdown_, 150, UI_BTN_HEIGHT);

        // Dropdown opens upward (drop-up) since button is at bottom
        lv_dropdown_set_dir(dropdown_, LV_DIR_TOP);

        // === Style the main button part ===
        lv_obj_set_style_bg_color(dropdown_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
        lv_obj_set_style_bg_grad_color(dropdown_, lv_color_hex(UI_COLOR_SURFACE_DARK), 0);
        lv_obj_set_style_bg_grad_dir(dropdown_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(dropdown_, lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(dropdown_, UI_BORDER_NORMAL, 0);
        lv_obj_set_style_radius(dropdown_, UI_RADIUS_MEDIUM, 0);
        lv_obj_set_style_text_color(dropdown_, lv_color_hex(UI_COLOR_TEXT), 0);
        lv_obj_set_style_pad_all(dropdown_, UI_PADDING_SMALL, 0);

        // Pressed state
        lv_obj_set_style_bg_color(dropdown_, lv_color_hex(UI_COLOR_SURFACE), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(dropdown_, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_PRESSED);

        // Disable scrolling/drag on the dropdown itself
        lv_obj_clear_flag(dropdown_, LV_OBJ_FLAG_SCROLLABLE);

        // Set user data and event callbacks
        lv_obj_set_user_data(dropdown_, this);
        lv_obj_add_event_cb(dropdown_, eventHandler, LV_EVENT_VALUE_CHANGED, nullptr);
        // Handle dropdown open to disable drag on the list
        lv_obj_add_event_cb(dropdown_, openHandler, LV_EVENT_CLICKED, nullptr);

        initialized_ = true;
        return true;

    } catch (...) {
        cleanup();
        return false;
    }
}

void EffectsList::setCallback(EffectChangeCallback callback) {
    callback_ = callback;
}

void EffectsList::setSelectedEffect(int effectIndex, bool animate) {
    if (!initialized_ || !dropdown_) {
        return;
    }

    lv_dropdown_set_selected(dropdown_, effectIndex);
}

void EffectsList::setActiveState(bool active) {
    if (!initialized_ || !dropdown_) {
        return;
    }

    activeState_ = active;

    if (active) {
        // Active state - bright cyan border, like VU/White buttons when on
        lv_obj_set_style_bg_color(dropdown_, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_bg_grad_color(dropdown_, lv_color_hex(UI_COLOR_PRIMARY_DARK), 0);
        lv_obj_set_style_border_color(dropdown_, lv_color_hex(UI_COLOR_WHITE), 0);
        lv_obj_set_style_border_width(dropdown_, UI_BORDER_THICK, 0);
        lv_obj_set_style_text_color(dropdown_, lv_color_hex(UI_COLOR_BLACK), 0);
    } else {
        // Inactive state - dark hardware button look
        lv_obj_set_style_bg_color(dropdown_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
        lv_obj_set_style_bg_grad_color(dropdown_, lv_color_hex(UI_COLOR_SURFACE_DARK), 0);
        lv_obj_set_style_border_color(dropdown_, lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(dropdown_, UI_BORDER_NORMAL, 0);
        lv_obj_set_style_text_color(dropdown_, lv_color_hex(UI_COLOR_TEXT), 0);
    }
}

int EffectsList::getSelectedEffect() const {
    if (!initialized_ || !dropdown_) {
        return 0;
    }

    return (int)lv_dropdown_get_selected(dropdown_);
}

void EffectsList::eventHandler(lv_event_t* event) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    EffectsList* instance = static_cast<EffectsList*>(lv_obj_get_user_data(target));

    if (instance && lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        instance->handleEffectChange();
    }
}

void EffectsList::openHandler(lv_event_t* event) {
    lv_obj_t* dropdown = static_cast<lv_obj_t*>(lv_event_get_target(event));
    lv_obj_t* list = lv_dropdown_get_list(dropdown);
    if (list) {
        // Style the dropdown list
        lv_obj_set_style_bg_color(list, lv_color_hex(UI_COLOR_SURFACE), 0);
        lv_obj_set_style_border_color(list, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_border_width(list, UI_BORDER_NORMAL, 0);
        lv_obj_set_style_radius(list, UI_RADIUS_MEDIUM, 0);
        lv_obj_set_style_text_color(list, lv_color_hex(UI_COLOR_TEXT), 0);

        // Selected item style
        lv_obj_set_style_bg_color(list, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_SELECTED);
        lv_obj_set_style_text_color(list, lv_color_hex(UI_COLOR_BLACK), LV_PART_SELECTED);

        // Allow vertical scroll only, disable horizontal drag/scroll
        lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(list, LV_DIR_VER);  // Vertical only
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLL_CHAIN);
    }
}

void EffectsList::handleEffectChange() {
    if (!initialized_ || !dropdown_) {
        return;
    }

    int selectedEffect = getSelectedEffect();

    // Update global state
    showAnimation = true;
    currentAnimation = static_cast<animationOptions>(selectedEffect);

    // Clear white button state
    if (g_whiteButton && g_whiteButton->isInitialized()) {
        g_whiteButton->setState(false, false);
    }
    white = false;
    FastLED.clear();

    // Update web UI
    updateWebUi();

    // Call user callback if set
    if (callback_) {
        callback_(selectedEffect);
    }
}

void EffectsList::cleanup() {
    if (dropdown_) {
        lv_obj_del(dropdown_);
        dropdown_ = nullptr;
    }

    callback_ = nullptr;
    initialized_ = false;
    activeState_ = false;
}

String EffectsList::buildOptionsString() {
    String options;

    for (int i = LEDManager::RAINBOW; i <= LEDManager::VU; i++) {
        if (i > LEDManager::RAINBOW) {
            options.concat("\n");
        }
        options.concat(LEDManager::getAnimationDescription(static_cast<LEDManager::AnimationType>(i)));
    }

    return options;
}