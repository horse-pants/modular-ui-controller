
#include "ui/BrightnessSlider.h"
#include "ui/ui.h"
#include "modular-ui.h"

// Static member definitions
lv_style_t BrightnessSlider::indicatorStyle_;
lv_style_t BrightnessSlider::knobStyle_;
bool BrightnessSlider::stylesInitialized_ = false;

BrightnessSlider::BrightnessSlider(int maxBrightness)
    : card_(nullptr)
    , slider_(nullptr)
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
    : card_(other.card_)
    , slider_(other.slider_)
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
        other.card_ = nullptr;
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

    // === INDICATOR STYLE - Bright cyan fill ===
    lv_style_init(&indicatorStyle_);
    lv_style_set_bg_opa(&indicatorStyle_, LV_OPA_COVER);
    lv_style_set_bg_color(&indicatorStyle_, lv_color_hex(UI_COLOR_PRIMARY));
    lv_style_set_bg_grad_color(&indicatorStyle_, lv_color_hex(UI_COLOR_PRIMARY_DARK));
    lv_style_set_bg_grad_dir(&indicatorStyle_, LV_GRAD_DIR_HOR);
    lv_style_set_radius(&indicatorStyle_, LV_RADIUS_CIRCLE);

    // === KNOB STYLE - High contrast, visible knob ===
    lv_style_init(&knobStyle_);
    lv_style_set_bg_color(&knobStyle_, lv_color_hex(UI_COLOR_WHITE));
    lv_style_set_bg_grad_color(&knobStyle_, lv_color_hex(UI_COLOR_TEXT));
    lv_style_set_bg_grad_dir(&knobStyle_, LV_GRAD_DIR_VER);
    lv_style_set_radius(&knobStyle_, LV_RADIUS_CIRCLE);
    lv_style_set_width(&knobStyle_, 28);
    lv_style_set_height(&knobStyle_, 28);
    lv_style_set_border_color(&knobStyle_, lv_color_hex(UI_COLOR_PRIMARY));
    lv_style_set_border_width(&knobStyle_, UI_BORDER_NORMAL);

    stylesInitialized_ = true;
}

void BrightnessSlider::createLabel() {
    // A card, so the caption, the percentage and the track read as one control.
    card_ = lv_obj_create(parentTab_);
    lv_obj_set_size(card_, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(card_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
    lv_obj_set_style_border_color(card_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card_, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(card_, UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_pad_all(card_, UI_PADDING_MEDIUM, 0);
    lv_obj_set_style_pad_row(card_, UI_PADDING_SMALL, 0);
    lv_obj_remove_flag(card_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = lv_obj_create(card_);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* caption = lv_label_create(header);
    lv_label_set_text(caption, "BRIGHTNESS");
    lv_obj_set_style_text_color(caption, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);

    label_ = lv_label_create(header);
    lv_obj_set_style_text_color(label_, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_label_set_text(label_, "0%");
}

void BrightnessSlider::updateValueLabel() {
    if (!label_ || maxBrightness_ <= 0) return;
    // Rounded, so dragging to either end reads a clean 0% / 100%.
    const int pct = (currentBrightness_ * 100 + maxBrightness_ / 2) / maxBrightness_;
    lv_label_set_text_fmt(label_, "%d%%", pct);
}

void BrightnessSlider::createSlider() {
    slider_ = lv_slider_create(card_);
    if (slider_) {
        // Horizontal fader across the card.
        lv_obj_set_width(slider_, LV_PCT(100));
        lv_obj_set_height(slider_, 22);
        lv_slider_set_range(slider_, 0, maxBrightness_);

        // The round knob overhangs the track ends; this keeps it unclipped.
        lv_obj_set_style_pad_hor(slider_, 4, 0);

        // Taller touch area than the track, so a slightly high or low thumb
        // still lands on it.
        lv_obj_set_ext_click_area(slider_, 14);

        // Set user data to this instance for event handling
        lv_obj_set_user_data(slider_, this);
        lv_obj_add_event_cb(slider_, eventHandlerWrapper, LV_EVENT_VALUE_CHANGED, nullptr);
    }
}

void BrightnessSlider::applySliderStyling() {
    if (!slider_) return;

    // Initialize styles if not done yet
    initializeStyles();

    // === TRACK STYLE - Metallic recessed look ===
    lv_obj_set_style_bg_color(slider_, lv_color_hex(UI_COLOR_SURFACE_DARK), 0);
    lv_obj_set_style_bg_grad_color(slider_, lv_color_hex(UI_COLOR_TRACK), 0);
    lv_obj_set_style_bg_grad_dir(slider_, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(slider_, UI_RADIUS_SMALL, 0);
    lv_obj_set_style_border_color(slider_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(slider_, UI_BORDER_THIN, 0);
    lv_obj_set_style_border_side(slider_, LV_BORDER_SIDE_FULL, 0);

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
    updateValueLabel();
    
    // Call the callback if set
    if (callback_) {
        callback_(currentBrightness_);
    }
}

bool BrightnessSlider::initialize(lv_obj_t* parent, int initialBrightness) {
    if (initialized_ || !parent) {
        return false;
    }

    parentTab_ = parent;

    try {
        // Create UI elements (they add themselves to parentTab_)
        createLabel();
        createSlider();

        // Check if creation was successful
        if (!slider_) {
            cleanup();
            return false;
        }

        // No positioning - parent's layout handles arrangement

        // Apply styling
        applySliderStyling();

        // Mark as initialized before setting initial value
        initialized_ = true;

        // Set initial brightness
        setBrightness(initialBrightness, false);

        return true;

    } catch (...) {
        initialized_ = false;
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
    updateValueLabel();
    
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
    // Deleting the card takes the header, the caption, the readout and the
    // track with it.
    if (card_) {
        lv_obj_del(card_);
        card_ = nullptr;
    }
    slider_ = nullptr;
    label_ = nullptr;
    
    parentTab_ = nullptr;
    initialized_ = false;
    currentBrightness_ = 0;
    callback_ = nullptr;
}