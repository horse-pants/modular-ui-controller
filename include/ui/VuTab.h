#pragma once

#include <lvgl.h>
#include <memory>
#include "ui/VuGraph.h"

/**
 * @brief The "VU" tab — hosts the VU meter graph.
 *
 * Thin view wrapper around the VuGraph component: builds it into the tab page
 * and ticks it each frame. Publishes g_vuGraph on build(), clears it on destroy.
 */
class VuTab {
public:
    ~VuTab();

    /// Build the VU graph into @p parent (the VU tab page).
    bool build(lv_obj_t* parent);

    /// Tick the graph (called from UIManager::update when visible).
    void update();

private:
    std::unique_ptr<VuGraph> vuGraph_;
};
