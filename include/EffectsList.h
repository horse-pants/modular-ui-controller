#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <functional>

/**
 * @brief Dropdown button for selecting LED effects
 *
 * Compact dropdown widget for effect selection.
 * Layout handled by parent container.
 */
class EffectsList {
public:
    using EffectChangeCallback = std::function<void(int effectIndex)>;

    EffectsList();
    ~EffectsList();

    EffectsList(EffectsList&& other) noexcept;
    EffectsList& operator=(EffectsList&& other) noexcept;

    EffectsList(const EffectsList&) = delete;
    EffectsList& operator=(const EffectsList&) = delete;

    /**
     * @brief Initialize the dropdown on the specified parent
     * @param parent The parent LVGL object (layout handled by parent)
     * @return true if initialization was successful
     */
    bool initialize(lv_obj_t* parent);

    void setCallback(EffectChangeCallback callback);
    void setSelectedEffect(int effectIndex, bool animate = false);
    int getSelectedEffect() const;

    /**
     * @brief Set the active/inactive visual state
     * @param active true when animations are running, false when color wheel is active
     */
    void setActiveState(bool active);

    bool isInitialized() const { return initialized_; }
    lv_obj_t* getLvglObject() const { return dropdown_; }

private:
    lv_obj_t* dropdown_;
    EffectChangeCallback callback_;
    bool initialized_;
    bool activeState_;

    static void eventHandler(lv_event_t* event);
    static void openHandler(lv_event_t* event);
    void handleEffectChange();
    void cleanup();
    String buildOptionsString();
};