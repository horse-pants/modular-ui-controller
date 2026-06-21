#include "VuGraph.h"
#include "modular-ui.h"
#include "ui.h"
#include "AudioBus.h"

VuGraph::VuGraph()
    : canvas_(nullptr)
    , initialized_(false)
{
    // Initialize arrays
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        for (int j = 0; j < SEGMENTS_PER_BAR; j++) {
            segments_[i][j] = nullptr;
        }
        peakSegments_[i] = nullptr;
        vuValues_[i] = 0;
        peakLevels_[i] = 0;
        peakTimers_[i] = 0;
        prevLitSegments_[i] = -1;  // Force initial update
    }
}

VuGraph::~VuGraph() {
    cleanup();
}

VuGraph::VuGraph(VuGraph&& other) noexcept
    : canvas_(other.canvas_)
    , initialized_(other.initialized_)
{
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        for (int j = 0; j < SEGMENTS_PER_BAR; j++) {
            segments_[i][j] = other.segments_[i][j];
            other.segments_[i][j] = nullptr;
        }
        peakSegments_[i] = other.peakSegments_[i];
        other.peakSegments_[i] = nullptr;
        vuValues_[i] = other.vuValues_[i];
        peakLevels_[i] = other.peakLevels_[i];
        peakTimers_[i] = other.peakTimers_[i];
        prevLitSegments_[i] = other.prevLitSegments_[i];
        other.vuValues_[i] = 0;
        other.peakLevels_[i] = 0;
        other.peakTimers_[i] = 0;
        other.prevLitSegments_[i] = -1;
    }

    other.canvas_ = nullptr;
    other.initialized_ = false;
}

VuGraph& VuGraph::operator=(VuGraph&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();

        // Move resources from other
        canvas_ = other.canvas_;
        initialized_ = other.initialized_;

        for (int i = 0; i < NUM_VU_CHANNELS; i++) {
            for (int j = 0; j < SEGMENTS_PER_BAR; j++) {
                segments_[i][j] = other.segments_[i][j];
                other.segments_[i][j] = nullptr;
            }
            peakSegments_[i] = other.peakSegments_[i];
            other.peakSegments_[i] = nullptr;
            vuValues_[i] = other.vuValues_[i];
            peakLevels_[i] = other.peakLevels_[i];
            peakTimers_[i] = other.peakTimers_[i];
            prevLitSegments_[i] = other.prevLitSegments_[i];
            other.vuValues_[i] = 0;
            other.peakLevels_[i] = 0;
            other.peakTimers_[i] = 0;
            other.prevLitSegments_[i] = -1;
        }

        // Reset the moved-from object
        other.canvas_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool VuGraph::initialize(lv_obj_t* parent) {
    if (initialized_) {
        return true; // Already initialized
    }

    if (!parent) {
        return false; // Invalid parent
    }

    try {
        // Create container for VU bars
        canvas_ = lv_obj_create(parent);
        if (!canvas_) {
            return false;
        }

        lv_obj_set_size(canvas_, LV_PCT(95), LV_PCT(90));
        lv_obj_center(canvas_);
        lv_obj_set_style_bg_opa(canvas_, LV_OPA_0, 0);
        lv_obj_set_style_border_opa(canvas_, LV_OPA_0, 0);
        lv_obj_set_style_pad_all(canvas_, 0, 0);
        lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);

        // Calculate centering offset (320px display width)
        int totalWidth = NUM_VU_CHANNELS * SEGMENT_WIDTH + (NUM_VU_CHANNELS - 1) * BAR_SPACING;
        int start_x = (LEFT_ALIGNMENT - totalWidth) / 2;  // Shift left to center properly

        // Create segment grid for each channel
        for (int i = 0; i < NUM_VU_CHANNELS; i++) {
            int bar_x = start_x + i * (SEGMENT_WIDTH + BAR_SPACING);

            // Create segments from bottom to top
            for (int j = 0; j < SEGMENTS_PER_BAR; j++) {
                segments_[i][j] = lv_obj_create(canvas_);
                if (!segments_[i][j]) {
                    cleanup();
                    return false;
                }

                // Position: j=0 is bottom, j=SEGMENTS_PER_BAR-1 is top
                int seg_y = BAR_TOTAL_HEIGHT - (j + 1) * (SEGMENT_HEIGHT + SEGMENT_GAP);

                lv_obj_set_size(segments_[i][j], SEGMENT_WIDTH, SEGMENT_HEIGHT);
                lv_obj_set_pos(segments_[i][j], bar_x, seg_y);

                // Style: dim (unlit) by default
                lv_obj_set_style_bg_color(segments_[i][j], getSegmentColor(j, false), 0);
                lv_obj_set_style_bg_opa(segments_[i][j], LV_OPA_COVER, 0);
                lv_obj_set_style_border_opa(segments_[i][j], LV_OPA_0, 0);
                lv_obj_set_style_radius(segments_[i][j], 2, 0);
                lv_obj_clear_flag(segments_[i][j], LV_OBJ_FLAG_SCROLLABLE);
            }

            // Create peak indicator segment (overlays the peak position)
            peakSegments_[i] = lv_obj_create(canvas_);
            if (!peakSegments_[i]) {
                cleanup();
                return false;
            }

            lv_obj_set_size(peakSegments_[i], SEGMENT_WIDTH, SEGMENT_HEIGHT);
            lv_obj_set_pos(peakSegments_[i], bar_x, BAR_TOTAL_HEIGHT - SEGMENT_HEIGHT);
            lv_obj_set_style_bg_color(peakSegments_[i], lv_color_white(), 0);
            lv_obj_set_style_bg_opa(peakSegments_[i], LV_OPA_0, 0); // Hidden initially
            lv_obj_set_style_border_opa(peakSegments_[i], LV_OPA_0, 0);
            lv_obj_set_style_radius(peakSegments_[i], 2, 0);
            lv_obj_clear_flag(peakSegments_[i], LV_OBJ_FLAG_SCROLLABLE);

            peakLevels_[i] = 0;
            peakTimers_[i] = 0;
            prevLitSegments_[i] = -1;
        }

        // Create frequency labels
        createFrequencyLabels();

        // Audio sampling lives in the audio task now (g_audioBus); nothing to
        // initialise here — this widget only consumes frames.

        initialized_ = true;
        return true;

    } catch (...) {
        cleanup(); // Clean up on any exception
        return false;
    }
}

void VuGraph::update() {
    if (!initialized_) {
        return;
    }

    // Pull the freshest audio frame (published by the audio task) and redraw.
    // No sampling, no filtering here — the frame is already filtered.
    const AudioFrame frame = g_audioBus.latest();
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        vuValues_[i] = frame.bands[i];
    }

    updateVuBars();
}

void VuGraph::centerContentIn(lv_coord_t areaW, lv_coord_t areaH) {
    if (!canvas_) {
        return;
    }

    // Content bounding box in canvas-local coordinates (matches initialize() and
    // createFrequencyLabels()): bars span [start_x, start_x + totalWidth] across
    // and [0, BAR_TOTAL_HEIGHT + label row] down.
    const int totalWidth = NUM_VU_CHANNELS * SEGMENT_WIDTH + (NUM_VU_CHANNELS - 1) * BAR_SPACING;
    const int start_x = (LEFT_ALIGNMENT - totalWidth) / 2;
    const int contentCenterX = start_x + totalWidth / 2;
    const int contentCenterY = (BAR_TOTAL_HEIGHT + VU_LABEL_BAND_HEIGHT) / 2;

    // Override the VU-tab sizing (PCT + centre): make the canvas span the region
    // and shift it so the content's centre lands at the region's centre. The
    // canvas background is transparent, so its overflow off-screen is invisible;
    // the bars stay within the canvas bounds, so nothing is clipped.
    lv_obj_set_align(canvas_, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(canvas_, areaW, areaH);
    lv_obj_set_pos(canvas_, areaW / 2 - contentCenterX, areaH / 2 - contentCenterY);
}

void VuGraph::updateVuBars() {
    if (!initialized_) {
        return;
    }

    unsigned long currentTime = millis();
    const unsigned long PEAK_HOLD_TIME = 500;  // Hold peak for 500ms
    const unsigned long PEAK_DECAY_TIME = 50;  // Decay one segment every 50ms

    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        int level = vuValues_[i];

        // Map level (0-255) to segment count (0-SEGMENTS_PER_BAR)
        int litSegments = map(level, 0, 255, 0, SEGMENTS_PER_BAR);

        // Update peak tracking
        if (litSegments > peakLevels_[i]) {
            peakLevels_[i] = litSegments;
            peakTimers_[i] = currentTime;
        } else if (currentTime - peakTimers_[i] > PEAK_HOLD_TIME) {
            // Decay peak after hold time
            if (currentTime - peakTimers_[i] > PEAK_HOLD_TIME + PEAK_DECAY_TIME) {
                if (peakLevels_[i] > litSegments) {
                    peakLevels_[i]--;
                    peakTimers_[i] = currentTime - PEAK_HOLD_TIME; // Reset decay timer
                }
            }
        }

        // Only update segments that changed
        if (litSegments != prevLitSegments_[i]) {
            int minSeg = (litSegments < prevLitSegments_[i]) ? litSegments : prevLitSegments_[i];
            int maxSeg = (litSegments > prevLitSegments_[i]) ? litSegments : prevLitSegments_[i];
            if (prevLitSegments_[i] < 0) { minSeg = 0; maxSeg = SEGMENTS_PER_BAR; }

            for (int j = minSeg; j < maxSeg && j < SEGMENTS_PER_BAR; j++) {
                if (!segments_[i][j]) continue;
                bool lit = (j < litSegments);
                lv_obj_set_style_bg_color(segments_[i][j], getSegmentColor(j, lit), 0);
            }
            prevLitSegments_[i] = litSegments;
        }

        // Update peak indicator
        if (peakSegments_[i]) {
            if (peakLevels_[i] > 0 && peakLevels_[i] > litSegments) {
                // Show peak indicator
                int peakY = BAR_TOTAL_HEIGHT - peakLevels_[i] * (SEGMENT_HEIGHT + SEGMENT_GAP);
                lv_obj_set_y(peakSegments_[i], peakY);
                lv_obj_set_style_bg_color(peakSegments_[i], getSegmentColor(peakLevels_[i] - 1, true), 0);
                lv_obj_set_style_bg_opa(peakSegments_[i], LV_OPA_COVER, 0);
            } else {
                // Hide peak indicator when it matches current level
                lv_obj_set_style_bg_opa(peakSegments_[i], LV_OPA_0, 0);
            }
        }
    }
}

void VuGraph::cleanup() {
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        for (int j = 0; j < SEGMENTS_PER_BAR; j++) {
            if (segments_[i][j]) {
                lv_obj_del(segments_[i][j]);
                segments_[i][j] = nullptr;
            }
        }
        if (peakSegments_[i]) {
            lv_obj_del(peakSegments_[i]);
            peakSegments_[i] = nullptr;
        }
        vuValues_[i] = 0;
        peakLevels_[i] = 0;
        peakTimers_[i] = 0;
        prevLitSegments_[i] = -1;
    }

    if (canvas_) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }

    initialized_ = false;
}

void VuGraph::createFrequencyLabels() {
    if (!canvas_) {
        return;
    }

    // Calculate centering offset - must match initialize()
    int totalWidth = NUM_VU_CHANNELS * SEGMENT_WIDTH + (NUM_VU_CHANNELS - 1) * BAR_SPACING;
    int start_x = (LEFT_ALIGNMENT - totalWidth) / 2;

    const char* freq_labels[] = {"63", "160", "400", "1K", "2.5K", "6.3K", "16K"};

    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        lv_obj_t* label = lv_label_create(canvas_);
        lv_label_set_text(label, freq_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_PRIMARY), 0);
        int label_x = start_x + i * (SEGMENT_WIDTH + BAR_SPACING);
        lv_obj_set_pos(label, label_x, BAR_TOTAL_HEIGHT + 5);
    }
}

lv_color_t VuGraph::getSegmentColor(int segmentIndex, bool lit) {
    // Segments: 0 = bottom (green), SEGMENTS_PER_BAR-1 = top (red)
    // 10 segments: Green 0-5, Yellow 6-7, Red 8-9

    lv_color_t color;

    if (segmentIndex < 6) {
        // Green zone (bottom 60%)
        color = lit ? lv_color_hex(0x00FF00) : lv_color_hex(0x002200);
    } else if (segmentIndex < 8) {
        // Yellow zone (middle 20%)
        color = lit ? lv_color_hex(0xFFFF00) : lv_color_hex(0x222200);
    } else {
        // Red zone (top 20%)
        color = lit ? lv_color_hex(0xFF0000) : lv_color_hex(0x220000);
    }

    return color;
}