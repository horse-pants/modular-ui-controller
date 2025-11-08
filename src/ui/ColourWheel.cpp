#include "ColourWheel.h"
#include "modular-ui.h"
#include "ui.h"
#include "WhiteButton.h"

// External stuff you already use
extern bool showAnimation;
extern WhiteButton* g_whiteButton;
extern bool white;
extern void updateWebUi();
extern CRGB leds[];
extern CFastLED FastLED;

ColourWheel::ColourWheel()
    : colorWheel_(nullptr)
    , callback_(nullptr)
    , initialized_(false)
{
}

ColourWheel::~ColourWheel() {
    cleanup();
}

ColourWheel::ColourWheel(ColourWheel&& other) noexcept
    : colorWheel_(other.colorWheel_)
    , callback_(std::move(other.callback_))
    , initialized_(other.initialized_)
{
    other.colorWheel_ = nullptr;
    other.callback_ = nullptr;
    other.initialized_ = false;
}

ColourWheel& ColourWheel::operator=(ColourWheel&& other) noexcept {
    if (this != &other) {
        cleanup();

        colorWheel_  = other.colorWheel_;
        callback_    = std::move(other.callback_);
        initialized_ = other.initialized_;

        other.colorWheel_ = nullptr;
        other.callback_   = nullptr;
        other.initialized_= false;
    }
    return *this;
}

bool ColourWheel::initialize(lv_obj_t* parent,
                             lv_coord_t size,
                             bool knobRecolor)
{
    if (initialized_) return true;
    if (!parent)      return false;

    // Create the LVGL color wheel widget
    colorWheel_ = lv_colorwheel_create(parent, knobRecolor);
    if (!colorWheel_) return false;

    // Size & position
    lv_obj_set_size(colorWheel_, size, size);
    lv_obj_center(colorWheel_);

    // Optional styling tweaks
    lv_obj_clear_flag(colorWheel_, LV_OBJ_FLAG_SCROLLABLE);

    // Hook event: use user_data on the *event* to point back to this instance
    lv_obj_add_event_cb(colorWheel_, eventHandler,
                        LV_EVENT_VALUE_CHANGED, this);

    initialized_ = true;
    return true;
}

void ColourWheel::setCallback(ColorChangeCallback callback) {
    callback_ = callback;
}

void ColourWheel::setColor(const String& hexString) {
    if (!initialized_ || !colorWheel_) return;

    String clean = hexString;
    if (clean.startsWith("#")) {
        clean = clean.substring(1);
    }

    int number = (int)strtol(clean.c_str(), nullptr, 16);
    uint8_t r = (number >> 16) & 0xFF;
    uint8_t g = (number >> 8)  & 0xFF;
    uint8_t b =  number        & 0xFF;

    setColor(r, g, b);
}

void ColourWheel::setColor(uint8_t r, uint8_t g, uint8_t b) {
    if (!initialized_ || !colorWheel_) return;

    lv_color_t col = lv_color_make(r, g, b);
    lv_colorwheel_set_rgb(colorWheel_, col);

    // Immediately apply side-effects
    handleColorChange();
}

String ColourWheel::getColorHex() const {
    uint8_t r, g, b;
    getColorRGB(r, g, b);
    return createRGBString(r, g, b);
}

void ColourWheel::getColorRGB(uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (!initialized_ || !colorWheel_) {
        r = g = b = 0;
        return;
    }

    lv_color_t col = lv_colorwheel_get_rgb(colorWheel_);
    lv_color32_t c32 = lv_color_to_32(col, LV_OPA_COVER);
    r = c32.red;
    g = c32.green;
    b = c32.blue;
}

void ColourWheel::getHSV(uint16_t& h, uint8_t& s, uint8_t& v) const {
    if (!initialized_ || !colorWheel_) {
        h = 0; s = 0; v = 0;
        return;
    }

    lv_color_hsv_t hsv = lv_colorwheel_get_hsv(colorWheel_);
    h = hsv.h;
    s = hsv.s;
    v = hsv.v;
}

void ColourWheel::setHSV(uint16_t h, uint8_t s, uint8_t v) {
    if (!initialized_ || !colorWheel_) return;

    lv_color_hsv_t hsv;
    hsv.h = h;
    hsv.s = s;
    hsv.v = v;

    lv_colorwheel_set_hsv(colorWheel_, hsv);
    handleColorChange();
}

void ColourWheel::eventHandler(lv_event_t* event) {
    auto* instance =
        static_cast<ColourWheel*>(lv_event_get_user_data(event));

    if (!instance) return;

    if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
        instance->handleColorChange();
    }
}

void ColourWheel::handleColorChange() {
    if (!initialized_ || !colorWheel_) return;

    // Get current color
    uint8_t r, g, b;
    getColorRGB(r, g, b);

    // Update LED strip
    showAnimation = false;
    FastLED.clear();
    colorFill(CRGB(r, g, b));

    // Clear white button state (prevent recursion)
    if (g_whiteButton && g_whiteButton->isInitialized()) {
        g_whiteButton->setState(false, false);
    }
    white = false;

    // Update web UI
    updateWebUi();

    // Call user callback if set
    if (callback_) {
        callback_(r, g, b);
    }
}

void ColourWheel::cleanup() {
    if (colorWheel_) {
        lv_obj_del(colorWheel_);
        colorWheel_ = nullptr;
    }
    callback_ = nullptr;
    initialized_ = false;
}

String ColourWheel::createRGBString(uint8_t r, uint8_t g, uint8_t b) const {
    long value = ((r & 0xff) << 16) +
                 ((g & 0xff) << 8)  +
                 (b & 0xff);
    char hexBuffer[10] = "";
    ltoa(value, hexBuffer, 16);
    String s = String(hexBuffer);
    while (s.length() < 6) {
        s = "0" + s;
    }
    return "#" + s;
}
