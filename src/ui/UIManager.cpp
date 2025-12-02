#include "UIManager.h"
#include "modular-ui.h"
#include "ui.h"
#include <memory>
#include <Logger.h>

// Global instance definitions
UIManager* g_uiManager = nullptr;
BrightnessSlider* g_brightnessSlider = nullptr;
ColourWheel* g_colourWheel = nullptr;
EffectsList* g_effectsList = nullptr;
WhiteButton* g_whiteButton = nullptr;
VuButton* g_vuButton = nullptr;
VuGraph* g_vuGraph = nullptr;

// LGFX Display class - moved from ui.cpp
class MyLGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    MyLGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 40000000;
            cfg.pin_wr = 47;
            cfg.pin_rd = -1;
            cfg.pin_rs = 0;
            cfg.pin_d0 = 9;
            cfg.pin_d1 = 46;
            cfg.pin_d2 = 3;
            cfg.pin_d3 = 8;
            cfg.pin_d4 = 18;
            cfg.pin_d5 = 17;
            cfg.pin_d6 = 16;
            cfg.pin_d7 = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = -1;
            cfg.pin_rst = 4;
            cfg.pin_busy = -1;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 45;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda = 6;
            cfg.pin_scl = 5;
            cfg.freq = 400000;
            cfg.x_min = 0;
            cfg.x_max = 320;
            cfg.y_min = 0;
            cfg.y_max = 480;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

// Static display instance
static MyLGFX lcd;

// LVGL display configuration
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 480;
static lv_color_t buf[screenWidth * 10];

// Display flush callback
void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushPixels((uint16_t*)px_map, w * h, true);
    lcd.endWrite();

    lv_display_flush_ready(disp);
}

// Touch read callback
void touchpadRead(lv_indev_t* indev_driver, lv_indev_data_t* data) {
    uint16_t touchX, touchY;
    bool touched = lcd.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

UIManager::UIManager()
    : brightnessSlider_(nullptr)
    , colourWheel_(nullptr)
    , effectsList_(nullptr)
    , whiteButton_(nullptr)
    , vuButton_(nullptr)
    , vuGraph_(nullptr)
    , tabview_(nullptr)
    , tab1_(nullptr)
    , tab2_(nullptr)
    , tab3_(nullptr)
    , otaScreen_(nullptr)
    , otaLabel_(nullptr)
    , otaBar_(nullptr)
    , otaScreenActive_(false)
    , otaPendingProgress_(0)
    , otaProgressChanged_(false)
    , initialized_(false)
    , screenInitialized_(false)
{
}

UIManager::~UIManager() {
    cleanup();
}

UIManager::UIManager(UIManager&& other) noexcept
    : brightnessSlider_(std::move(other.brightnessSlider_))
    , colourWheel_(std::move(other.colourWheel_))
    , effectsList_(std::move(other.effectsList_))
    , whiteButton_(std::move(other.whiteButton_))
    , vuButton_(std::move(other.vuButton_))
    , vuGraph_(std::move(other.vuGraph_))
    , tabview_(other.tabview_)
    , tab1_(other.tab1_)
    , tab2_(other.tab2_)
    , tab3_(other.tab3_)
    , otaScreen_(other.otaScreen_)
    , otaLabel_(other.otaLabel_)
    , otaBar_(other.otaBar_)
    , otaScreenActive_(other.otaScreenActive_)
    , otaPendingProgress_(other.otaPendingProgress_)
    , otaProgressChanged_(other.otaProgressChanged_)
    , initialized_(other.initialized_)
    , screenInitialized_(other.screenInitialized_)
{
    // Reset the moved-from object
    other.tabview_ = nullptr;
    other.tab1_ = nullptr;
    other.tab2_ = nullptr;
    other.tab3_ = nullptr;
    other.otaScreen_ = nullptr;
    other.otaLabel_ = nullptr;
    other.otaBar_ = nullptr;
    other.otaScreenActive_ = false;
    other.otaPendingProgress_ = 0;
    other.otaProgressChanged_ = false;
    other.initialized_ = false;
    other.screenInitialized_ = false;
}

UIManager& UIManager::operator=(UIManager&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();

        // Move resources from other
        brightnessSlider_ = std::move(other.brightnessSlider_);
        colourWheel_ = std::move(other.colourWheel_);
        effectsList_ = std::move(other.effectsList_);
        whiteButton_ = std::move(other.whiteButton_);
        vuButton_ = std::move(other.vuButton_);
        vuGraph_ = std::move(other.vuGraph_);
        tabview_ = other.tabview_;
        tab1_ = other.tab1_;
        tab2_ = other.tab2_;
        tab3_ = other.tab3_;
        otaScreen_ = other.otaScreen_;
        otaLabel_ = other.otaLabel_;
        otaBar_ = other.otaBar_;
        otaScreenActive_ = other.otaScreenActive_;
        otaPendingProgress_ = other.otaPendingProgress_;
        otaProgressChanged_ = other.otaProgressChanged_;
        initialized_ = other.initialized_;
        screenInitialized_ = other.screenInitialized_;

        // Reset the moved-from object
        other.tabview_ = nullptr;
        other.tab1_ = nullptr;
        other.tab2_ = nullptr;
        other.tab3_ = nullptr;
        other.otaScreen_ = nullptr;
        other.otaLabel_ = nullptr;
        other.otaBar_ = nullptr;
        other.otaScreenActive_ = false;
        other.otaPendingProgress_ = 0;
        other.otaProgressChanged_ = false;
        other.initialized_ = false;
        other.screenInitialized_ = false;
    }
    return *this;
}

bool UIManager::initializeScreen() {
    if (screenInitialized_) {
        return true; // Already initialized
    }
    
    try {
        // Initialize LovyanGFX
        lcd.init();
        lv_init();
        
        lcd.setRotation(2);
        
        setupDisplayDriver();
        setupTouchDriver();
        
        screenInitialized_ = true;
        return true;
        
    } catch (...) {
        return false;
    }
}

bool UIManager::initializeUI() {
    if (initialized_) {
        return true; // Already initialized
    }
    
    if (!screenInitialized_ && !initializeScreen()) {
        return false; // Screen initialization failed
    }
    
    try {
        // Apply the synth theme first
        applySynthTheme();
        
        // Create tabview
        if (!createTabview()) {
            return false;
        }
        
        // Initialize all components
        if (!initializeComponents()) {
            cleanup();
            return false;
        }
        
        initialized_ = true;
        return true;
        
    } catch (...) {
        cleanup();
        return false;
    }
}

void UIManager::update() {
    if (!initialized_) {
        return;
    }

    // Handle OTA screen updates (must be in main loop for LVGL thread safety)
    if (otaProgressChanged_) {
        otaProgressChanged_ = false;

        if (otaScreenActive_ && !otaScreen_) {
            // Create OTA screen
            otaScreen_ = lv_obj_create(lv_scr_act());
            lv_obj_set_size(otaScreen_, LV_HOR_RES, LV_VER_RES);
            lv_obj_set_style_bg_color(otaScreen_, lv_color_hex(UI_COLOR_BACKGROUND), 0);
            lv_obj_set_style_bg_opa(otaScreen_, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(otaScreen_, 0, 0);
            lv_obj_clear_flag(otaScreen_, LV_OBJ_FLAG_SCROLLABLE);

            // Title label
            otaLabel_ = lv_label_create(otaScreen_);
            lv_label_set_text(otaLabel_, "OTA UPDATE\n0%");
            lv_obj_set_style_text_color(otaLabel_, lv_color_hex(UI_COLOR_PRIMARY), 0);
            lv_obj_set_style_text_font(otaLabel_, &lv_font_montserrat_32, 0);
            lv_obj_set_style_text_align(otaLabel_, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(otaLabel_, LV_ALIGN_CENTER, 0, -60);

            // Progress bar
            otaBar_ = lv_bar_create(otaScreen_);
            lv_obj_set_size(otaBar_, 300, 30);
            lv_obj_align(otaBar_, LV_ALIGN_CENTER, 0, 20);
            lv_obj_set_style_bg_color(otaBar_, lv_color_hex(UI_COLOR_SURFACE), 0);
            lv_obj_set_style_bg_color(otaBar_, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_INDICATOR);
            lv_obj_set_style_border_color(otaBar_, lv_color_hex(UI_COLOR_BORDER), 0);
            lv_obj_set_style_border_width(otaBar_, 2, 0);
            lv_obj_set_style_radius(otaBar_, 8, 0);
            lv_bar_set_range(otaBar_, 0, 100);
            lv_bar_set_value(otaBar_, 0, LV_ANIM_OFF);
        } else if (otaScreenActive_ && otaScreen_) {
            // Update existing OTA screen
            if (otaBar_) {
                lv_bar_set_value(otaBar_, otaPendingProgress_, LV_ANIM_OFF);
            }
            if (otaLabel_) {
                char buffer[32];
                if (otaPendingProgress_ >= 100) {
                    snprintf(buffer, sizeof(buffer), "OTA UPDATE\nCOMPLETE!");
                } else {
                    snprintf(buffer, sizeof(buffer), "OTA UPDATE\n%u%%", otaPendingProgress_);
                }
                lv_label_set_text(otaLabel_, buffer);
            }
        } else if (!otaScreenActive_ && otaScreen_) {
            // Cleanup OTA screen
            lv_obj_del(otaScreen_);
            otaScreen_ = nullptr;
            otaLabel_ = nullptr;
            otaBar_ = nullptr;
        }
    }

    // Update VU graph if it exists
    if (vuGraph_) {
        vuGraph_->update();
    }

    // Process LVGL tasks
    lv_timer_handler();
}

void UIManager::applyCurrentColor() {
    if (colourWheel_) {
        uint8_t r, g, b;
        colourWheel_->getColorRGB(r, g, b);
        showAnimation = false;
        FastLED.clear();
        colorFill(CRGB(r, g, b));
        if (whiteButton_) {
            whiteButton_->setState(false, false);
        }
        white = false;
        updateWebUi();
        
        // Update LED manager
        extern LEDManager* g_ledManager;
        if (g_ledManager) {
            g_ledManager->setAnimationEnabled(false);
            g_ledManager->setWhiteMode(false);
        }
    }
}

void UIManager::logAndUpdateVuState(bool newState) {
    Logger.info("VU Button - State: %s", newState ? "ON" : "OFF");

    vu = newState;

    // Update LED manager
    extern LEDManager* g_ledManager;
    if (g_ledManager) {
        g_ledManager->setVuMode(newState);
    }

    updateWebUi();
}

void UIManager::logAndUpdateWhiteState(bool newState) {
    Logger.info("White Button - State: %s", newState ? "ON" : "OFF");

    white = newState;

    if (newState) {
        showAnimation = false;
        fillWhite();
    }

    // Update LED manager
    extern LEDManager* g_ledManager;
    if (g_ledManager) {
        g_ledManager->setWhiteMode(newState);
        if (newState) {
            g_ledManager->setAnimationEnabled(false);
            showAnimation = false;
        }
    }

    updateWebUi();
}

void UIManager::setVuState(bool newState) {
    logAndUpdateVuState(newState);

    if (vuButton_) {
        vuButton_->setState(newState);
    }
}

void UIManager::setWhiteState(bool newState) {
    logAndUpdateWhiteState(newState);

    if (whiteButton_) {
        whiteButton_->setState(newState);
    }
}

void UIManager::setAnimationState(bool newState) {
    showAnimation = newState;
    if (newState) {
        if (whiteButton_) {
            whiteButton_->setState(false, false);
        }
        white = false;
        FastLED.clear();
    } else {
        applyCurrentColor();
    }
    
    // Update LED manager
    extern LEDManager* g_ledManager;
    if (g_ledManager) {
        g_ledManager->setAnimationEnabled(newState);
    }
}

void UIManager::setAnimation(int animation) {
    if (effectsList_) {
        effectsList_->setSelectedEffect(animation, false);
    }
    showAnimation = true;
    currentAnimation = static_cast<animationOptions>(animation);
    if (whiteButton_) {
        whiteButton_->setState(false, false);
    }
    white = false;
    FastLED.clear();
    
    // Update LED manager
    extern LEDManager* g_ledManager;
    if (g_ledManager) {
        g_ledManager->setCurrentAnimation(static_cast<LEDManager::AnimationType>(animation));
        g_ledManager->setAnimationEnabled(true);
    }
}

void UIManager::applySynthTheme() {
    // Apply dark synth theme to screen background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_grad_color(lv_scr_act(), lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_grad_dir(lv_scr_act(), LV_GRAD_DIR_VER, 0);
}

bool UIManager::createTabview() {
    // Create a Tab view object
    tabview_ = lv_tabview_create(lv_scr_act());
    if (!tabview_) {
        return false;
    }

    // Set tab bar position and size
    lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview_, 50);
    
    lv_obj_clear_flag(lv_tabview_get_content(tabview_), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(lv_tabview_get_content(tabview_), scrollBeginEvent, LV_EVENT_SCROLL_BEGIN, NULL);

    // Style the tabview with synth theme
    lv_obj_set_style_bg_color(tabview_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(tabview_, LV_OPA_COVER, 0);
    
    lv_obj_set_style_border_width(tabview_, 3, 0);
    lv_obj_set_style_border_color(tabview_, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_opa(tabview_, LV_OPA_COVER, 0);

    // Create tabs
    tab1_ = lv_tabview_add_tab(tabview_, "Colour");
    tab2_ = lv_tabview_add_tab(tabview_, "Effects");
    tab3_ = lv_tabview_add_tab(tabview_, "VU");
    
    if (!tab1_ || !tab2_ || !tab3_) {
        return false;
    }
    
    // Style individual tabs
    lv_obj_set_style_bg_color(tab1_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(tab2_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(tab3_, lv_color_hex(UI_COLOR_SURFACE), 0);
    
    // Style tab buttons properly using LV_PART_ITEMS for individual buttons
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tabview_);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(UI_COLOR_SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(UI_COLOR_PRIMARY), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(UI_COLOR_TEXT), LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_btns, lv_color_hex(UI_COLOR_WHITE), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(tab_btns, 2, 0);
    
    // Remove the bottom indicator line that shows in blue
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_NONE, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_outline_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(tab_btns, 0, LV_PART_ITEMS | LV_STATE_CHECKED);
    
    return true;
}

bool UIManager::initializeComponents() {
    // Initialize brightness slider
    brightnessSlider_.reset(new BrightnessSlider(255)); // MAX_BRIGHTNESS
    if (brightnessSlider_) {
        // Set callback BEFORE initialization to ensure proper state sync
        brightnessSlider_->setCallback([this](int newBrightness) {
            brightness = newBrightness;
            vu = false;
            if (vuButton_) {
                vuButton_->setState(false);
            }
            updateWebUi();
            
            // Update LED manager
            extern LEDManager* g_ledManager;
            if (g_ledManager) {
                g_ledManager->setBrightness(newBrightness);
                g_ledManager->setVuMode(false);
            }
        });
        
        // Initialize with current brightness value (should be 128 = middle of 255)
        if (!brightnessSlider_->initialize(tab1_, 20, 10, brightness)) {
            return false;
        }
    }
    
    // Initialize VU button
    vuButton_.reset(new VuButton());
    if (vuButton_ && !vuButton_->initialize(tab1_)) {
        return false;
    }
    
    // Set VU button callback
    if (vuButton_) {
        vuButton_->setCallback([this](bool newState) {
            vu = newState;
            
            // Update LED manager
            extern LEDManager* g_ledManager;
            if (g_ledManager) {
                g_ledManager->setVuMode(newState);
            }
            
            updateWebUi();
        });
    }
    
    // Initialize VU graph
    vuGraph_.reset(new VuGraph());
    if (vuGraph_ && !vuGraph_->initialize(tab3_)) {
        return false;
    }
    
    // Initialize colour wheel
    colourWheel_.reset(new ColourWheel());
    if (colourWheel_ && !colourWheel_->initialize(tab1_, 200, true)) {
        return false;
    }
    
    // Set color wheel callback
    if (colourWheel_) {
        colourWheel_->setCallback([this](uint8_t r, uint8_t g, uint8_t b) {
            this->applyCurrentColor();
        });
    }
    
    // Initialize effects list
    effectsList_.reset(new EffectsList());
    if (effectsList_ && !effectsList_->initialize(tab2_)) {
        return false;
    }
    
    // Set effects list callback
    if (effectsList_) {
        effectsList_->setCallback([this](int effectIndex) {
            showAnimation = true;
            currentAnimation = static_cast<LEDManager::AnimationType>(effectIndex);
            
            // Update LED manager
            extern LEDManager* g_ledManager;
            if (g_ledManager) {
                g_ledManager->setAnimationEnabled(true);
                g_ledManager->setCurrentAnimation(static_cast<LEDManager::AnimationType>(effectIndex));
            }
            
            updateWebUi();
        });
    }
    
    // Initialize white button
    whiteButton_.reset(new WhiteButton());
    if (whiteButton_ && !whiteButton_->initialize(tab1_)) {
        return false;
    }
    
    return true;
}

void UIManager::setupDisplayDriver() {
    // LVGL 9: Create display
    lv_display_t* disp = lv_display_create(screenWidth, screenHeight);

    // Set display buffer
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Set flush callback
    lv_display_set_flush_cb(disp, displayFlush);
}

void UIManager::setupTouchDriver() {
    // LVGL 9: Create and setup input device
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpadRead);
}

void UIManager::cleanup() {
    // Smart pointers will automatically clean up their resources
    brightnessSlider_.reset();
    colourWheel_.reset();
    effectsList_.reset();
    whiteButton_.reset();
    vuButton_.reset();
    vuGraph_.reset();

    // Clean up LVGL objects
    if (tabview_) {
        lv_obj_del(tabview_);
        tabview_ = nullptr;
    }

    // Clean up OTA screen if it exists
    if (otaScreen_) {
        lv_obj_del(otaScreen_);
        otaScreen_ = nullptr;
        otaLabel_ = nullptr;
        otaBar_ = nullptr;
    }

    tab1_ = nullptr;
    tab2_ = nullptr;
    tab3_ = nullptr;
    initialized_ = false;
}

void UIManager::scrollBeginEvent(lv_event_t* e) {
    // Disable the scroll animations. Triggered when a tab button is clicked
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        lv_anim_t* a = (lv_anim_t*)lv_event_get_param(e);
        if (a) lv_anim_set_duration(a, 0);
    }
}

void UIManager::showOTAScreen() {
    // Just set flag - actual screen creation happens in update()
    otaScreenActive_ = true;
    otaPendingProgress_ = 0;
    otaProgressChanged_ = true;
    g_ledManager->fillColor(CRGB(0, 0, 0));
}

void UIManager::updateOTAProgress(uint8_t progress) {
    // Just set flag - actual update happens in update()
    otaPendingProgress_ = progress;
    otaProgressChanged_ = true;
}

void UIManager::hideOTAScreen() {
    // Just set flag - actual cleanup happens in update()
    otaScreenActive_ = false;
    otaProgressChanged_ = true;
}