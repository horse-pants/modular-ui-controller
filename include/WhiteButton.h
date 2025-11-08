#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/**
 * @brief Modern C++ class for managing an LVGL white button widget
 * 
 * This class provides a clean interface for creating and managing a white
 * button with proper resource management and callback support.
 * 
 * Features:
 * - RAII resource management
 * - Move semantics support
 * - Callback-based event handling
 * - Automatic cleanup on destruction
 * - Exception safety
 * 
 * @author Claude Code
 */
class WhiteButton {
public:
    using StateChangeCallback = std::function<void(bool isWhite)>;

    /**
     * @brief Default constructor
     */
    WhiteButton();

    /**
     * @brief Destructor - automatically cleans up LVGL objects
     */
    ~WhiteButton();

    /**
     * @brief Move constructor
     */
    WhiteButton(WhiteButton&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    WhiteButton& operator=(WhiteButton&& other) noexcept;

    // Disable copy operations to prevent resource issues
    WhiteButton(const WhiteButton&) = delete;
    WhiteButton& operator=(const WhiteButton&) = delete;

    /**
     * @brief Initialize the white button on the specified parent object
     * @param parent The parent LVGL object (usually a tab)
     * @param width Width of the button in pixels
     * @param xOffset X offset from center position
     * @param yOffset Y offset from bottom position
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(lv_obj_t* parent, lv_coord_t width = 120, lv_coord_t xOffset = 10, lv_coord_t yOffset = 0);

    /**
     * @brief Set the state change callback function
     * @param callback Function to call when button state changes
     */
    void setCallback(StateChangeCallback callback);

    /**
     * @brief Set the white button state
     * @param isWhite Whether the button should be in white mode
     * @param triggerCallback Whether to trigger callbacks (default true)
     */
    void setState(bool isWhite, bool triggerCallback = true);

    /**
     * @brief Get the current white button state
     * @return true if in white mode, false otherwise
     */
    bool getState() const;

    /**
     * @brief Check if the white button is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the underlying LVGL object (for advanced use)
     * @return Pointer to the LVGL button object, or nullptr if not initialized
     */
    lv_obj_t* getLvglObject() const { return button_; }

private:
    lv_obj_t* button_;
    StateChangeCallback callback_;
    bool initialized_;
    bool currentState_;

    /**
     * @brief Static event handler for LVGL events
     */
    static void eventHandler(lv_event_t* event);

    /**
     * @brief Internal state change handler
     */
    void handleStateChange();

    /**
     * @brief Clean up LVGL objects
     */
    void cleanup();

    /**
     * @brief Apply button styling
     */
    void applyButtonStyling();
};