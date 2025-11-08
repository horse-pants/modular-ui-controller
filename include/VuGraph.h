#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include "AudioAnalyzer.h"
#include <Filter.h>

#define NUM_VU_CHANNELS 7
#define BAR_WIDTH 35
#define BAR_HEIGHT 280
#define BAR_SPACING 10

/**
 * @brief Modern C++ class for managing an LVGL VU meter graph widget
 * 
 * This class provides a clean interface for creating and managing a VU
 * meter with multiple frequency bands and proper resource management.
 * 
 * Features:
 * - RAII resource management
 * - Move semantics support
 * - Real-time audio visualization
 * - Automatic cleanup on destruction
 * - Exception safety
 * 
 * @author Claude Code
 */
class VuGraph {
public:
    /**
     * @brief Default constructor
     */
    VuGraph();

    /**
     * @brief Destructor - automatically cleans up LVGL objects
     */
    ~VuGraph();

    /**
     * @brief Move constructor
     */
    VuGraph(VuGraph&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    VuGraph& operator=(VuGraph&& other) noexcept;

    // Disable copy operations to prevent resource issues
    VuGraph(const VuGraph&) = delete;
    VuGraph& operator=(const VuGraph&) = delete;

    /**
     * @brief Initialize the VU graph on the specified parent object
     * @param parent The parent LVGL object (usually a tab)
     * @return true if initialization was successful, false otherwise
     */
    bool initialize(lv_obj_t* parent);

    /**
     * @brief Update the VU meter with new audio data
     * Should be called regularly to refresh the display
     */
    void update();

    /**
     * @brief Get the overall audio volume level
     * @return Current volume level (0-255)
     */
    int getOverallVolume();

    /**
     * @brief Get VU levels for LED strips (5 channels)
     */
    void getVuLevels5();

    /**
     * @brief Get VU levels for LED strips (3 channels)
     */
    void getVuLevels3();

    /**
     * @brief Get VU levels using current configuration
     */
    void getVuLevels();

    /**
     * @brief Check if the VU graph is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the underlying LVGL canvas object (for advanced use)
     * @return Pointer to the LVGL canvas object, or nullptr if not initialized
     */
    lv_obj_t* getLvglObject() const { return canvas_; }

    /**
     * @brief Get current VU value for a specific channel
     * @param channel Channel index (0-6)
     * @return VU value for the channel
     */
    int getVuValue(int channel) const;

private:
    lv_obj_t* canvas_;
    lv_obj_t* vuBars_[NUM_VU_CHANNELS];
    bool initialized_;
    
    // Audio processing components
    ExponentialFilter<int> filters_[NUM_VU_CHANNELS];
    ExponentialFilter<int> audioFilter_;
    Analyzer audio_;
    int vuValues_[NUM_VU_CHANNELS];
    int audioLevel_;

    /**
     * @brief Update individual VU bars with current levels
     */
    void updateVuBars();

    /**
     * @brief Read frequency data from audio analyzer
     */
    void readFrequencies();

    /**
     * @brief Clean up LVGL objects
     */
    void cleanup();

    /**
     * @brief Create frequency labels for the bars
     */
    void createFrequencyLabels();
};