#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/**
 * @brief Modern C++ class for managing an LVGL VU button widget
 * 
 * This class provides a clean interface for creating and managing a VU
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
class VuButton {
public:
    using StateChangeCallback = std::function<void(bool isActive)>;

    /**
     * @brief Default constructor
     */
    VuButton();

    /**
     * @brief Destructor - automatically cleans up LVGL objects
     */
    ~VuButton();

    /**
     * @brief Move constructor
     */
    VuButton(VuButton&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    VuButton& operator=(VuButton&& other) noexcept;

    // Disable copy operations to prevent resource issues
    VuButton(const VuButton&) = delete;
    VuButton& operator=(const VuButton&) = delete;

    /**
     * @brief Initialize the VU button on the specified parent object
     * @param parent The parent LVGL object (usually a tab)
     * @param width Width of the button in pixels
     * @param xOffset X offset from right position
     * @param yOffset Y offset from top position
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(lv_obj_t* parent, lv_coord_t width = 100, lv_coord_t xOffset = -20, lv_coord_t yOffset = 10);

    /**
     * @brief Set the state change callback function
     * @param callback Function to call when button state changes
     */
    void setCallback(StateChangeCallback callback);

    /**
     * @brief Set the VU button state
     * @param isActive Whether the button should be active
     */
    void setState(bool isActive);

    /**
     * @brief Get the current VU button state
     * @return true if active, false otherwise
     */
    bool getState() const;

    /**
     * @brief Check if the VU button is initialized
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