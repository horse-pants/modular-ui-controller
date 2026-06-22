#pragma once

#include <lvgl.h>
#include <Arduino.h>

#define NUM_VU_CHANNELS 7
#define SEGMENTS_PER_BAR 10
#define SEGMENT_WIDTH 26
#define SEGMENT_HEIGHT 18
#define SEGMENT_GAP 2
#define BAR_SPACING 10
#define LEFT_ALIGNMENT 260
#define BAR_TOTAL_HEIGHT ((SEGMENT_HEIGHT + SEGMENT_GAP) * SEGMENTS_PER_BAR)
// Height of the frequency-label row drawn just below the bars (montserrat_14 at
// BAR_TOTAL_HEIGHT + 5). Used when centring the whole block on the screensaver.
#define VU_LABEL_BAND_HEIGHT 24

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
     * @brief Redraw the VU meter from the latest audio frame (g_audioBus).
     * Called on the render task; pulls data, never samples.
     */
    void update();

    /**
     * @brief Re-centre the meter's content within an @p areaW × @p areaH region.
     *
     * The bars and frequency labels are absolutely positioned for the VU tab's
     * geometry; this repositions the underlying canvas so the whole block sits
     * centred (horizontally and vertically) in a full-screen region. Used by the
     * idle screensaver — the VU tab never calls it, so the tab layout is
     * untouched. Call after initialize().
     */
    void centerContentIn(lv_coord_t areaW, lv_coord_t areaH);

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

private:
    lv_obj_t* canvas_;
    lv_obj_t* segments_[NUM_VU_CHANNELS][SEGMENTS_PER_BAR];
    lv_obj_t* peakSegments_[NUM_VU_CHANNELS];  // Peak hold indicators
    bool initialized_;

    // Latest per-band levels (0-255) pulled from g_audioBus; drawn by updateVuBars
    int vuValues_[NUM_VU_CHANNELS];

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