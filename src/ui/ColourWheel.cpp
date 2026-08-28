#include "ui/ColourWheel.h"
#include "modular-ui.h"
#include <Logger.h>
#include <esp_heap_caps.h>
#include <cmath>

namespace {

// The knob overhangs the wheel square by half its width at full saturation;
// the parent container leaves padding for that (see ColourTab).
constexpr int32_t KNOB_SIZE = 24;

// Width of the soft edge at the rim, in pixels. A hard-thresholded circle at
// this size is visibly jagged without it.
constexpr float FEATHER_PX = 1.5f;

constexpr float DEG_PER_RAD = 180.0f / static_cast<float>(M_PI);

uint8_t lerp8(uint8_t from, uint8_t to, float t) {
    return static_cast<uint8_t>(from + (to - from) * t + 0.5f);
}

// True while LVGL is scrolling something under the finger. The wheel sits in a
// tab page and the tabview swipes horizontally, so a drag across the wheel can
// turn into a tab change; nothing else guards that.
bool isScrollInProgress() {
    lv_indev_t* indev = lv_indev_active();
    return indev && lv_indev_get_scroll_obj(indev) != nullptr;
}

}  // namespace

ColourWheel::~ColourWheel() {
    // The canvas holds a draw_buf pointing at buf_, so the objects go first.
    // Deleting the wrapper takes the canvas and the knob (and the event
    // callbacks registered on it) with it.
    if (wrapper_) {
        lv_obj_delete(wrapper_);
        wrapper_ = nullptr;
        canvas_  = nullptr;
        knob_    = nullptr;
    }
    if (buf_) {
        heap_caps_free(buf_);
        buf_ = nullptr;
    }
    callback_ = nullptr;
}

bool ColourWheel::initialize(lv_obj_t* parent, int32_t size, uint32_t backdrop) {
    if (wrapper_)             return true;
    if (!parent || size <= 0) return false;

    size_   = size;
    radius_ = size / 2;

    const size_t bufSize = LV_CANVAS_BUF_SIZE(size_, size_, 16, LV_DRAW_BUF_STRIDE_ALIGN);
    buf_ = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bufSize, MALLOC_CAP_SPIRAM);
    if (!buf_) {
        Logger.error("ColourWheel: %u byte PSRAM alloc failed (largest free block %u)",
                     static_cast<unsigned>(bufSize),
                     static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
        return false;
    }

    // A wrapper, so the knob is a SIBLING of the canvas rather than a child of
    // an image widget, and so one object owns the whole square's input.
    wrapper_ = lv_obj_create(parent);
    lv_obj_set_size(wrapper_, size_, size_);
    lv_obj_set_style_bg_opa(wrapper_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrapper_, 0, 0);
    lv_obj_set_style_pad_all(wrapper_, 0, 0);
    lv_obj_remove_flag(wrapper_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(wrapper_, LV_OBJ_FLAG_CLICKABLE);
    // The knob hangs over the square at full saturation.
    lv_obj_add_flag(wrapper_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    canvas_ = lv_canvas_create(wrapper_);
    lv_canvas_set_buffer(canvas_, buf_, size_, size_, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas_, 0, 0);
    lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);   // the wrapper owns input

    paintWheel(backdrop);

    knob_ = lv_obj_create(wrapper_);
    lv_obj_set_size(knob_, KNOB_SIZE, KNOB_SIZE);
    lv_obj_set_style_radius(knob_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(knob_, UI_BORDER_THICK, 0);
    lv_obj_set_style_border_color(knob_, lv_color_hex(UI_COLOR_WHITE), 0);
    lv_obj_set_style_pad_all(knob_, 0, 0);
    lv_obj_remove_flag(knob_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(knob_, LV_OBJ_FLAG_CLICKABLE);     // never swallow a drag
    placeKnob();

    // PRESSED positions the knob, PRESSING previews a drag live, RELEASED
    // reports a plain tap (which produces no PRESSING). See handleTouch for why
    // the press itself must not report.
    lv_obj_add_event_cb(wrapper_, touchEventHandler, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(wrapper_, touchEventHandler, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(wrapper_, touchEventHandler, LV_EVENT_RELEASED, this);

    return true;
}

void ColourWheel::paintWheel(uint32_t backdrop) {
    const uint8_t bgR = (backdrop >> 16) & 0xFF;
    const uint8_t bgG = (backdrop >> 8)  & 0xFF;
    const uint8_t bgB =  backdrop        & 0xFF;
    const lv_color_t bg = lv_color_make(bgR, bgG, bgB);

    const float centre = (size_ - 1) / 2.0f;
    const float rad    = radius_ - 1.0f;

    for (int32_t y = 0; y < size_; ++y) {
        const float dy = y - centre;
        for (int32_t x = 0; x < size_; ++x) {
            const float dx   = x - centre;
            const float dist = sqrtf(dx * dx + dy * dy);

            if (dist > rad + FEATHER_PX) {
                lv_canvas_set_px(canvas_, x, y, bg, LV_OPA_COVER);
                continue;
            }

            float angle = atan2f(dy, dx) * DEG_PER_RAD;
            if (angle < 0.0f) angle += 360.0f;
            const float satF = (dist / rad) * 100.0f;

            lv_color_t c = lv_color_hsv_to_rgb(static_cast<uint16_t>(angle),
                                               static_cast<uint8_t>(satF > 100.0f ? 100.0f : satF),
                                               100);
            // Feather the rim into the backdrop instead of hard-clipping it.
            if (dist > rad) {
                const float t = (dist - rad) / FEATHER_PX;
                c = lv_color_make(lerp8(c.red,   bgR, t),
                                  lerp8(c.green, bgG, t),
                                  lerp8(c.blue,  bgB, t));
            }
            lv_canvas_set_px(canvas_, x, y, c, LV_OPA_COVER);
        }
    }

    // The pixels were written by the CPU, but the draw unit can reach PSRAM over
    // DMA - without a writeback it can blit a stale line.
    lv_draw_buf_t* drawBuf = lv_canvas_get_draw_buf(canvas_);
    if (drawBuf) lv_draw_buf_flush_cache(drawBuf, nullptr);
    lv_obj_invalidate(canvas_);
}

void ColourWheel::touchEventHandler(lv_event_t* event) {
    auto* instance = static_cast<ColourWheel*>(lv_event_get_user_data(event));
    if (instance) instance->handleTouch(lv_event_get_code(event));
}

void ColourWheel::handleTouch(lv_event_code_t code) {
    if (!wrapper_) return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;

    // The drag turned into a tab swipe. Report nothing, and put the knob back
    // where the last reported colour was - the press that started the scroll
    // already moved it.
    if (isScrollInProgress()) {
        hsv_ = committed_;
        placeKnob();
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t area;
    lv_obj_get_coords(wrapper_, &area);

    const float centre = (size_ - 1) / 2.0f;
    const float dx  = (point.x - area.x1) - centre;
    const float dy  = (point.y - area.y1) - centre;
    const float rad = radius_ - 1.0f;

    // Outside the circle still picks, clamped to the rim, so a drag that
    // overshoots keeps tracking hue instead of freezing.
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > rad) dist = rad;

    float angle = atan2f(dy, dx) * DEG_PER_RAD;
    if (angle < 0.0f) angle += 360.0f;

    const float satF = (dist / rad) * 100.0f;
    hsv_.h = static_cast<uint16_t>(angle);
    hsv_.s = static_cast<uint8_t>(satF > 100.0f ? 100.0f : satF);
    hsv_.v = 100;

    placeKnob();

    // A press moves the knob but reports nothing: a press is also how a tab
    // swipe begins, and at press time there is no scroll yet for the guard above
    // to see. The colour goes out once the gesture is known - PRESSING (a drag,
    // giving the live preview) or RELEASED (a tap).
    if (code == LV_EVENT_PRESSED) return;

    notifyChange();
}

void ColourWheel::setCallback(ColorChangeCallback callback) {
    callback_ = callback;
}

void ColourWheel::setColor(const String& hexString) {
    String clean = hexString;
    if (clean.startsWith("#")) {
        clean = clean.substring(1);
    }

    const long value = strtol(clean.c_str(), nullptr, 16);
    setColor(static_cast<uint8_t>((value >> 16) & 0xFF),
             static_cast<uint8_t>((value >> 8)  & 0xFF),
             static_cast<uint8_t>( value        & 0xFF));
}

void ColourWheel::setColor(uint8_t r, uint8_t g, uint8_t b, bool notify) {
    if (!wrapper_) return;

    hsv_ = lv_color_rgb_to_hsv(r, g, b);
    placeKnob();

    if (notify) {
        notifyChange();
    } else {
        commit();
    }
}

String ColourWheel::getColorHex() const {
    uint8_t r, g, b;
    getColorRGB(r, g, b);

    // Oversized on purpose: "#RRGGBB" needs 8, but %02X takes an int so GCC's
    // -Wformat-truncation assumes the worst case.
    char hex[16];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    return String(hex);
}

void ColourWheel::getColorRGB(uint8_t& r, uint8_t& g, uint8_t& b) const {
    if (!wrapper_) {
        r = g = b = 0;
        return;
    }

    const lv_color_t c = lv_color_hsv_to_rgb(hsv_.h, hsv_.s, hsv_.v);
    r = c.red;
    g = c.green;
    b = c.blue;
}

void ColourWheel::commit() {
    committed_ = hsv_;
}

void ColourWheel::notifyChange() {
    commit();

    if (callback_) {
        uint8_t r, g, b;
        getColorRGB(r, g, b);
        callback_(r, g, b);
    }
}

void ColourWheel::placeKnob() {
    if (!knob_) return;

    const float centre = (size_ - 1) / 2.0f;
    const float rad    = radius_ - 1.0f;
    const float theta  = hsv_.h / DEG_PER_RAD;
    const float dist   = (hsv_.s / 100.0f) * rad;

    lv_obj_set_pos(knob_,
                   static_cast<int32_t>(centre + cosf(theta) * dist) - KNOB_SIZE / 2,
                   static_cast<int32_t>(centre + sinf(theta) * dist) - KNOB_SIZE / 2);

    // Gesture-rate, not tick-rate: the knob's fill shows the picked colour, so
    // it has to change when the colour does. This isn't the style-thrash the
    // LVGL rule warns about (that's restyling from an update tick).
    lv_obj_set_style_bg_color(knob_, lv_color_hsv_to_rgb(hsv_.h, hsv_.s, hsv_.v), 0);
}
