#pragma once

#include <lvgl.h>
#include <functional>
#include "led/Animations.h"

/**
 * @brief The animation picker: a grid of tappable tiles, one per animation.
 *
 * Replaces the dropdown that used to sit on the Colour tab. A dropdown is the
 * wrong control for fifteen options on a touch panel — it hides every choice
 * behind a tap, then makes you read a scrolling list of words. Here every
 * animation is on screen at once, thumb-sized, and each tile carries a colour
 * strip that previews what the effect actually looks like, so Fire is found by
 * its orange rather than by reading.
 *
 * Patterns and audio-reactive animations are split by a segmented control. That
 * is what makes it fit: 8 and 7 tiles are four rows each, so neither page
 * scrolls on a 320x480 panel.
 *
 * Selection is reported through the callback; the caller owns the LED side.
 * OFF_INDEX means "no animation" (the bar under the grid).
 */
class EffectGrid {
public:
    /// @param animation an AnimationType index, or OFF_INDEX for none
    using SelectCallback = std::function<void(int animation)>;

    static constexpr int OFF_INDEX = -1;

    ~EffectGrid();

    EffectGrid(const EffectGrid&) = delete;
    EffectGrid& operator=(const EffectGrid&) = delete;
    EffectGrid() = default;

    /// Build the picker into @p parent (the Effects tab page).
    bool initialize(lv_obj_t* parent);

    void setCallback(SelectCallback callback) { callback_ = callback; }

    /// Reflect the current selection. Never fires the callback.
    void setSelected(int animation);

    int getSelected() const { return selected_; }

    /// The tile strip's two gradient stops for @p animation, so the Colour tab's
    /// effect bar and the web UI can show the same signature. 0 out of range.
    static uint32_t signatureColor(int animation);
    static uint32_t signatureColorEnd(int animation);

    bool isInitialized() const { return root_ != nullptr; }

private:
    // Index of the first audio-reactive animation — the split point for the two
    // pages. Everything from here to ANIMATION_COUNT needs the mic.
    static constexpr int FIRST_AUDIO = ICEWAVES;

    static void tileEvent(lv_event_t* event);
    static void segEvent(lv_event_t* event);

    lv_obj_t* buildTile(lv_obj_t* page, int animation);
    void showPage(bool audio);
    void applySelection();

    lv_obj_t* root_ = nullptr;
    lv_obj_t* segPatterns_ = nullptr;
    lv_obj_t* segAudio_ = nullptr;
    lv_obj_t* pagePatterns_ = nullptr;
    lv_obj_t* pageAudio_ = nullptr;
    lv_obj_t* offBar_ = nullptr;
    lv_obj_t* tiles_[ANIMATION_COUNT] = { nullptr };

    int  selected_ = OFF_INDEX;
    bool showingAudio_ = false;

    SelectCallback callback_;
};
