#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/**
 * @brief Touch HSV colour wheel (LVGL 9).
 *
 * LVGL 9 ships no colour picker — v8's lv_colorwheel was dropped and never
 * replaced — so the wheel is painted pixel-by-pixel into an lv_canvas: angle =
 * hue, distance from centre = saturation, value pinned at full (overall
 * brightness is the fader's job, not part of picking a colour).
 *
 * The paint happens ONCE in initialize() and the buffer is retained: the wheel
 * image never changes, only the knob moves, so every later frame is a plain
 * blit. That costs size * size * 2 bytes of PSRAM for the object's lifetime.
 *
 * A pure widget — it reports the picked colour through the callback and knows
 * nothing about LEDs or the web UI. The owner (ColourTab) applies the effects.
 */
class ColourWheel {
public:
    using ColorChangeCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;

    ColourWheel() = default;
    ~ColourWheel();

    ColourWheel(const ColourWheel&) = delete;
    ColourWheel& operator=(const ColourWheel&) = delete;

    /**
     * @brief Build the wheel on @p parent.
     * @param size     Diameter in pixels (the widget is a size x size square).
     * @param backdrop 0xRRGGBB painted outside the circle. The canvas is square
     *                 and RGB565 has no alpha, so this has to be the colour of
     *                 the card behind the wheel or the corners will show.
     * @return false if the pixel buffer couldn't be allocated — a large
     *         contiguous PSRAM block can fail on a fragmented heap, so callers
     *         must degrade rather than assume success.
     */
    bool initialize(lv_obj_t* parent, int32_t size, uint32_t backdrop);

    void setCallback(ColorChangeCallback callback);

    /// Set the colour from a hex string (e.g. "#FF0000"). Fires the callback.
    void setColor(const String& hexString);

    /**
     * @brief Move the knob to an RGB colour.
     * @param notify Fire the change callback (default). Pass false to sync the
     *               UI without re-applying the colour downstream.
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b, bool notify = true);

    String getColorHex() const;
    void getColorRGB(uint8_t& r, uint8_t& g, uint8_t& b) const;

    bool isInitialized() const { return wrapper_ != nullptr; }

private:
    static void touchEventHandler(lv_event_t* event);
    void handleTouch(lv_event_code_t code);
    void paintWheel(uint32_t backdrop);
    void placeKnob();
    void commit();
    void notifyChange();

    lv_obj_t* wrapper_ = nullptr;   // owns the input; knob is a canvas sibling
    lv_obj_t* canvas_  = nullptr;
    lv_obj_t* knob_    = nullptr;
    void*     buf_     = nullptr;

    int32_t size_   = 0;
    int32_t radius_ = 0;

    // Value stays at 100 for anything picked off the wheel, but a colour pushed
    // in from web/MQTT can be dimmer, so it's carried rather than discarded.
    lv_color_hsv_t hsv_ = { 0, 0, 100 };

    // Last colour actually reported. A press moves the knob before the gesture
    // is known to be a pick or a tab swipe, so this is what it rolls back to.
    lv_color_hsv_t committed_ = { 0, 0, 100 };

    ColorChangeCallback callback_;
};
