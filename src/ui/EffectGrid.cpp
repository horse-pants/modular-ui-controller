#include "ui/EffectGrid.h"

#include "modular-ui.h"

#include <string.h>

namespace {

// Two-stop colour signature per animation, indexed by AnimationType. Two stops
// rather than a real preview because LV_GRADIENT_MAX_STOPS is 2 — enough to make
// each tile recognisable at a glance, which is the whole point of the grid.
struct Signature { uint32_t from; uint32_t to; };

constexpr Signature kSignature[ANIMATION_COUNT] = {
    { 0xFF0000, 0x8000FF },  // RAINBOW    - red through violet
    { 0x330000, 0xFF2020 },  // CYLON      - the red eye sweeping
    { 0xFF0000, 0x00A0FF },  // RGBCHASER
    { 0x00D9FF, 0x0040A0 },  // BEATSINE
    { 0xFF00C8, 0x2000A0 },  // PLASMA
    { 0x203040, 0xFFFFFF },  // SPARKLE    - white points on dark
    { 0x00E0C0, 0x006080 },  // WAVE
    { 0xFFFFFF, 0x003048 },  // COMET      - bright head, dark tail
    { 0x7FE8FF, 0x2A6BFF },  // ICEWAVES
    { 0xB24BFF, 0x3A1078 },  // PURPLERAIN
    { 0xFF3D00, 0xFFD000 },  // FIRE
    { 0x003B1A, 0x00FF6A },  // MATRIX
    { 0x22C55E, 0xEF4444 },  // VU         - the meter's own green-to-red
    { 0x00D9FF, 0x062430 },  // RIPPLE
    { 0xFF0080, 0xFFD000 },  // CONFETTI
};

// The catalog labels audio-reactive animations with an " (A)" suffix. The grid
// already says AUDIO REACTIVE above them, so the suffix is noise here.
void writeShortName(char* out, size_t cap, const char* full) {
    const char* suffix = strstr(full, " (A)");
    const size_t n = suffix ? (size_t)(suffix - full) : strlen(full);
    const size_t copy = n < cap - 1 ? n : cap - 1;
    memcpy(out, full, copy);
    out[copy] = '\0';
}

void styleSegButton(lv_obj_t* btn) {
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_style_radius(btn, UI_RADIUS_SMALL, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    // Selected page: pre-set so switching is a state toggle, never a restyle.
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_SURFACE_LIGHT), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(btn, lv_color_hex(UI_COLOR_TEXT), LV_STATE_CHECKED);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

uint32_t EffectGrid::signatureColor(int animation) {
    if (animation < 0 || animation >= (int)ANIMATION_COUNT) return 0;
    return kSignature[animation].from;
}

uint32_t EffectGrid::signatureColorEnd(int animation) {
    if (animation < 0 || animation >= (int)ANIMATION_COUNT) return 0;
    return kSignature[animation].to;
}

EffectGrid::~EffectGrid() {
    if (root_) {
        lv_obj_delete(root_);
        root_ = nullptr;
    }
    callback_ = nullptr;
}

bool EffectGrid::initialize(lv_obj_t* parent) {
    if (root_) return true;
    if (!parent) return false;

    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_pad_row(root_, UI_SPACING_NORMAL, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // --- Segmented control: which half of the catalog is on screen ---
    lv_obj_t* seg = lv_obj_create(root_);
    lv_obj_set_size(seg, LV_PCT(100), UI_SEG_HEIGHT);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(seg, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_border_color(seg, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(seg, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(seg, UI_RADIUS_SMALL, 0);
    lv_obj_set_style_pad_all(seg, 2, 0);
    lv_obj_set_style_pad_column(seg, 2, 0);
    lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);

    segPatterns_ = lv_button_create(seg);
    styleSegButton(segPatterns_);
    lv_obj_add_event_cb(segPatterns_, segEvent, LV_EVENT_CLICKED, this);
    lv_obj_t* lblP = lv_label_create(segPatterns_);
    lv_label_set_text(lblP, "PATTERNS");
    lv_obj_center(lblP);

    segAudio_ = lv_button_create(seg);
    styleSegButton(segAudio_);
    lv_obj_add_event_cb(segAudio_, segEvent, LV_EVENT_CLICKED, this);
    lv_obj_t* lblA = lv_label_create(segAudio_);
    lv_label_set_text(lblA, "AUDIO REACTIVE");
    lv_obj_center(lblA);

    // --- The two pages of tiles ---
    for (int pass = 0; pass < 2; ++pass) {
        lv_obj_t* page = lv_obj_create(root_);
        lv_obj_set_width(page, LV_PCT(100));
        lv_obj_set_flex_grow(page, 1);
        lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_set_style_pad_row(page, UI_SPACING_TIGHT, 0);
        lv_obj_set_style_pad_column(page, UI_SPACING_TIGHT, 0);
        // Vertical only: a horizontal drag has to reach the tabview so the tab
        // swipe still works from this page.
        lv_obj_set_scroll_dir(page, LV_DIR_VER);

        const int first = (pass == 0) ? 0 : FIRST_AUDIO;
        const int last  = (pass == 0) ? FIRST_AUDIO : (int)ANIMATION_COUNT;
        for (int i = first; i < last; ++i) {
            tiles_[i] = buildTile(page, i);
        }

        if (pass == 0) pagePatterns_ = page;
        else           pageAudio_ = page;
    }

    // --- "No effect" lives outside the grid: it is not an animation, and as a
    //     tile it would make Patterns nine items and push the page into scroll.
    offBar_ = lv_button_create(root_);
    lv_obj_set_size(offBar_, LV_PCT(100), UI_OFFBAR_HEIGHT);
    lv_obj_set_style_bg_color(offBar_, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_border_color(offBar_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(offBar_, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(offBar_, UI_RADIUS_SMALL, 0);
    lv_obj_set_style_text_color(offBar_, lv_color_hex(UI_COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_bg_color(offBar_, lv_color_hex(UI_COLOR_SURFACE_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(offBar_, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_style_border_color(offBar_, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
    lv_obj_set_style_border_width(offBar_, UI_BORDER_NORMAL, LV_STATE_CHECKED);
    lv_obj_set_style_text_color(offBar_, lv_color_hex(UI_COLOR_TEXT), LV_STATE_CHECKED);
    lv_obj_remove_flag(offBar_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(offBar_, tileEvent, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(offBar_, (void*)(intptr_t)OFF_INDEX);
    lv_obj_t* offLbl = lv_label_create(offBar_);
    lv_label_set_text(offLbl, "EFFECTS OFF");
    lv_obj_center(offLbl);

    showPage(false);
    applySelection();
    return true;
}

lv_obj_t* EffectGrid::buildTile(lv_obj_t* page, int animation) {
    lv_obj_t* tile = lv_button_create(page);
    // Two per row: half the page minus half the column gap.
    lv_obj_set_size(tile, LV_PCT(48), UI_TILE_HEIGHT);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_style_radius(tile, UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COLOR_SURFACE_LIGHT), 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(tile, UI_BORDER_NORMAL, 0);
    lv_obj_set_style_clip_corner(tile, true, 0);
    // Active: a thick bright border, per the theme's active-state convention.
    lv_obj_set_style_border_color(tile, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_COLOR_SURFACE_ACTIVE), LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_user_data(tile, (void*)(intptr_t)animation);
    lv_obj_add_event_cb(tile, tileEvent, LV_EVENT_CLICKED, this);

    // The signature strip: what the effect looks like, in one band.
    lv_obj_t* strip = lv_obj_create(tile);
    lv_obj_set_size(strip, LV_PCT(100), UI_TILE_STRIP_HEIGHT);
    lv_obj_align(strip, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(strip, 0, 0);
    lv_obj_set_style_border_width(strip, 0, 0);
    lv_obj_set_style_pad_all(strip, 0, 0);
    lv_obj_set_style_bg_color(strip, lv_color_hex(kSignature[animation].from), 0);
    lv_obj_set_style_bg_grad_color(strip, lv_color_hex(kSignature[animation].to), 0);
    lv_obj_set_style_bg_grad_dir(strip, LV_GRAD_DIR_HOR, 0);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    char name[24];
    writeShortName(name, sizeof(name), animationDescription((AnimationType)animation));

    lv_obj_t* label = lv_label_create(tile);
    lv_label_set_text(label, name);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, UI_TILE_STRIP_HEIGHT / 2);

    return tile;
}

void EffectGrid::tileEvent(lv_event_t* event) {
    auto* self = static_cast<EffectGrid*>(lv_event_get_user_data(event));
    if (!self) return;

    lv_obj_t* target = lv_event_get_target_obj(event);
    const int animation = (int)(intptr_t)lv_obj_get_user_data(target);

    self->selected_ = animation;
    self->applySelection();

    if (self->callback_) {
        self->callback_(animation);
    }
}

void EffectGrid::segEvent(lv_event_t* event) {
    auto* self = static_cast<EffectGrid*>(lv_event_get_user_data(event));
    if (!self) return;
    self->showPage(lv_event_get_target_obj(event) == self->segAudio_);
}

void EffectGrid::showPage(bool audio) {
    showingAudio_ = audio;

    if (audio) {
        lv_obj_add_flag(pagePatterns_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(pageAudio_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(segAudio_, LV_STATE_CHECKED);
        lv_obj_remove_state(segPatterns_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_flag(pagePatterns_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pageAudio_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_state(segPatterns_, LV_STATE_CHECKED);
        lv_obj_remove_state(segAudio_, LV_STATE_CHECKED);
    }
}

void EffectGrid::setSelected(int animation) {
    if (animation < 0 || animation >= (int)ANIMATION_COUNT) animation = OFF_INDEX;
    selected_ = animation;
    if (!root_) return;

    // Follow the selection to the page it lives on, so a change pushed from the
    // web or MQTT is visible rather than hidden behind the other segment.
    if (animation != OFF_INDEX) {
        showPage(animation >= FIRST_AUDIO);
    }
    applySelection();
}

void EffectGrid::applySelection() {
    for (int i = 0; i < (int)ANIMATION_COUNT; ++i) {
        if (!tiles_[i]) continue;
        if (i == selected_) lv_obj_add_state(tiles_[i], LV_STATE_CHECKED);
        else                lv_obj_remove_state(tiles_[i], LV_STATE_CHECKED);
    }
    if (offBar_) {
        if (selected_ == OFF_INDEX) lv_obj_add_state(offBar_, LV_STATE_CHECKED);
        else                        lv_obj_remove_state(offBar_, LV_STATE_CHECKED);
    }
}
