#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <memory>
#include "BrightnessSlider.h"
#include "ColourWheel.h"
#include "EffectsList.h"
#include "WhiteButton.h"
#include "VuButton.h"
#include "VuGraph.h"

/**
 * @brief Modern C++ class for managing the entire LVGL UI system
 * 
 * This class provides a clean interface for creating and managing the complete
 * UI with proper resource management and initialization.
 * 
 * Features:
 * - RAII resource management
 * - Move semantics support
 * - Complete UI lifecycle management
 * - Automatic cleanup on destruction
 * - Exception safety
 * - Display and touch driver management
 * 
 * @author Claude Code
 */
class UIManager {
public:
    /**
     * @brief Default constructor
     */
    UIManager();

    /**
     * @brief Destructor - automatically cleans up all UI resources
     */
    ~UIManager();

    /**
     * @brief Move constructor
     */
    UIManager(UIManager&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    UIManager& operator=(UIManager&& other) noexcept;

    // Disable copy operations to prevent resource issues
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    /**
     * @brief Initialize the display and touch drivers
     * @return true if initialization was successful, false otherwise
     */
    bool initializeScreen();

    /**
     * @brief Initialize the complete UI system
     * @return true if initialization was successful, false otherwise
     */
    bool initializeUI();

    /**
     * @brief Update the UI (call this regularly in main loop)
     */
    void update();

    /**
     * @brief Apply the current color from color wheel to LEDs
     */
    void applyCurrentColor();

    /**
     * @brief Set VU button state (called from web UI)
     * @param newState Whether VU should be active
     */
    void setVuState(bool newState);

    /**
     * @brief Set white button state (called from web UI)
     * @param newState Whether white mode should be active
     */
    void setWhiteState(bool newState);

    /**
     * @brief Log and update VU state (called from both screen and web)
     * @param newState Whether VU should be active
     */
    void logAndUpdateVuState(bool newState);

    /**
     * @brief Log and update white state (called from both screen and web)
     * @param newState Whether white mode should be active
     */
    void logAndUpdateWhiteState(bool newState);

    /**
     * @brief Set animation state
     * @param newState Whether animations should be active
     */
    void setAnimationState(bool newState);

    /**
     * @brief Set specific animation
     * @param animation Animation index to set
     */
    void setAnimation(int animation);

    /**
     * @brief Check if the UI manager is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the brightness slider instance
     */
    BrightnessSlider* getBrightnessSlider() const { return brightnessSlider_.get(); }

    /**
     * @brief Get the colour wheel instance
     */
    ColourWheel* getColourWheel() const { return colourWheel_.get(); }

    /**
     * @brief Get the effects list instance
     */
    EffectsList* getEffectsList() const { return effectsList_.get(); }

    /**
     * @brief Get the white button instance
     */
    WhiteButton* getWhiteButton() const { return whiteButton_.get(); }

    /**
     * @brief Get the VU button instance
     */
    VuButton* getVuButton() const { return vuButton_.get(); }

    /**
     * @brief Get the VU graph instance
     */
    VuGraph* getVuGraph() const { return vuGraph_.get(); }

    /**
     * @brief Show OTA update screen
     */
    void showOTAScreen();

    /**
     * @brief Update OTA progress on screen
     * @param progress Progress percentage (0-100)
     */
    void updateOTAProgress(uint8_t progress);

    /**
     * @brief Hide OTA update screen
     */
    void hideOTAScreen();

private:
    // UI component instances using smart pointers
    std::unique_ptr<BrightnessSlider> brightnessSlider_;
    std::unique_ptr<ColourWheel> colourWheel_;
    std::unique_ptr<EffectsList> effectsList_;
    std::unique_ptr<WhiteButton> whiteButton_;
    std::unique_ptr<VuButton> vuButton_;
    std::unique_ptr<VuGraph> vuGraph_;

    // LVGL objects
    lv_obj_t* tabview_;
    lv_obj_t* tab1_;  // Colour tab
    lv_obj_t* tab2_;  // Effects tab
    lv_obj_t* tab3_;  // VU tab

    // OTA screen objects
    lv_obj_t* otaScreen_;
    lv_obj_t* otaLabel_;
    lv_obj_t* otaBar_;
    bool otaScreenActive_;
    uint8_t otaPendingProgress_;
    bool otaProgressChanged_;

    bool initialized_;
    bool screenInitialized_;

    /**
     * @brief Apply the synth theme to the screen
     */
    void applySynthTheme();

    /**
     * @brief Create and style the tabview
     */
    bool createTabview();

    /**
     * @brief Initialize all UI components
     */
    bool initializeComponents();

    /**
     * @brief Setup display driver
     */
    void setupDisplayDriver();

    /**
     * @brief Setup touch driver
     */
    void setupTouchDriver();

    /**
     * @brief Clean up all UI resources
     */
    void cleanup();

    /**
     * @brief Static event handler for tab scroll events
     */
    static void scrollBeginEvent(lv_event_t* e);
};