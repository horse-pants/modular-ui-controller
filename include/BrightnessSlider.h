#pragma once

#include <lvgl.h>
#include <functional>

/**
 * @brief Brightness slider class for controlling LED brightness
 * 
 * This class manages a styled brightness slider widget with proper
 * encapsulation and event handling using callbacks.
 */
class BrightnessSlider {
public:
    /// Callback function type for brightness change events
    using BrightnessCallback = std::function<void(int brightness)>;

private:
    lv_obj_t* slider_;              ///< The slider widget
    lv_obj_t* label_;               ///< The brightness label
    lv_obj_t* parentTab_;           ///< Parent tab container
    bool initialized_;              ///< Initialization status
    int currentBrightness_;         ///< Current brightness value
    int maxBrightness_;             ///< Maximum brightness value
    BrightnessCallback callback_;   ///< Callback for brightness changes
    
    // Static styles for the slider components
    static lv_style_t indicatorStyle_;
    static lv_style_t knobStyle_;
    static bool stylesInitialized_;
    
    /// Initialize static styles (called once)
    static void initializeStyles();
    
    /// Create and style the slider widget
    void createSlider();
    
    /// Create and style the label
    void createLabel();
    
    /// Apply styling to the slider
    void applySliderStyling();
    
    /// Static event handler wrapper for LVGL
    static void eventHandlerWrapper(lv_event_t* e);
    
    /// Instance event handler
    void handleSliderEvent(lv_event_t* e);

public:
    /**
     * @brief Constructor
     * 
     * @param maxBrightness Maximum brightness value (default: 255)
     */
    explicit BrightnessSlider(int maxBrightness = 255);
    
    /**
     * @brief Destructor - cleans up LVGL objects
     */
    ~BrightnessSlider();
    
    // Delete copy constructor and copy assignment operator
    BrightnessSlider(const BrightnessSlider&) = delete;
    BrightnessSlider& operator=(const BrightnessSlider&) = delete;
    
    // Allow move constructor and move assignment operator
    BrightnessSlider(BrightnessSlider&& other) noexcept;
    BrightnessSlider& operator=(BrightnessSlider&& other) noexcept;
    
    /**
     * @brief Initialize the brightness slider on a parent tab
     * 
     * @param parentTab The parent tab container
     * @param x X position relative to parent
     * @param y Y position relative to parent
     * @param initialBrightness Initial brightness value
     * @return true if initialization successful, false otherwise
     */
    bool initialize(lv_obj_t* parentTab, int x = 20, int y = 10, int initialBrightness = 100);
    
    /**
     * @brief Set brightness value programmatically
     * 
     * @param brightness New brightness value (0 to maxBrightness)
     * @param animate Whether to animate the change
     * @param triggerCallback Whether to fire the callback (for external updates)
     */
    void setBrightness(int brightness, bool animate = true, bool triggerCallback = false);
    
    /**
     * @brief Get current brightness value
     * 
     * @return Current brightness value
     */
    int getBrightness() const { return currentBrightness_; }
    
    /**
     * @brief Get brightness value directly from slider widget
     * 
     * @return Current slider value, or 0 if not initialized
     */
    int getSliderValue() const;
    
    /**
     * @brief Sync internal brightness value with slider widget value
     * 
     * Useful for ensuring consistency after external modifications
     */
    void syncFromSlider();
    
    /**
     * @brief Get maximum brightness value
     * 
     * @return Maximum brightness value
     */
    int getMaxBrightness() const { return maxBrightness_; }
    
    /**
     * @brief Set callback for brightness change events
     * 
     * @param callback Function to call when brightness changes
     */
    void setCallback(BrightnessCallback callback) { callback_ = callback; }
    
    /**
     * @brief Check if the slider is initialized
     * 
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * @brief Get the slider widget pointer (for advanced use)
     * 
     * @return Pointer to the slider widget, nullptr if not initialized
     */
    lv_obj_t* getSliderWidget() const { return slider_; }
    
    /**
     * @brief Clean up and destroy the slider
     * 
     * Removes all LVGL objects.
     * Called automatically by destructor.
     */
    void cleanup();
};