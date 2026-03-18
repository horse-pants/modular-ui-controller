#include "VuButton.h"
#include "modular-ui.h"
#include "ui.h"
#include "UIManager.h"

VuButton::VuButton()
    : button_(nullptr)
    , callback_(nullptr)
    , initialized_(false)
    , currentState_(false)
{
}

VuButton::~VuButton() {
    cleanup();
}

VuButton::VuButton(VuButton&& other) noexcept
    : button_(other.button_)
    , callback_(std::move(other.callback_))
    , initialized_(other.initialized_)
    , currentState_(other.currentState_)
{
    // Reset the moved-from object
    other.button_ = nullptr;
    other.callback_ = nullptr;
    other.initialized_ = false;
    other.currentState_ = false;
}

VuButton& VuButton::operator=(VuButton&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();
        
        // Move resources from other
        button_ = other.button_;
        callback_ = std::move(other.callback_);
        initialized_ = other.initialized_;
        currentState_ = other.currentState_;
        
        // Reset the moved-from object
        other.button_ = nullptr;
        other.callback_ = nullptr;
        other.initialized_ = false;
        other.currentState_ = false;
    }
    return *this;
}

bool VuButton::initialize(lv_obj_t* parent) {
    if (initialized_) {
        return true;
    }

    if (!parent) {
        return false;
    }

    try {
        // Create the button
        button_ = lv_btn_create(parent);
        if (!button_) {
            return false;
        }

        // Size only - no positioning (parent layout handles it)
        lv_obj_set_size(button_, UI_BTN_COMPACT_WIDTH, UI_BTN_HEIGHT);
        lv_obj_add_flag(button_, LV_OBJ_FLAG_CHECKABLE);

        // Apply styling
        applyButtonStyling();

        // Create label
        lv_obj_t* label = lv_label_create(button_);
        lv_label_set_text(label, "VU");
        lv_obj_center(label);

        // Set user data and event callback
        lv_obj_set_user_data(button_, this);
        lv_obj_add_event_cb(button_, eventHandler, LV_EVENT_ALL, nullptr);

        initialized_ = true;
        currentState_ = false;
        return true;

    } catch (...) {
        cleanup();
        return false;
    }
}

void VuButton::setCallback(StateChangeCallback callback) {
    callback_ = callback;
}

void VuButton::setState(bool isActive) {
    if (!initialized_ || !button_) {
        return;
    }
    
    currentState_ = isActive;
    vu = isActive; // Update global state
    
    if (isActive) {
        lv_obj_add_state(button_, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(button_, LV_STATE_CHECKED);
    }
}

bool VuButton::getState() const {
    return currentState_;
}

void VuButton::eventHandler(lv_event_t* event) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    VuButton* instance = static_cast<VuButton*>(lv_obj_get_user_data(target));
    
    if (instance && lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        instance->handleStateChange();
    }
}

void VuButton::handleStateChange() {
    if (!initialized_ || !button_) {
        return;
    }

    // Toggle state
    currentState_ = !currentState_;

    // Call UIManager's simple helper
    extern UIManager* g_uiManager;
    if (g_uiManager) {
        g_uiManager->logAndUpdateVuState(currentState_);
    } else {
        // Fallback if UIManager not available
        vu = currentState_;
        updateWebUi();
    }

    // Call user callback if set
    if (callback_) {
        callback_(currentState_);
    }
}

void VuButton::cleanup() {
    if (button_) {
        lv_obj_del(button_);
        button_ = nullptr;
    }
    
    callback_ = nullptr;
    initialized_ = false;
    currentState_ = false;
}

void VuButton::applyButtonStyling() {
    if (!button_) {
        return;
    }

    // === INACTIVE STATE - Dark hardware button ===
    lv_obj_set_style_bg_color(button_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
    lv_obj_set_style_bg_grad_color(button_, lv_color_hex(UI_COLOR_SURFACE_DARK), 0);
    lv_obj_set_style_bg_grad_dir(button_, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(button_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(button_, UI_BORDER_NORMAL, 0);
    lv_obj_set_style_radius(button_, UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_text_color(button_, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);

    // === ACTIVE STATE - Bright cyan, thick bright border (simulates glow) ===
    lv_obj_set_style_bg_color(button_, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
    lv_obj_set_style_bg_grad_color(button_, lv_color_hex(UI_COLOR_PRIMARY_DARK), LV_STATE_CHECKED);
    lv_obj_set_style_border_color(button_, lv_color_hex(UI_COLOR_WHITE), LV_STATE_CHECKED);
    lv_obj_set_style_border_width(button_, UI_BORDER_THICK, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button_, lv_color_hex(UI_COLOR_BLACK), LV_STATE_CHECKED);

    // === PRESSED STATE - Feedback ===
    lv_obj_set_style_bg_color(button_, lv_color_hex(UI_COLOR_PRIMARY_DIM), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button_, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button_, lv_color_hex(UI_COLOR_WHITE), LV_STATE_PRESSED);
}