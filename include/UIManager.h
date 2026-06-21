#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <memory>
#include "UiCommand.h"
#include "BrightnessSlider.h"
#include "ColourWheel.h"
#include "EffectsList.h"
#include "WhiteButton.h"
#include "VuButton.h"
#include "VuGraph.h"
#include "AudioVisualiser.h"

/**
 * @brief Modern C++ class for managing the entire LVGL UI system
 * 
 * This class provides a clean interface for creating and managing the complete
 * UI with proper resource management and initialization.
 * 
 * Features:
 * - RAII resource management
 * - Move semantics support
 * - Complete UI lifecycle management
 * - Automatic cleanup on destruction
 * - Exception safety
 * - Display and touch driver management
 * 
 * @author Claude Code
 */
class UIManager {
public:
    /**
     * @brief Default constructor
     */
    UIManager();

    /**
     * @brief Destructor - automatically cleans up all UI resources
     */
    ~UIManager();

    /**
     * @brief Move constructor
     */
    UIManager(UIManager&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    UIManager& operator=(UIManager&& other) noexcept;

    // Disable copy operations to prevent resource issues
    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;

    /**
     * @brief Initialize the display and touch drivers
     * @return true if initialization was successful, false otherwise
     */
    bool initializeScreen();

    /**
     * @brief Initialize the complete UI system
     * @return true if initialization was successful, false otherwise
     */
    bool initializeUI();

    /**
     * @brief Update the UI (call this regularly in main loop)
     */
    void update();

    /**
     * @brief Queue a UI mutation from another task (e.g. AsyncTCP web handlers).
     *
     * Non-blocking and safe to call from any task. The command is applied on
     * the UI owner task inside update(); it is silently dropped if the queue
     * is full (these are latest-state controls, so a flood losing a stale
     * intermediate value is acceptable).
     *
     * @param cmd POD command, copied by value into the queue
     */
    void postUiCommand(const UiCommand& cmd);

    /**
     * @brief Spin up the render task as the sole owner of all lv_* calls.
     *
     * Pinned to core 1 at a priority above the Arduino loopTask so it preempts
     * it. From this point on, NOTHING else may call lv_*: the loop runs only the
     * non-UI managers, and web handlers hand work over via postUiCommand().
     * Call once, after the full UI is initialised and all startup lv_* (e.g.
     * syncWithLEDState) has completed. Only used in normal mode — in setup mode
     * the boot UI keeps driving LVGL on the loop and this is never called.
     */
    void startRenderTask();

    /**
     * @brief Whether the render task owns LVGL (true after startRenderTask()).
     */
    bool isRenderTaskRunning() const { return renderTaskHandle_ != nullptr; }

    /**
     * @brief Apply the current color from color wheel to LEDs
     */
    void applyCurrentColor();

    /**
     * @brief Set VU button state (called from web UI)
     * @param newState Whether VU should be active
     */
    void setVuState(bool newState);

    /**
     * @brief Set white button state (called from web UI)
     * @param newState Whether white mode should be active
     */
    void setWhiteState(bool newState);

    /**
     * @brief Log and update VU state (called from both screen and web)
     * @param newState Whether VU should be active
     */
    void logAndUpdateVuState(bool newState);

    /**
     * @brief Log and update white state (called from both screen and web)
     * @param newState Whether white mode should be active
     */
    void logAndUpdateWhiteState(bool newState);

    /**
     * @brief Set animation state
     * @param newState Whether animations should be active
     */
    void setAnimationState(bool newState);

    /**
     * @brief Set specific animation
     * @param animation Animation index to set
     */
    void setAnimation(int animation);

    /**
     * @brief Sync UI state with LEDManager's loaded state
     * Call this after UI is initialized to reflect saved LED state
     */
    void syncWithLEDState();

    /**
     * @brief Check if the UI manager is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the brightness slider instance
     */
    BrightnessSlider* getBrightnessSlider() const { return brightnessSlider_.get(); }

    /**
     * @brief Get the colour wheel instance
     */
    ColourWheel* getColourWheel() const { return colourWheel_.get(); }

    /**
     * @brief Get the effects list instance
     */
    EffectsList* getEffectsList() const { return effectsList_.get(); }

    /**
     * @brief Get the white button instance
     */
    WhiteButton* getWhiteButton() const { return whiteButton_.get(); }

    /**
     * @brief Get the VU button instance
     */
    VuButton* getVuButton() const { return vuButton_.get(); }

    /**
     * @brief Get the VU graph instance
     */
    VuGraph* getVuGraph() const { return vuGraph_.get(); }

    /**
     * @brief Configure the idle VU-meter screensaver (persisted to NVS).
     *
     * Safe to call from any task (e.g. the web handler on AsyncTCP): it only
     * writes POD config + NVS, never lv_*. The render task picks the new values
     * up on its next update() tick — including live-dismissing the screensaver
     * if it's disabled while showing.
     *
     * @param enabled   Whether the screensaver may appear at all
     * @param idleMs    Untouched time before it appears, in milliseconds
     */
    void setScreensaverConfig(bool enabled, uint32_t idleMs);

    /** @brief Whether the idle screensaver is enabled. */
    bool isScreensaverEnabled() const { return screensaverEnabled_; }

    /** @brief Idle timeout before the screensaver appears, in milliseconds. */
    uint32_t getScreensaverIdleMs() const { return screensaverIdleMs_; }

    /**
     * @brief Show OTA update screen
     */
    void showOTAScreen();

    /**
     * @brief Update OTA progress on screen
     * @param progress Progress percentage (0-100)
     */
    void updateOTAProgress(uint8_t progress);

    /**
     * @brief Hide OTA update screen
     */
    void hideOTAScreen();

    /**
     * @brief Create a styled panel container (hardware module look)
     * @param parent Parent object
     * @return Styled panel object
     */
    static lv_obj_t* createPanel(lv_obj_t* parent);

    /**
     * @brief Create a section label (e.g., "BRIGHTNESS", "COLOUR")
     * @param parent Parent object
     * @param text Label text
     * @return Label object
     */
    static lv_obj_t* createSectionLabel(lv_obj_t* parent, const char* text);

private:
    // Shared styles (initialized once)
    static lv_style_t stylePanelMain_;
    static lv_style_t stylePanelInset_;
    static lv_style_t styleSectionLabel_;
    static bool stylesInitialized_;

    /**
     * @brief Initialize shared styles (called once)
     */
    static void initStyles();

    // UI component instances using smart pointers
    std::unique_ptr<BrightnessSlider> brightnessSlider_;
    std::unique_ptr<ColourWheel> colourWheel_;
    std::unique_ptr<EffectsList> effectsList_;
    std::unique_ptr<WhiteButton> whiteButton_;
    std::unique_ptr<VuButton> vuButton_;
    std::unique_ptr<VuGraph> vuGraph_;
    std::unique_ptr<AudioVisualiser> audioVisualiser_;

    // LVGL objects
    lv_obj_t* tabview_;
    lv_obj_t* tab1_;  // Colour tab
    lv_obj_t* tab2_;  // VU tab

    // OTA screen objects (lv_* objects: only ever touched on the render task)
    lv_obj_t* otaScreen_;
    lv_obj_t* otaLabel_;
    lv_obj_t* otaBar_;
    // OTA flags: written from the AsyncTCP task (OTA progress callbacks during a
    // web upload), read on the render task in update(). volatile so the reader
    // always sees the latest value; aligned bool/uint8_t writes are atomic on
    // ESP32, and a missed "changed" edge is caught on the next render tick.
    volatile bool otaScreenActive_;
    volatile uint8_t otaPendingProgress_;
    volatile bool otaProgressChanged_;

    bool initialized_;
    bool screenInitialized_;

    // Idle-screensaver config, loaded from NVS in initializeComponents() and
    // updatable live from the web (setScreensaverConfig). volatile: written from
    // the AsyncTCP task, read on the render task in update() — aligned bool/
    // uint32_t accesses are atomic on ESP32, same pattern as the OTA flags.
    volatile bool screensaverEnabled_;
    volatile uint32_t screensaverIdleMs_;

    // Cross-task UI command queue (producers: web handlers on AsyncTCP;
    // consumer: this manager's update() on the UI owner task)
    QueueHandle_t uiCommandQueue_;

    // Render task — sole lv_* owner once started (nullptr until startRenderTask)
    TaskHandle_t renderTaskHandle_;

    /**
     * @brief FreeRTOS entry point: loops update() on the render task
     */
    static void renderTaskTrampoline(void* arg);

    /**
     * @brief Drain and apply all queued UI commands (called from update())
     */
    void drainUiCommands();

    /**
     * @brief Apply a single UI command on the UI owner task
     */
    void applyUiCommand(const UiCommand& cmd);

    /**
     * @brief Apply the synth theme to the screen
     */
    void applySynthTheme();

    /**
     * @brief Create and style the tabview
     */
    bool createTabview();

    /**
     * @brief Initialize all UI components
     */
    bool initializeComponents();

    /**
     * @brief Load the idle-screensaver config from NVS into the runtime members.
     */
    void loadScreensaverConfig();

    /**
     * @brief Setup display driver
     */
    void setupDisplayDriver();

    /**
     * @brief Setup touch driver
     */
    void setupTouchDriver();

    /**
     * @brief Clean up all UI resources
     */
    void cleanup();

    /**
     * @brief Static event handler for tab scroll events
     */
    static void scrollBeginEvent(lv_event_t* e);
};