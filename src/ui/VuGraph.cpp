#include "VuGraph.h"
#include "modular-ui.h"
#include "ui.h"

VuGraph::VuGraph()
    : canvas_(nullptr)
    , initialized_(false)
    , filters_{ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), 
               ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), 
               ExponentialFilter<int>(10, 0)}
    , audioFilter_(10, 0)
    , audio_(13, 21, 12) // Strobe pin ->13  RST pin ->21 Analog Pin ->12
    , audioLevel_(0)
{
    // Initialize arrays
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        vuBars_[i] = nullptr;
        vuValues_[i] = 0;
    }
}

VuGraph::~VuGraph() {
    cleanup();
}

VuGraph::VuGraph(VuGraph&& other) noexcept
    : canvas_(other.canvas_)
    , initialized_(other.initialized_)
    , filters_{ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), 
               ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0), 
               ExponentialFilter<int>(10, 0)}
    , audioFilter_(10, 0) // Re-initialize with default values
    , audio_(13, 21, 12) // Re-initialize with default values
    , audioLevel_(other.audioLevel_)
{
    // Move VU bars array and copy filter state
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        vuBars_[i] = other.vuBars_[i];
        vuValues_[i] = other.vuValues_[i];
        other.vuBars_[i] = nullptr;
        other.vuValues_[i] = 0;
    }
    
    // Reset the moved-from object
    other.canvas_ = nullptr;
    other.initialized_ = false;
    other.audioLevel_ = 0;
}

VuGraph& VuGraph::operator=(VuGraph&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        cleanup();
        
        // Move resources from other
        canvas_ = other.canvas_;
        initialized_ = other.initialized_;
        audioFilter_ = ExponentialFilter<int>(10, 0); // Re-initialize
        audio_ = Analyzer(13, 21, 12); // Re-initialize
        audioLevel_ = other.audioLevel_;
        
        for (int i = 0; i < NUM_VU_CHANNELS; i++) {
            vuBars_[i] = other.vuBars_[i];
            filters_[i] = ExponentialFilter<int>(10, 0); // Re-initialize filters
            vuValues_[i] = other.vuValues_[i];
            other.vuBars_[i] = nullptr;
            other.vuValues_[i] = 0;
        }
        
        // Reset the moved-from object
        other.canvas_ = nullptr;
        other.initialized_ = false;
        other.audioLevel_ = 0;
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
        // Create container for VU bars - use more vertical space
        canvas_ = lv_obj_create(parent);
        if (!canvas_) {
            return false;
        }
        
        lv_obj_set_size(canvas_, LV_PCT(95), LV_PCT(85));
        lv_obj_center(canvas_);
        lv_obj_set_style_bg_opa(canvas_, LV_OPA_0, 0);
        lv_obj_set_style_border_opa(canvas_, LV_OPA_0, 0);
        lv_obj_set_style_pad_all(canvas_, 0, 0);
        lv_obj_clear_flag(canvas_, LV_OBJ_FLAG_SCROLLABLE);
        
        // Calculate total width needed and center offset - use smaller bars to fit
        int start_x = 15;  // Simple left margin
        
        // Create individual bars with simple styling
        for(int i = 0; i < NUM_VU_CHANNELS; i++) {
            vuBars_[i] = lv_obj_create(canvas_);
            if (!vuBars_[i]) {
                cleanup();
                return false;
            }
            
            lv_obj_set_size(vuBars_[i], BAR_WIDTH, 5); // Start with small height
            lv_obj_set_pos(vuBars_[i], start_x + i * (BAR_WIDTH + BAR_SPACING), BAR_HEIGHT + 5);
            
            // Simple bar styling
            lv_obj_set_style_bg_color(vuBars_[i], lv_palette_main(LV_PALETTE_GREEN), 0);
            lv_obj_set_style_border_opa(vuBars_[i], LV_OPA_0, 0);
            lv_obj_set_style_radius(vuBars_[i], 4, 0);
            lv_obj_clear_flag(vuBars_[i], LV_OBJ_FLAG_SCROLLABLE);
        }
        
        // Create frequency labels
        createFrequencyLabels();
        
        // Initialize audio analyzer
        audio_.Init();
        
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
    
    readFrequencies();
    updateVuBars();
    getVuLevels();
    
    // Sync global variables for LED animations
    for (int i = 0; i < NUM_VU_CHANNELS && i < 7; i++) {
        extern int vuValue[7];
        vuValue[i] = vuValues_[i];
    }
    extern int audioLevel;
    audioLevel = audioLevel_;
    
}

int VuGraph::getOverallVolume() {
    int totalVolume = 0;
    for(int i = 0; i < NUM_VU_CHANNELS; i++){
        totalVolume += filters_[i].Current();
    }
    audioFilter_.Filter(totalVolume / NUM_VU_CHANNELS);
    return audioFilter_.Current();
}

void VuGraph::getVuLevels5() {
    for (int i = 0; i < NUM_STRIPS; ++i) {
        if(i == 0){
            int maxVu = max(filters_[i].Current(), filters_[i + 1].Current());
            vuValues_[i] = maxVu;
        }
        else if (i > 0 && i < 4){
            vuValues_[i] = filters_[i + 1].Current();
        }
        else if (i == 4){
            int maxVu = max(filters_[i + 1].Current(), filters_[i + 2].Current());
            vuValues_[i] = maxVu;
        }
    }
}

void VuGraph::getVuLevels3() {
    for (int i = 0; i < NUM_STRIPS; ++i) {
        if(i == 0){
            int maxVu = max(filters_[0].Current(), filters_[1].Current());
            vuValues_[i] = maxVu;
        }
        else if (i == 1){
            int maxVu = max(filters_[2].Current(), filters_[3].Current());
            maxVu = max(maxVu, filters_[4].Current());
            vuValues_[i] = maxVu;
        }
        else if (i == 2){
            int maxVu = max(filters_[5].Current(), filters_[6].Current());
            vuValues_[i] = maxVu;
        }
    }
}

void VuGraph::getVuLevels() {
    getVuLevels5();
}

int VuGraph::getVuValue(int channel) const {
    if (channel >= 0 && channel < NUM_VU_CHANNELS) {
        return vuValues_[channel];
    }
    return 0;
}

void VuGraph::updateVuBars() {
    if (!initialized_) {
        return;
    }
    
    for(int i = 0; i < NUM_VU_CHANNELS; i++) {
        if (!vuBars_[i]) continue;
        
        int level = filters_[i].Current();
        int barHeight = map(level, 0, 255, 1, BAR_HEIGHT);
        
        // Update bar height and position
        lv_obj_set_height(vuBars_[i], barHeight);
        lv_obj_set_y(vuBars_[i], BAR_HEIGHT - barHeight + 10);
        
        // Update colors based on level
        lv_color_t color;
        
        if(level < 85) {
            // Low: Blue to Cyan
            color = (level < 42) ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_CYAN);
        } else if(level < 170) {
            // Medium: Cyan to Green
            color = (level < 127) ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_GREEN);
        } else if(level < 220) {
            // High: Green to Orange
            color = lv_palette_main(LV_PALETTE_ORANGE);
        } else {
            // Peak: Red
            color = lv_palette_main(LV_PALETTE_RED);
        }
        
        lv_obj_set_style_bg_color(vuBars_[i], color, 0);
    }
}

void VuGraph::readFrequencies() {
    if (!initialized_) {
        return;
    }
    
    // Read audio data and update filters
    int freqVal[7];
    audio_.ReadFreq(freqVal);

    for(int i = 0; i < NUM_VU_CHANNELS; i++){
        int mappedValue = map(freqVal[i], 0, 4096, 0, 255);
        filters_[i].Filter(mappedValue);
        // IMPORTANT: Copy filtered values to vuValues_ array for LED animations
        vuValues_[i] = filters_[i].Current();
    } 

    audioLevel_ = getOverallVolume();
    
    // Update LEDManager with new VU levels
    extern LEDManager* g_ledManager;
    if (g_ledManager) {
        g_ledManager->updateVuLevels(vuValues_, audioLevel_);
    }
}

void VuGraph::cleanup() {
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        if (vuBars_[i]) {
            lv_obj_del(vuBars_[i]);
            vuBars_[i] = nullptr;
        }
        vuValues_[i] = 0;
    }
    
    
    if (canvas_) {
        lv_obj_del(canvas_);
        canvas_ = nullptr;
    }
    
    initialized_ = false;
    audioLevel_ = 0;
}

void VuGraph::createFrequencyLabels() {
    if (!canvas_) {
        return;
    }
    
    int start_x = 15;  // Same as bars
    const char* freq_labels[] = {"63", "160", "400", "1K", "2.5K", "6.3K", "16K"};
    
    for (int i = 0; i < NUM_VU_CHANNELS; i++) {
        lv_obj_t* label = lv_label_create(canvas_);
        lv_label_set_text(label, freq_labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_PRIMARY), 0);
        lv_obj_set_pos(label, start_x + i * (BAR_WIDTH + BAR_SPACING) + (BAR_WIDTH/2) - 10, BAR_HEIGHT + 20);
    }
    
}