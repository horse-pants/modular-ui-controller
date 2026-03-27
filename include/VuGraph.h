#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include "AudioAnalyzer.h"
#include <Filter.h>

#define NUM_VU_CHANNELS 7
#define SEGMENTS_PER_BAR 10
#define SEGMENT_WIDTH 26
#define SEGMENT_HEIGHT 18
#define SEGMENT_GAP 2
#define BAR_SPACING 10
#define LEFT_ALIGNMENT 260
#define BAR_TOTAL_HEIGHT ((SEGMENT_HEIGHT + SEGMENT_GAP) * SEGMENTS_PER_BAR)

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
    lv_obj_t* segments_[NUM_VU_CHANNELS][SEGMENTS_PER_BAR];
    lv_obj_t* peakSegments_[NUM_VU_CHANNELS];  // Peak hold indicators
    bool initialized_;

    // Audio processing components
    ExponentialFilter<int> filters_[NUM_VU_CHANNELS];
    ExponentialFilter<int> audioFilter_;
    Analyzer audio_;
    int vuValues_[NUM_VU_CHANNELS];
    int audioLevel_;

    // Peak hold tracking
    int peakLevels_[NUM_VU_CHANNELS];
    unsigned long peakTimers_[NUM_VU_CHANNELS];

    // Previous state for change detection
    int prevLitSegments_[NUM_VU_CHANNELS];

    /**
     * @brief Update individual VU segments with current levels
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

    /**
     * @brief Get color for a segment based on its position
     * @param segmentIndex Segment index (0 = bottom, SEGMENTS_PER_BAR-1 = top)
     * @param lit Whether the segment is lit or dim
     * @return Color for the segment
     */
    lv_color_t getSegmentColor(int segmentIndex, bool lit);
};