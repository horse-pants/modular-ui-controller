#include "ui/UIManager.h"
#include "ui/Display.h"
#include "modular-ui.h"
#include "ui/ui.h"
#include <memory>
#include <Preferences.h>
#include <Logger.h>

// Global instance definitions
UIManager* g_uiManager = nullptr;
BrightnessSlider* g_brightnessSlider = nullptr;
ColourWheel* g_colourWheel = nullptr;
EffectGrid* g_effectGrid = nullptr;
ColourTab* g_colourTab = nullptr;
WhiteButton* g_whiteButton = nullptr;
VuButton* g_vuButton = nullptr;
VuGraph* g_vuGraph = nullptr;

// LVGL v9 pulls elapsed time from this callback (replaces v8's compile-time
// LV_TICK_CUSTOM), so the tick source is task-agnostic and no longer depends on
// loop() calling lv_tick_inc(). Wrapped rather than passing millis() directly
// because millis() returns `unsigned long`, not `uint32_t`, and the function
// pointer types must match exactly.
static uint32_t lvglTickCallback() {
    return millis();
}

UIManager::UIManager()
    : colourTab_(nullptr)
    , effectsTab_(nullptr)
    , vuTab_(nullptr)
    , audioVisualiser_(nullptr)
    , tabview_(nullptr)
    , tab1_(nullptr)
    , tab2_(nullptr)
    , tab3_(nullptr)
    , initialized_(false)
    , screenInitialized_(false)
    , screensaverEnabled_(true)
    , screensaverIdleMs_(30000)
    , uiCommandQueue_(nullptr)
    , renderTaskHandle_(nullptr)
{
    // Created up front so web handlers can post before the full UI is built.
    uiCommandQueue_ = xQueueCreate(16, sizeof(UiCommand));
}

UIManager::~UIManager() {
    // Stop the render task before tearing down any lv_* objects, or it could
    // touch freed widgets mid-flush.
    if (renderTaskHandle_) {
        vTaskDelete(renderTaskHandle_);
        renderTaskHandle_ = nullptr;
    }
    cleanup();
    if (uiCommandQueue_) {
        vQueueDelete(uiCommandQueue_);
        uiCommandQueue_ = nullptr;
    }
}

bool UIManager::initializeScreen() {
    if (screenInitialized_) {
        return true; // Already initialized
    }

    try {
        // Hardware panel first, then LVGL core, then wire the two together.
        Display::initPanel();
        lv_init();

        // Task-agnostic tick source (no more lv_tick_inc in loop())
        lv_tick_set_cb(lvglTickCallback);

        Display::setupLvglDisplay();
        Display::setupLvglTouch();

        screenInitialized_ = true;
        return true;

    } catch (...) {
        return false;
    }
}

bool UIManager::initializeUI() {
    if (initialized_) {
        return true; // Already initialized
    }
    
    if (!screenInitialized_ && !initializeScreen()) {
        return false; // Screen initialization failed
    }
    
    try {
        // Apply the synth theme first
        applySynthTheme();
        
        // Create tabview
        if (!createTabview()) {
            return false;
        }
        
        // Initialize all components
        if (!initializeComponents()) {
            cleanup();
            return false;
        }
        
        initialized_ = true;
        logLvglMemory("after UI build");
        return true;
        
    } catch (...) {
        cleanup();
        return false;
    }
}

// LVGL allocates objects, styles AND its draw layers from one fixed pool
// (LV_MEM_SIZE). Overrunning it trips LV_ASSERT_MALLOC, whose handler is
// `while(1)` — which halts the render task silently: no panic, no reboot, the
// web server carries on, and the screen simply stops updating. That cost a long
// debugging session once, so the headroom is logged rather than assumed.
void UIManager::logLvglMemory(const char* when) {
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_BUILTIN
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    // max_used is the number that matters: the build-time figure misses the draw
    // layers LVGL allocates while rendering, and those are what overran the pool.
    // Free internal heap is here too because LV_MEM_SIZE is a static array — every
    // KB given to LVGL is a KB taken from WiFi, AsyncTCP and MQTT.
    Logger.info("LVGL heap %s: %u%% now, peak %u KB of %u KB, largest free %u KB, "
                "frag %u%% | internal heap free %u KB",
                when,
                (unsigned)mon.used_pct,
                (unsigned)(mon.max_used / 1024),
                (unsigned)(mon.total_size / 1024),
                (unsigned)(mon.free_biggest_size / 1024),
                (unsigned)mon.frag_pct,
                (unsigned)(ESP.getFreeHeap() / 1024));
    if (mon.used_pct > 80) {
        Logger.warning("LVGL heap above 80%% - raise LV_MEM_SIZE in include/lv_conf.h "
                       "before adding more widgets");
    }
#else
    (void)when;
#endif
}

void UIManager::update() {
    if (!initialized_) {
        return;
    }

    // One-shot once everything has settled. The boot report is taken before the
    // first frame, so it cannot show the render-time peak.
    if (!heapSettledLogged_ && millis() > 20000) {
        heapSettledLogged_ = true;
        logLvglMemory("settled");
    }

    // Apply any UI mutations queued from other tasks (web handlers) before we
    // touch LVGL, so all lv_* mutation originates on this single task.
    drainUiCommands();

    // Handle OTA screen updates (must run on the render task for LVGL safety).
    if (otaScreen_.changed()) {
        // The OTA screen lives under the top-layer screensaver — dismiss the
        // screensaver so the progress is visible.
        if (otaScreen_.isActive() && audioVisualiser_ && audioVisualiser_->isActive()) {
            audioVisualiser_->hide();
        }
        otaScreen_.apply();
    }

    // Idle screensaver: after screensaverIdleMs_ with no screen touch, reveal the
    // full-screen visualiser; while it's up, draw it (and skip the VU meter
    // underneath, which is covered). A tap dismisses it (handled by the overlay).
    // The feature can be disabled from the web Settings page (screensaverEnabled_).
    if (audioVisualiser_) {
        if (audioVisualiser_->isActive()) {
            // Live-dismiss if it was disabled from the web while showing.
            if (!screensaverEnabled_) {
                audioVisualiser_->hide();
            } else {
                audioVisualiser_->update();
            }
        } else {
            if (vuTab_) {
                vuTab_->update();
            }
            // Don't kick in when disabled, or over the OTA progress screen.
            if (screensaverEnabled_ && !otaScreen_.isActive() &&
                (millis() - Display::lastTouchMs()) > screensaverIdleMs_) {
                audioVisualiser_->show();
            }
        }
    } else if (vuTab_) {
        vuTab_->update();
    }

    // Process LVGL tasks
    lv_timer_handler();
}

void UIManager::postUiCommand(const UiCommand& cmd) {
    if (!uiCommandQueue_) {
        return;
    }
    // Non-blocking: never stall the AsyncTCP task. Drop on overflow.
    xQueueSend(uiCommandQueue_, &cmd, 0);
}

void UIManager::drainUiCommands() {
    if (!uiCommandQueue_) {
        return;
    }
    UiCommand cmd;
    while (xQueueReceive(uiCommandQueue_, &cmd, 0) == pdTRUE) {
        applyUiCommand(cmd);
    }
}

void UIManager::applyUiCommand(const UiCommand& cmd) {
    switch (cmd.type) {
        case UiCommandType::SetVu:
            if (colourTab_) colourTab_->setVuState(cmd.boolValue);
            updateWebUi();
            break;

        case UiCommandType::SetWhite:
            if (colourTab_) colourTab_->setWhiteState(cmd.boolValue);
            updateWebUi();
            break;

        case UiCommandType::SetBrightness:
            if (g_brightnessSlider) {
                // Trigger callback to update global state and notify clients
                g_brightnessSlider->setBrightness(cmd.intValue, true, true);
            } else {
                if (g_ledManager) {
                    g_ledManager->setBrightness(cmd.intValue);
                }
                updateWebUi();
            }
            break;

        case UiCommandType::SetAnimation:
            if (colourTab_) {
                if (cmd.boolValue) {
                    colourTab_->setAnimation(cmd.intValue);
                }
                colourTab_->setAnimationState(cmd.boolValue);
            }
            updateWebUi();
            break;

        case UiCommandType::SetColour:
            if (g_colourWheel) {
                g_colourWheel->setColor(String(cmd.colour));
            }
            break;

        case UiCommandType::None:
        default:
            break;
    }
}

void UIManager::renderTaskTrampoline(void* arg) {
    UIManager* self = static_cast<UIManager*>(arg);
    // ~200 Hz cap: smooth UI + keeps the VU read (still in update() until step 4)
    // responsive, while yielding the core to loopTask (LED/web/OTA) each pass.
    const TickType_t period = pdMS_TO_TICKS(5);
    for (;;) {
        self->update();
        vTaskDelay(period);
    }
}

void UIManager::startRenderTask() {
    if (renderTaskHandle_) {
        return;  // already running
    }
    // Stack in BYTES (ESP-IDF FreeRTOS). LVGL render + LovyanGFX flush + the VU
    // read fit comfortably under the old 8 KB loopTask budget; 10 KB gives
    // headroom — watch uxTaskGetStackHighWaterMark on long runs.
    // Priority 2 > Arduino loopTask (1) so the render task preempts it. Core 1.
    BaseType_t ok = xTaskCreatePinnedToCore(
        renderTaskTrampoline,
        "lvgl_render",
        10240,
        this,
        2,
        &renderTaskHandle_,
        1);
    if (ok != pdPASS) {
        renderTaskHandle_ = nullptr;
        Logger.error("Failed to create LVGL render task");
    } else {
        Logger.info("LVGL render task started (core 1, prio 2)");
    }
}

// These delegate to the Colour tab (which owns the widgets + control logic).
// Kept on UIManager because the VuButton / WhiteButton widgets call back through
// g_uiManager, and the web command queue dispatches here.
void UIManager::applyCurrentColor() {
    if (colourTab_) colourTab_->applyCurrentColor();
}

void UIManager::logAndUpdateVuState(bool newState) {
    if (colourTab_) colourTab_->logAndUpdateVuState(newState);
}

void UIManager::logAndUpdateWhiteState(bool newState) {
    if (colourTab_) colourTab_->logAndUpdateWhiteState(newState);
}

void UIManager::setVuState(bool newState) {
    if (colourTab_) colourTab_->setVuState(newState);
}

void UIManager::setWhiteState(bool newState) {
    if (colourTab_) colourTab_->setWhiteState(newState);
}

void UIManager::setAnimationState(bool newState) {
    if (colourTab_) colourTab_->setAnimationState(newState);
}

void UIManager::setAnimation(int animation) {
    if (colourTab_) colourTab_->setAnimation(animation);
    if (effectsTab_) effectsTab_->setSelected(animation);
}

void UIManager::syncWithLEDState() {
    if (colourTab_) colourTab_->syncWithLed();
    if (effectsTab_) effectsTab_->syncWithLed();
}

void UIManager::applySynthTheme() {
    // Apply dark synth theme to screen background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_grad_color(lv_scr_act(), lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_grad_dir(lv_scr_act(), LV_GRAD_DIR_VER, 0);
}

bool UIManager::createTabview() {
    // Create a Tab view object
    tabview_ = lv_tabview_create(lv_scr_act());
    if (!tabview_) {
        return false;
    }

    // Set tab bar position and size
    lv_tabview_set_tab_bar_position(tabview_, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview_, 50);
    
    lv_obj_t* tabContent = lv_tabview_get_content(tabview_);
    lv_obj_clear_flag(tabContent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tabContent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(tabContent, scrollBeginEvent, LV_EVENT_SCROLL_BEGIN, NULL);

    // Style the tabview with synth theme - hardware module look
    lv_obj_set_style_bg_color(tabview_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(tabview_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tabview_, UI_BORDER_NORMAL, 0);
    lv_obj_set_style_border_color(tabview_, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(tabview_, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tabview_, UI_RADIUS_MEDIUM, 0);
    lv_obj_add_flag(tabview_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    // Three tabs, one horizontal swipe apart. Effects has its own page now: as
    // a dropdown on the Colour tab it was the loudest control on screen and the
    // least used, and it left fifteen animations behind a scrolling word list.
    tab1_ = lv_tabview_add_tab(tabview_, "Colour");
    tab2_ = lv_tabview_add_tab(tabview_, "Effects");
    tab3_ = lv_tabview_add_tab(tabview_, "VU");

    if (!tab1_ || !tab2_ || !tab3_) {
        return false;
    }

    // Style individual tabs
    lv_obj_set_style_bg_color(tab1_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(tab2_, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(tab3_, lv_color_hex(UI_COLOR_SURFACE), 0);
    
    // === TAB BAR - Hardware selector style ===
    lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview_);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(tab_bar, UI_BORDER_NORMAL, 0);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(tab_bar, UI_PADDING_SMALL, 0);

    // Style each tab button
    uint32_t child_count = lv_obj_get_child_count(tab_bar);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* btn = lv_obj_get_child(tab_bar, i);

        // === INACTIVE STATE - Light text on dark background ===
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_SURFACE), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(UI_COLOR_TEXT), 0);  // Bright white text
        lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_BORDER), 0);
        lv_obj_set_style_border_width(btn, UI_BORDER_THIN, 0);
        lv_obj_set_style_radius(btn, UI_RADIUS_SMALL, 0);

        // === ACTIVE/CHECKED STATE - Cyan background, white text ===
        lv_obj_set_style_bg_color(btn, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_hex(UI_COLOR_WHITE), LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(UI_COLOR_PRIMARY), LV_STATE_CHECKED);
    }
    
    return true;
}

bool UIManager::initializeComponents() {
    // Colour control tab (fader, VU toggle, wheel, white, effects).
    colourTab_ = std::make_unique<ColourTab>();
    if (!colourTab_->build(tab1_)) {
        return false;
    }

    // Effects tab — the animation picker.
    effectsTab_ = std::make_unique<EffectsTab>();
    if (!effectsTab_->build(tab2_)) {
        return false;
    }

    // VU meter tab.
    vuTab_ = std::make_unique<VuTab>();
    if (!vuTab_->build(tab3_)) {
        return false;
    }

    // ==========================================================================
    // AUDIO VISUALISER — full-screen idle screensaver on the top layer (not a
    // tab). Manages g_audioVisualiser itself. The canvas (PSRAM) is allocated
    // lazily the first time it's shown.
    // ==========================================================================
    audioVisualiser_.reset(new AudioVisualiser());
    if (audioVisualiser_ && !audioVisualiser_->initialize(lv_layer_top())) {
        return false;
    }

    // Apply the saved screensaver preferences (enable + idle timeout).
    loadScreensaverConfig();

    return true;
}

void UIManager::loadScreensaverConfig() {
    Preferences prefs;
    prefs.begin("ui-settings", true);  // read-only
    screensaverEnabled_ = prefs.getBool("scrn_en", true);
    uint32_t idleSec = prefs.getUInt("scrn_sec", 30);
    screensaverScene_ = prefs.getInt("scrn_scene", 1);   // default: first scene
    prefs.end();

    if (idleSec < 5) idleSec = 5;  // clamp to a sane minimum
    screensaverIdleMs_ = idleSec * 1000;

    // Apply the saved choice now the visualiser exists (this runs after it is
    // built in initializeComponents()).
    if (audioVisualiser_) {
        audioVisualiser_->setSelection(screensaverScene_);
    }

    Logger.info("Screensaver: %s, idle %us, scene %d (%s)",
                screensaverEnabled_ ? "enabled" : "disabled", (unsigned)idleSec,
                (int)screensaverScene_, screensaverSceneName(screensaverScene_));
}

void UIManager::setScreensaverConfig(bool enabled, uint32_t idleMs) {
    if (idleMs < 5000) idleMs = 5000;  // clamp to a sane minimum
    screensaverEnabled_ = enabled;
    screensaverIdleMs_ = idleMs;

    // Persist (NVS stores the timeout in whole seconds).
    Preferences prefs;
    prefs.begin("ui-settings", false);
    prefs.putBool("scrn_en", enabled);
    prefs.putUInt("scrn_sec", idleMs / 1000);
    prefs.end();

    Logger.info("Screensaver config saved: %s, idle %ums",
                enabled ? "enabled" : "disabled", (unsigned)idleMs);
}

void UIManager::setScreensaverScene(int index) {
    if (index < 0) index = 0;
    screensaverScene_ = index;

    if (audioVisualiser_) {
        audioVisualiser_->setSelection(index);
    }

    Preferences prefs;
    prefs.begin("ui-settings", false);
    prefs.putInt("scrn_scene", index);
    prefs.end();

    Logger.info("Screensaver scene saved: %d (%s)", index, screensaverSceneName(index));
}

int UIManager::screensaverSceneCount() const {
    return audioVisualiser_ ? audioVisualiser_->selectionCount() : 0;
}

const char* UIManager::screensaverSceneName(int index) const {
    return audioVisualiser_ ? audioVisualiser_->selectionName(index) : "none";
}

void UIManager::cleanup() {
    // Reset the view components before deleting the tabview, so each tab's
    // widgets (and their containers) are gone before the parent tab is. Each
    // tab's destructor also nulls the g_* widget globals it published;
    // audioVisualiser_ nulls its own g_audioVisualiser + frees its canvas.
    colourTab_.reset();
    vuTab_.reset();
    audioVisualiser_.reset();

    // Clean up LVGL objects
    if (tabview_) {
        lv_obj_del(tabview_);
        tabview_ = nullptr;
    }

    // Clean up OTA screen if it exists
    otaScreen_.teardown();

    tab1_ = nullptr;
    tab2_ = nullptr;
    tab3_ = nullptr;
    initialized_ = false;
}

void UIManager::scrollBeginEvent(lv_event_t* e) {
    // Disable the scroll animations. Triggered when a tab button is clicked
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        lv_anim_t* a = (lv_anim_t*)lv_event_get_param(e);
        if (a) lv_anim_set_duration(a, 0);
    }
}

void UIManager::showEffectsTab() {
    if (!tabview_) return;

    // LV_ANIM_OFF is load-bearing, not a style choice. On LV_EVENT_SCROLL_END the
    // tabview recomputes which tab is active from the content's LIVE scroll
    // position (lv_tabview.c: `t = (p.x + w / 2) / w`). An animated scroll leaves
    // that position at the old tab for the whole animation, so any SCROLL_END
    // landing in that window resolves back to the tab we just left — the switch
    // visibly happens and then flicks back. Jumping outright closes the window.
    lv_tabview_set_active(tabview_, 1, LV_ANIM_OFF);
}

void UIManager::showOTAScreen() {
    // Flag the overlay; the render task creates it in update(). Also blank the
    // LEDs so the strips don't keep animating during the update.
    otaScreen_.show();
    if (g_ledManager) {
        g_ledManager->fillColor(CRGB(0, 0, 0));
    }
}

void UIManager::updateOTAProgress(uint8_t progress) {
    otaScreen_.updateProgress(progress);
}

void UIManager::hideOTAScreen() {
    otaScreen_.hide();
}