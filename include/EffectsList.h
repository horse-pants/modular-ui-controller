#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/**
 * @brief Modern C++ class for managing an LVGL effects roller widget
 * 
 * This class provides a clean interface for creating and managing an effects
 * selection roller with proper resource management and callback support.
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
class EffectsList {
public:
    using EffectChangeCallback = std::function<void(int effectIndex)>;

    /**
     * @brief Default constructor
     */
    EffectsList();

    /**
     * @brief Destructor - automatically cleans up LVGL objects
     */
    ~EffectsList();

    /**
     * @brief Move constructor
     */
    EffectsList(EffectsList&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    EffectsList& operator=(EffectsList&& other) noexcept;

    // Disable copy operations to prevent resource issues
    EffectsList(const EffectsList&) = delete;
    EffectsList& operator=(const EffectsList&) = delete;

    /**
     * @brief Initialize the effects roller on the specified parent object
     * @param parent The parent LVGL object (usually a tab)
     * @param visibleRows Number of visible rows in the roller
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(lv_obj_t* parent, int visibleRows = 7);

    /**
     * @brief Set the effect change callback function
     * @param callback Function to call when effect selection changes
     */
    void setCallback(EffectChangeCallback callback);

    /**
     * @brief Set the currently selected effect
     * @param effectIndex Index of the effect to select
     * @param animate Whether to animate the selection change
     */
    void setSelectedEffect(int effectIndex, bool animate = false);

    /**
     * @brief Get the currently selected effect index
     * @return Index of the currently selected effect
     */
    int getSelectedEffect() const;

    /**
     * @brief Check if the effects list is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the underlying LVGL object (for advanced use)
     * @return Pointer to the LVGL roller object, or nullptr if not initialized
     */
    lv_obj_t* getLvglObject() const { return roller_; }

private:
    lv_obj_t* roller_;
    EffectChangeCallback callback_;
    bool initialized_;

    /**
     * @brief Static event handler for LVGL events
     */
    static void eventHandler(lv_event_t* event);

    /**
     * @brief Internal effect change handler
     */
    void handleEffectChange();

    /**
     * @brief Clean up LVGL objects
     */
    void cleanup();

    /**
     * @brief Build the options string for the roller
     */
    String buildOptionsString();
};