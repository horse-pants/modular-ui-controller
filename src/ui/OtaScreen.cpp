#include "ui/OtaScreen.h"
#include "modular-ui.h"

void OtaScreen::show() {
    active_ = true;
    pendingProgress_ = 0;
    changed_ = true;
}

void OtaScreen::updateProgress(uint8_t pct) {
    pendingProgress_ = pct;
    changed_ = true;
}

void OtaScreen::hide() {
    active_ = false;
    changed_ = true;
}

void OtaScreen::apply() {
    if (!changed_) {
        return;
    }
    changed_ = false;

    if (active_ && !screen_) {
        // Create OTA screen
        screen_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(screen_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(screen_, lv_color_hex(UI_COLOR_BACKGROUND), 0);
        lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(screen_, 0, 0);
        lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);

        // Title label
        label_ = lv_label_create(screen_);
        lv_label_set_text(label_, "OTA UPDATE\n0%");
        lv_obj_set_style_text_color(label_, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_style_text_font(label_, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label_, LV_ALIGN_CENTER, 0, -60);

        // Progress bar
        bar_ = lv_bar_create(screen_);
        lv_obj_set_size(bar_, 300, 30);
        lv_obj_align(bar_, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(bar_, lv_color_hex(UI_COLOR_SURFACE), 0);
        lv_obj_set_style_bg_color(bar_, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_INDICATOR);
        lv_obj_set_style_border_color(bar_, lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(bar_, 2, 0);
        lv_obj_set_style_radius(bar_, 8, 0);
        lv_bar_set_range(bar_, 0, 100);
        lv_bar_set_value(bar_, 0, LV_ANIM_OFF);
    } else if (active_ && screen_) {
        // Update existing OTA screen
        if (bar_) {
            lv_bar_set_value(bar_, pendingProgress_, LV_ANIM_OFF);
        }
        if (label_) {
            char buffer[32];
            if (pendingProgress_ >= 100) {
                snprintf(buffer, sizeof(buffer), "OTA UPDATE\nCOMPLETE!");
            } else {
                snprintf(buffer, sizeof(buffer), "OTA UPDATE\n%u%%", pendingProgress_);
            }
            lv_label_set_text(label_, buffer);
        }
    } else if (!active_ && screen_) {
        teardown();
    }
}

void OtaScreen::teardown() {
    if (screen_) {
        lv_obj_del(screen_);
        screen_ = nullptr;
        label_ = nullptr;
        bar_ = nullptr;
    }
}
