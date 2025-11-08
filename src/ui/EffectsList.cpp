#include "EffectsList.h"
#include "modular-ui.h"
#include "ui.h"
#include "WhiteButton.h"

EffectsList::EffectsList()
    : roller_(nullptr)
    , callback_(nullptr)
    , initialized_(false)
{
}

EffectsList::~EffectsList() {
    cleanup();
}

EffectsList::EffectsList(EffectsList&& other) noexcept
    : roller_(other.roller_)
    , callback_(std::move(other.callback_))
    , initialized_(other.initialized_)
{
    // Reset the moved-from object
    other.roller_ = nullptr;
    other.callback_ = nullptr;
    other.initialized_ = false;
}

EffectsList& EffectsList::operator=(EffectsList&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();
        
        // Move resources from other
        roller_ = other.roller_;
        callback_ = std::move(other.callback_);
        initialized_ = other.initialized_;
        
        // Reset the moved-from object
        other.roller_ = nullptr;
        other.callback_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool EffectsList::initialize(lv_obj_t* parent, int visibleRows) {
    if (initialized_) {
        return true; // Already initialized
    }
    
    if (!parent) {
        return false; // Invalid parent
    }
    
    try {
        // Create the roller
        roller_ = lv_roller_create(parent);
        if (!roller_) {
            return false;
        }

        // Build options string
        String options = buildOptionsString();
        lv_roller_set_options(roller_, options.c_str(), LV_ROLLER_MODE_INFINITE);

        // Set size and position
        lv_roller_set_visible_row_count(roller_, visibleRows);
        lv_obj_set_width(roller_, LV_PCT(90));
        lv_obj_set_height(roller_, LV_PCT(80));
        lv_obj_center(roller_);

        // Apply styling
        static lv_style_t style_main;
        static lv_style_t style_sel;
        static bool styles_initialized = false;
        
        if (!styles_initialized) {
            // Style the main roller background
            lv_style_init(&style_main);
            lv_style_set_text_font(&style_main, &lv_font_montserrat_14);
            lv_style_set_bg_color(&style_main, lv_color_hex(UI_COLOR_SURFACE));
            lv_style_set_bg_grad_color(&style_main, lv_color_hex(UI_COLOR_SURFACE_LIGHT));
            lv_style_set_bg_grad_dir(&style_main, LV_GRAD_DIR_VER);
            lv_style_set_border_color(&style_main, lv_color_hex(UI_COLOR_PRIMARY));
            lv_style_set_border_width(&style_main, 2);
            lv_style_set_radius(&style_main, 12);
            lv_style_set_text_color(&style_main, lv_color_hex(UI_COLOR_TEXT));

            // Style the selected item
            lv_style_init(&style_sel);
            lv_style_set_text_font(&style_sel, &lv_font_montserrat_22);
            lv_style_set_bg_color(&style_sel, lv_color_hex(UI_COLOR_PRIMARY));
            lv_style_set_bg_opa(&style_sel, LV_OPA_30);
            lv_style_set_text_color(&style_sel, lv_color_hex(UI_COLOR_PRIMARY));
            
            styles_initialized = true;
        }

        lv_obj_add_style(roller_, &style_main, LV_PART_MAIN);
        lv_obj_add_style(roller_, &style_sel, LV_PART_SELECTED);

        // Set user data and event callback
        lv_obj_set_user_data(roller_, this);
        lv_obj_add_event_cb(roller_, eventHandler, LV_EVENT_VALUE_CHANGED, nullptr);
        
        initialized_ = true;
        return true;
        
    } catch (...) {
        cleanup(); // Clean up on any exception
        return false;
    }
}

void EffectsList::setCallback(EffectChangeCallback callback) {
    callback_ = callback;
}

void EffectsList::setSelectedEffect(int effectIndex, bool animate) {
    if (!initialized_ || !roller_) {
        return;
    }
    
    lv_roller_set_selected(roller_, effectIndex, animate ? LV_ANIM_ON : LV_ANIM_OFF);
}

int EffectsList::getSelectedEffect() const {
    if (!initialized_ || !roller_) {
        return 0;
    }
    
    return (int)lv_roller_get_selected(roller_);
}

void EffectsList::eventHandler(lv_event_t* event) {
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    EffectsList* instance = static_cast<EffectsList*>(lv_obj_get_user_data(target));
    
    if (instance && lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        instance->handleEffectChange();
    }
}

void EffectsList::handleEffectChange() {
    if (!initialized_ || !roller_) {
        return;
    }
    
    int selectedEffect = getSelectedEffect();
    
    // Update global state
    showAnimation = true;
    currentAnimation = static_cast<animationOptions>(selectedEffect);
    
    // Clear white button state (prevent recursion)
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
    if (roller_) {
        lv_obj_del(roller_);
        roller_ = nullptr;
    }
    
    callback_ = nullptr;
    initialized_ = false;
}

String EffectsList::buildOptionsString() {
    String options;
    
    for (int i = LEDManager::RAINBOW; i <= LEDManager::SOUNDRIPPLE; i++) {
        options.concat(LEDManager::getAnimationDescription(static_cast<LEDManager::AnimationType>(i)));
        options.concat("\n");
    }
    
    return options;
}