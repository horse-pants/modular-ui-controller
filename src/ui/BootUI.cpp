// ============================================================================
// OLD BootUI.cpp - NO LONGER USED
// This file has been replaced by library/src/WiFiSetupBootUI.cpp
// All code commented out to prevent compilation
// ============================================================================

/*
#include "BootUI.h"
#include "ui.h"
#include <lvgl.h>

 BootUI::BootUI()
    : textArea_(nullptr)
    , titleLabel_(nullptr)
    , initialized_(false)
{
}

BootUI::~BootUI() {
    cleanup();
}

BootUI::BootUI(BootUI&& other) noexcept
    : textArea_(other.textArea_)
    , titleLabel_(other.titleLabel_)
    , initialized_(other.initialized_)
{
    // Reset the moved-from object
    other.textArea_ = nullptr;
    other.titleLabel_ = nullptr;
    other.initialized_ = false;
}

BootUI& BootUI::operator=(BootUI&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();

        // Move resources from other
        textArea_ = other.textArea_;
        titleLabel_ = other.titleLabel_;
        initialized_ = other.initialized_;

        // Reset the moved-from object
        other.textArea_ = nullptr;
        other.titleLabel_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

void BootUI::applyScreenTheme() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_grad_color(lv_scr_act(), lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_grad_dir(lv_scr_act(), LV_GRAD_DIR_VER, 0);
}

void BootUI::createTitle() {
    titleLabel_ = lv_label_create(lv_scr_act());
    if (titleLabel_) {
        lv_label_set_text(titleLabel_, "MODULAR UI CONTROLLER");
        lv_obj_set_style_text_color(titleLabel_, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_text_font(titleLabel_, &lv_font_montserrat_22, 0);
        lv_obj_align(titleLabel_, LV_ALIGN_TOP_MID, 0, 30);
    }
}

void BootUI::createTextArea() {
    textArea_ = lv_textarea_create(lv_scr_act());
    if (textArea_) {
        lv_textarea_set_one_line(textArea_, false);
        lv_obj_align(textArea_, LV_ALIGN_CENTER, 0, 40);
        lv_obj_set_size(textArea_, LV_PCT(90), LV_PCT(60));

        // Apply synth theme styling with advanced gradient border (LVGL 9.x)
        lv_obj_set_style_bg_color(textArea_, lv_color_hex(UI_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_grad_color(textArea_, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
        lv_obj_set_style_bg_grad_dir(textArea_, LV_GRAD_DIR_VER, 0);

        lv_obj_set_style_border_width(textArea_, 3, 0);
        lv_obj_set_style_border_color(textArea_, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_border_opa(textArea_, LV_OPA_COVER, 0);

        lv_obj_set_style_radius(textArea_, 12, 0);
        lv_obj_set_style_text_color(textArea_, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_pad_all(textArea_, 20, 0);
    }
}

bool BootUI::initialize() {
    if (initialized_) {
        return true; // Already initialized
    }

    try {
        // Apply theme to screen background
        applyScreenTheme();

        // Create UI elements
        createTitle();
        createTextArea();

        // Check if creation was successful
        if (textArea_ && titleLabel_) {
            initialized_ = true;
            return true;
        } else {
            cleanup(); // Clean up partial initialization
            return false;
        }
    } catch (...) {
        cleanup(); // Clean up on any exception
        return false;
    }
}

void BootUI::addText(const char* text) {
    if (!initialized_ || !textArea_ || !text) {
        return; // Silently ignore if not initialized or invalid input
    }

    lv_textarea_add_text(textArea_, text);
}

void BootUI::clearText() {
    if (!initialized_ || !textArea_) {
        return;
    }

    lv_textarea_set_text(textArea_, "");
}

void BootUI::cleanup() {
    if (textArea_) {
        lv_obj_del(textArea_);
        textArea_ = nullptr;
    }

    if (titleLabel_) {
        lv_obj_del(titleLabel_);
        titleLabel_ = nullptr;
    }

    initialized_ = false;
}
*/

// ============================================================================
// END OF OLD BootUI.cpp
// ============================================================================
