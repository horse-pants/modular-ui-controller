#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

// If lv_colorwheel.h is in your lvgl tree, include it:
extern "C" {
#include "lv_colorwheel.h"
}

/**
 * @brief C++ wrapper for an LVGL color wheel widget (LVGL 9)
 *
 * - Wraps a single lv_colorwheel object (no sliders)
 * - Callback on color change
 * - RAII + move semantics
 */
class ColourWheel {
public:
    using ColorChangeCallback = std::function<void(uint8_t r, uint8_t g, uint8_t b)>;

    ColourWheel();
    ~ColourWheel();

    ColourWheel(ColourWheel&& other) noexcept;
    ColourWheel& operator=(ColourWheel&& other) noexcept;

    ColourWheel(const ColourWheel&) = delete;
    ColourWheel& operator=(const ColourWheel&) = delete;

    /**
     * @brief Initialize the color wheel on the specified parent object
     * @param parent The parent LVGL object (usually a tab or screen)
     * @param size   Size of the wheel (width/height)
     * @param knobRecolor Whether the knob shows the current color
     * @return true if initialization was successful
     */
    bool initialize(lv_obj_t* parent,
                    lv_coord_t size = 200,
                    bool knobRecolor = true);

    /**
     * @brief Set the color change callback function
     */
    void setCallback(ColorChangeCallback callback);

    /**
     * @brief Set the color from a hex string (e.g. "#FF0000")
     */
    void setColor(const String& hexString);

    /**
     * @brief Set the color from RGB components
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Get the current color as a hex string
     */
    String getColorHex() const;

    /**
     * @brief Get the current color as RGB components
     */
    void getColorRGB(uint8_t& r, uint8_t& g, uint8_t& b) const;

    /**
     * @brief Get current HSV components from the wheel
     */
    void getHSV(uint16_t& h, uint8_t& s, uint8_t& v) const;

    /**
     * @brief Set HSV components on the wheel
     */
    void setHSV(uint16_t h, uint8_t s, uint8_t v);

    /**
     * @brief Check if the color wheel is initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the underlying LVGL object
     */
    lv_obj_t* getLvglObject() const { return colorWheel_; }

private:
    lv_obj_t* colorWheel_;          // LVGL color wheel object
    ColorChangeCallback callback_;
    bool initialized_;

    static void eventHandler(lv_event_t* event);
    void handleColorChange();
    void cleanup();

    String createRGBString(uint8_t r, uint8_t g, uint8_t b) const;
};
