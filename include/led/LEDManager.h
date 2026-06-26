#pragma once

#include <Arduino.h>
#include "led/LedHelpers.h"
#include "led/LedDriver.h"
#include "led/Animations.h"
#include "led/AnimationEngine.h"
#include "audio/AudioFrame.h"
#include <Preferences.h>

/**
 * @brief Coordinates LED configuration, control state, and rendering.
 *
 * Owns the pixel buffer + the RMT/DMA driver, loads the strip config and the
 * persisted control state (brightness / mode / colour / animation), and drives
 * the AnimationEngine each tick. The animation *implementations* live in
 * AnimationEngine; the animation *vocabulary* (AnimationType + labels) lives in
 * Animations.h. This class is the thin coordinator over those pieces.
 */
class LEDManager {
public:
    LEDManager();
    ~LEDManager();

    // Disable copy operations to prevent resource issues
    LEDManager(const LEDManager&) = delete;
    LEDManager& operator=(const LEDManager&) = delete;

    /**
     * @brief Initialize LED system and load configuration
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Update LED animations and effects
     */
    void update();

    /**
     * @brief Set LED brightness
     * @param newBrightness Brightness value (0-255)
     */
    void setBrightness(uint8_t newBrightness);

    /**
     * @brief Blocking boot animation: a diagonal rainbow-field "dispersion wipe"
     *        (top-left → bottom-right across the strip grid) that then cross-fades
     *        from the final dispersion frame into the saved state. Runs once in
     *        setup() before the render task takes over LVGL.
     */
    void performStartupDispersion();

    /**
     * @brief Show OTA update progress on LEDs
     * @param progress Progress percentage (0-100)
     */
    void showOTAProgress(uint8_t progress);

    /**
     * @brief Get current brightness
     */
    uint8_t getBrightness() const { return brightness_; }

    /**
     * @brief Set animation state
     */
    void setAnimationEnabled(bool enabled);

    /**
     * @brief Enter / leave OTA mode. While in OTA mode, update() is a no-op so
     *        the OTA progress display isn't fought by the normal brightness tick.
     */
    void setOTAMode(bool enabled);

    /**
     * @brief Trigger a brief non-blocking red flash (e.g. OTA failure feedback).
     *        Rendered over the next ~1.2 s by update(); safe to call from any task.
     */
    void flashError();

    /**
     * @brief Check if animations are enabled
     */
    bool isAnimationEnabled() const { return showAnimation_; }

    /**
     * @brief Set current animation type
     */
    void setCurrentAnimation(AnimationType animation);

    /**
     * @brief Get current animation type
     */
    AnimationType getCurrentAnimation() const { return currentAnimation_; }

    /**
     * @brief Set VU mode state
     */
    void setVuMode(bool enabled);

    /**
     * @brief Check if VU mode is enabled
     */
    bool isVuModeEnabled() const { return vuMode_; }

    /**
     * @brief Set white mode state
     */
    void setWhiteMode(bool enabled);

    /**
     * @brief Check if white mode is enabled
     */
    bool isWhiteModeEnabled() const { return whiteMode_; }

    /**
     * @brief Set solid color mode with specified color
     */
    void setSolidColor(CRGB color);

    /**
     * @brief Get current solid color
     */
    CRGB getSolidColor() const { return solidColor_; }

    /**
     * @brief Fill all LEDs with a color (immediate, no state change)
     */
    void fillColor(CRGB color);

    /**
     * @brief Mark state as dirty (needs saving after debounce)
     */
    void markStateDirty();

    /**
     * @brief Check if saved state was loaded on boot
     */
    bool hasLoadedState() const { return stateLoaded_; }

    /**
     * @brief Clear saved LED state from preferences
     */
    void clearSavedState();

    /**
     * @brief Fill all LEDs with white
     */
    void fillWhite();

    /**
     * @brief Get number of LED strips
     */
    int getNumStrips() const;

    /**
     * @brief Get LEDs per strip
     */
    int getLedsPerStrip() const;

    /**
     * @brief Get total number of LEDs
     */
    int getTotalLeds() const;

    /**
     * @brief Check if LED configuration is valid
     */
    bool isConfigValid() const;

private:
    // Member variables
    CRGB* leds_;
    int numStrips_;
    int ledsPerStrip_;
    int totalLeds_;
    bool configLoaded_;
    bool initialized_;

    uint8_t brightness_;
    bool showAnimation_;
    bool vuMode_;
    bool whiteMode_;
    AnimationType currentAnimation_;
    CRGB solidColor_;

    // State persistence
    bool stateDirty_;
    bool stateLoaded_;
    unsigned long stateChangedTime_;
    static const unsigned long STATE_SAVE_DEBOUNCE_MS = 5000;

    // Overall audio level (used for VU-mode brightness)
    int audioLevel_;

    // OTA progress tracking
    uint8_t lastOTAProgress_;
    // While true, update() is suspended so showOTAProgress() can own the strips
    // without the normal brightness/animation tick fighting it.
    bool otaMode_;
    // Non-zero while a failure flash is active: millis() deadline to flash until.
    unsigned long errorFlashUntilMs_;

    Preferences preferences_;

    // Animation timing
    unsigned long lastAnimationUpdate_;

    LedDriver driver_;
    AnimationEngine engine_;

    // Private methods
    void loadConfiguration();
    void loadState();
    void saveState();
    void saveStateIfNeeded();
    bool allocateLedArrays();
    void deallocateLedArrays();
    void updateBrightness();
};

// Global LED manager instance
extern LEDManager* g_ledManager;
