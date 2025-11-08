
#include "BrightnessSlider.h"
#include "ui.h"
#include "modular-ui.h"

// Static member definitions
lv_style_t BrightnessSlider::indicatorStyle_;
lv_style_t BrightnessSlider::knobStyle_;
bool BrightnessSlider::stylesInitialized_ = false;

BrightnessSlider::BrightnessSlider(int maxBrightness)
    : slider_(nullptr)
    , label_(nullptr)
    , parentTab_(nullptr)
    , initialized_(false)
    , currentBrightness_(0)
    , maxBrightness_(maxBrightness)
    , callback_(nullptr)
{
}

BrightnessSlider::~BrightnessSlider() {
    cleanup();
}

BrightnessSlider::BrightnessSlider(BrightnessSlider&& other) noexcept
    : slider_(other.slider_)
    , label_(other.label_)
    , parentTab_(other.parentTab_)
    , initialized_(other.initialized_)
    , currentBrightness_(other.currentBrightness_)
    , maxBrightness_(other.maxBrightness_)
    , callback_(std::move(other.callback_))
{
    // Reset the moved-from object
    other.slider_ = nullptr;
    other.label_ = nullptr;
    other.parentTab_ = nullptr;
    other.initialized_ = false;
    other.currentBrightness_ = 0;
    other.callback_ = nullptr;
}

BrightnessSlider& BrightnessSlider::operator=(BrightnessSlider&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();
        
        // Move resources from other
        slider_ = other.slider_;
        label_ = other.label_;
        parentTab_ = other.parentTab_;
        initialized_ = other.initialized_;
        currentBrightness_ = other.currentBrightness_;
        maxBrightness_ = other.maxBrightness_;
        callback_ = std::move(other.callback_);
        
        // Reset the moved-from object
        other.slider_ = nullptr;
        other.label_ = nullptr;
        other.parentTab_ = nullptr;
        other.initialized_ = false;
        other.currentBrightness_ = 0;
        other.callback_ = nullptr;
    }
    return *this;
}

void BrightnessSlider::initializeStyles() {
    if (stylesInitialized_) {
        return;
    }
    
    // Initialize indicator style
    lv_style_init(&indicatorStyle_);
    lv_style_set_bg_opa(&indicatorStyle_, LV_OPA_COVER);
    lv_style_set_bg_color(&indicatorStyle_, lv_color_hex(UI_COLOR_PRIMARY));
    lv_style_set_bg_grad_color(&indicatorStyle_, lv_color_hex(UI_COLOR_PRIMARY_DARK));
    lv_style_set_bg_grad_dir(&indicatorStyle_, LV_GRAD_DIR_HOR);
    lv_style_set_radius(&indicatorStyle_, 3);
    
    // Initialize knob style
    lv_style_init(&knobStyle_);
    lv_style_set_bg_color(&knobStyle_, lv_color_hex(UI_COLOR_PRIMARY));
    lv_style_set_bg_grad_color(&knobStyle_, lv_color_hex(UI_COLOR_PRIMARY_DARK));
    lv_style_set_bg_grad_dir(&knobStyle_, LV_GRAD_DIR_VER);
    lv_style_set_radius(&knobStyle_, 12);
    lv_style_set_width(&knobStyle_, 24);
    lv_style_set_height(&knobStyle_, 24);
    lv_style_set_border_color(&knobStyle_, lv_color_hex(UI_COLOR_PRIMARY));
    lv_style_set_border_width(&knobStyle_, 2);
    
    stylesInitialized_ = true;
}

void BrightnessSlider::createLabel() {
    label_ = lv_label_create(parentTab_);
    if (label_) {
        lv_label_set_text(label_, "BRIGHTNESS");
        lv_obj_set_style_text_color(label_, lv_color_hex(UI_COLOR_PRIMARY), 0);
    }
}

void BrightnessSlider::createSlider() {
    slider_ = lv_slider_create(parentTab_);
    if (slider_) {
        lv_obj_set_width(slider_, 120);
        lv_obj_set_height(slider_, 6);
        lv_slider_set_range(slider_, 0, maxBrightness_);
        
        // Set user data to this instance for event handling
        lv_obj_set_user_data(slider_, this);
        lv_obj_add_event_cb(slider_, eventHandlerWrapper, LV_EVENT_VALUE_CHANGED, nullptr);
    }
}

void BrightnessSlider::applySliderStyling() {
    if (!slider_) return;
    
    // Initialize styles if not done yet
    initializeStyles();
    
    // Style the slider background (track)
    lv_obj_set_style_bg_color(slider_, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_grad_color(slider_, lv_color_hex(0x555555), 0);
    lv_obj_set_style_bg_grad_dir(slider_, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(slider_, 3, 0);
    
    // Apply indicator and knob styles
    lv_obj_add_style(slider_, &indicatorStyle_, LV_PART_INDICATOR);
    lv_obj_add_style(slider_, &knobStyle_, LV_PART_KNOB);
}

void BrightnessSlider::eventHandlerWrapper(lv_event_t* e) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    BrightnessSlider* instance = static_cast<BrightnessSlider*>(lv_obj_get_user_data(slider));
    
    if (instance) {
        instance->handleSliderEvent(e);
    }
}

void BrightnessSlider::handleSliderEvent(lv_event_t* e) {
    if (!slider_) return;
    
    int sliderValue = static_cast<int>(lv_slider_get_value(slider_));
    currentBrightness_ = sliderValue;
    
    // Call the callback if set
    if (callback_) {
        callback_(currentBrightness_);
    }
}

bool BrightnessSlider::initialize(lv_obj_t* parentTab, int x, int y, int initialBrightness) {
    if (initialized_ || !parentTab) {
        return false;
    }
    
    parentTab_ = parentTab;
    
    try {
        // Create UI elements
        createLabel();
        createSlider();
        
        // Check if creation was successful
        if (!slider_ || !label_) {
            cleanup();
            return false;
        }
        
        // Position elements
        lv_obj_align(label_, LV_ALIGN_TOP_LEFT, x, y);
        lv_obj_align(slider_, LV_ALIGN_TOP_LEFT, x, y + 20);
        
        // Apply styling
        applySliderStyling();
        
        // Mark as initialized before setting initial value
        initialized_ = true;
        
        // Set initial brightness
        setBrightness(initialBrightness, false);
        
        return true;
        
    } catch (...) {
        initialized_ = false;  // Reset flag before cleanup
        cleanup();
        return false;
    }
}

void BrightnessSlider::setBrightness(int brightness, bool animate, bool triggerCallback) {
    if (!initialized_ || !slider_) {
        return;
    }
    
    // Clamp brightness to valid range
    brightness = (brightness < 0) ? 0 : (brightness > maxBrightness_) ? maxBrightness_ : brightness;
    
    currentBrightness_ = brightness;
    lv_slider_set_value(slider_, brightness, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    
    // Trigger callback if requested (for external updates like web UI)
    if (triggerCallback && callback_) {
        callback_(currentBrightness_);
    }
}

int BrightnessSlider::getSliderValue() const {
    if (!initialized_ || !slider_) {
        return 0;
    }
    return static_cast<int>(lv_slider_get_value(slider_));
}

void BrightnessSlider::syncFromSlider() {
    if (!initialized_ || !slider_) {
        return;
    }
    currentBrightness_ = static_cast<int>(lv_slider_get_value(slider_));
}

void BrightnessSlider::cleanup() {
    if (slider_) {
        lv_obj_del(slider_);
        slider_ = nullptr;
    }
    
    if (label_) {
        lv_obj_del(label_);
        label_ = nullptr;
    }
    
    parentTab_ = nullptr;
    initialized_ = false;
    currentBrightness_ = 0;
    callback_ = nullptr;
}