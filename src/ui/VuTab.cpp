#include "ui/VuTab.h"
#include "ui/ui.h"

VuTab::~VuTab() {
    g_vuGraph = nullptr;
}

bool VuTab::build(lv_obj_t* parent) {
    vuGraph_.reset(new VuGraph());
    g_vuGraph = vuGraph_.get();
    return vuGraph_ && vuGraph_->initialize(parent);
}

void VuTab::update() {
    if (vuGraph_) {
        vuGraph_->update();
    }
}
