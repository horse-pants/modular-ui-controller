#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>

/**
 * @brief Modern C++ LED Manager class
 * 
 * Handles LED configuration, animations, and effects with proper RAII
 * resource management and clean interfaces.
 */
class LEDManager {
public:
    // Animation types
    enum AnimationType {
        RAINBOW = 0,
        CYLON,
        RGBCHASER,
        BEATSINE,
        ICEWAVES,
        PURPLERAIN,
        FIRE,
        MATRIX,
        VU,
        BEATDROP,
        SOUNDRIPPLE
    };
    
    /**
     * @brief Constructor
     */
    LEDManager();
    
    /**
     * @brief Destructor - automatically cleans up resources
     */
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
     * @param brightness Brightness value (0-255)
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief Perform startup fade-in sequence
     */
    void performStartupFadeIn();

    /**
     * @brief Show OTA update progress on LEDs
     * @param progress Progress percentage (0-100)
     */
    void showOTAProgress(uint8_t progress);

    /**
     * @brief Get current brightness
     * @return Current brightness value
     */
    uint8_t getBrightness() const { return brightness_; }
    
    /**
     * @brief Set animation state
     * @param enabled True to enable animations, false to disable
     */
    void setAnimationEnabled(bool enabled) { showAnimation_ = enabled; }
    
    /**
     * @brief Check if animations are enabled
     * @return True if animations are enabled
     */
    bool isAnimationEnabled() const { return showAnimation_; }
    
    /**
     * @brief Set current animation type
     * @param animation Animation type to set
     */
    void setCurrentAnimation(AnimationType animation) { currentAnimation_ = animation; }
    
    /**
     * @brief Get current animation type
     * @return Current animation type
     */
    AnimationType getCurrentAnimation() const { return currentAnimation_; }
    
    /**
     * @brief Set VU mode state
     * @param enabled True to enable VU brightness control
     */
    void setVuMode(bool enabled) { vuMode_ = enabled; }
    
    /**
     * @brief Check if VU mode is enabled
     * @return True if VU mode is enabled
     */
    bool isVuModeEnabled() const { return vuMode_; }
    
    /**
     * @brief Set white mode state
     * @param enabled True to enable white mode
     */
    void setWhiteMode(bool enabled) { whiteMode_ = enabled; }
    
    /**
     * @brief Check if white mode is enabled
     * @return True if white mode is enabled
     */
    bool isWhiteModeEnabled() const { return whiteMode_; }
    
    /**
     * @brief Fill all LEDs with a color
     * @param color Color to fill with
     */
    void fillColor(CRGB color);
    
    /**
     * @brief Fill all LEDs with white
     */
    void fillWhite();
    
    /**
     * @brief Update VU levels for animations (called from VuGraph)
     * @param vuLevels Array of 7 VU levels
     * @param audioLevel Overall audio level
     */
    void updateVuLevels(const int* vuLevels, int audioLevel);
    
    /**
     * @brief Get VU level for specific strip
     * @param strip Strip index
     * @return VU level for the strip
     */
    int getVuForStrip(int strip) const;
    
    /**
     * @brief Get number of LED strips
     * @return Number of strips
     */
    int getNumStrips() const;
    
    /**
     * @brief Get LEDs per strip
     * @return LEDs per strip
     */
    int getLedsPerStrip() const;
    
    /**
     * @brief Get total number of LEDs
     * @return Total LED count
     */
    int getTotalLeds() const;
    
    /**
     * @brief Check if LED configuration is valid
     * @return True if configuration is valid
     */
    bool isConfigValid() const;
    
    /**
     * @brief Get animation description
     * @param animation Animation type
     * @return Description string
     */
    static const char* getAnimationDescription(AnimationType animation);
    
    /**
     * @brief Get all animation descriptions
     * @return Array of description strings
     */
    static const char* const* getAnimationDescriptions();

private:
    // Configuration constants
    static const int DATA_PIN = 10;
    static const int MAX_BRIGHTNESS = 255;
    
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
    
    // VU data
    int vuLevels_[7];
    int audioLevel_;

    // OTA progress tracking
    uint8_t lastOTAProgress_;

    Preferences preferences_;
    
    // Animation timing
    unsigned long lastAnimationUpdate_;
    
    // Private methods
    void loadConfiguration();
    bool allocateLedArrays();
    void deallocateLedArrays();
    void updateBrightness();
    int getAnimationInterval() const;
    void runAnimation();
    
    // Animation implementations
    void fadeAll(int amount);
    void fadeRed(int amount);
    void fadeGreen(int amount);
    void animationRainbow();
    void animationCylon();
    void animationRgbChaser();
    void animationBeatSine();
    void animationIceWaves();
    void animationPurpleRain();
    void animationFire();
    void animationMatrix();
    void animationVu();
    void animationBeatDrop();
    void animationSoundRipple();
    
    // Helper methods
    int getCentreOfStrip(int strip) const;
    void fillFromCentre(int strip, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3);
    void moveFromCentre(int strip);
    void moveDown();
    int getRandomLed(int divisions, int division) const;
    CRGB pickColour(int led, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3) const;
    
    // Static animation descriptions
    static const char* animationDescriptions_[];
};

// Global LED manager instance
extern LEDManager* g_ledManager;

// Legacy compatibility - global state variables accessible to other modules
extern uint8_t brightness;
extern bool showAnimation;
extern bool vu;
extern bool white;
extern LEDManager::AnimationType currentAnimation;
extern int vuValue[7];
extern int audioLevel;