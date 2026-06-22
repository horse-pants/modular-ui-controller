#include "led/LEDManager.h"
#include "audio/AudioBus.h"
#include "pins.h"
#include <Logger.h>

// Global LED manager instance
LEDManager* g_ledManager = nullptr;

LEDManager::LEDManager()
    : leds_(nullptr)
    , numStrips_(0)
    , ledsPerStrip_(0)
    , totalLeds_(0)
    , configLoaded_(false)
    , initialized_(false)
    , brightness_(128)
    , showAnimation_(false)
    , vuMode_(false)
    , whiteMode_(false)
    , currentAnimation_(RAINBOW)
    , solidColor_(CRGB::Red)
    , stateDirty_(false)
    , stateLoaded_(false)
    , stateChangedTime_(0)
    , audioLevel_(0)
    , lastOTAProgress_(255)  // Invalid value to force first update
    , otaMode_(false)
    , errorFlashUntilMs_(0)
    , lastAnimationUpdate_(0)
{
}

LEDManager::~LEDManager() {
    driver_.end();
    deallocateLedArrays();
    preferences_.end();
}

bool LEDManager::initialize() {
    if (initialized_) {
        return true;
    }

    loadConfiguration();

    if (!isConfigValid()) {
        Logger.warning("No valid LED configuration found. LED functions disabled.");
        Logger.warning("Please configure LED settings through the setup page.");
        return false;
    }

    if (!allocateLedArrays()) {
        Logger.error("Failed to allocate memory for LEDs");
        return false;
    }

    if (!driver_.begin(pins::LED_DATA, totalLeds_)) {
        Logger.error("LedDriver init failed");
        return false;
    }
    driver_.setBrightness(0);
    driver_.clear();

    // Hand the strip geometry + pixel buffer to the animation engine.
    engine_.setStrips(numStrips_, ledsPerStrip_);
    engine_.setBuffer(leds_, totalLeds_);

    // Load saved LED state (brightness, mode, color, animation, etc.)
    loadState();

    initialized_ = true;
    Logger.info("LED Manager initialized: %d strips, %d LEDs/strip, %d total",
                numStrips_, ledsPerStrip_, totalLeds_);
    return true;
}

void LEDManager::update() {
    if (!initialized_ || !isConfigValid() || !leds_) {
        return;
    }

    // While OTA progress is being painted by showOTAProgress(), don't fight it
    // by resetting brightness/animation each tick — that's what caused the
    // strips to flicker between OTA brightness (150) and the user's saved value.
    if (otaMode_) {
        return;
    }

    // Non-blocking failure flash (set by flashError(), e.g. after a failed OTA):
    // blink red ~3x over 1.2 s, then fall through to normal operation.
    if (errorFlashUntilMs_ != 0) {
        const unsigned long now = millis();
        if (now < errorFlashUntilMs_) {
            const bool on = (((errorFlashUntilMs_ - now) / 200) & 1UL) == 0;
            fillColor(on ? CRGB(CRGB::Red) : CRGB(CRGB::Black));
            driver_.setBrightness(255);
            driver_.show(leds_);
            return;
        }
        errorFlashUntilMs_ = 0;  // done — resume normal rendering this tick
    }

    // Pull the latest audio snapshot (published by the audio task) and feed it to
    // the engine; keep the overall level for VU-mode brightness.
    const AudioFrame audio = g_audioBus.latest();
    audioLevel_ = audio.overall;
    engine_.setAudio(audio);

    updateBrightness();

    unsigned long currentTime = millis();
    if (currentTime - lastAnimationUpdate_ >= (unsigned long)engine_.frameIntervalMs(currentAnimation_)) {
        lastAnimationUpdate_ = currentTime;

        if (showAnimation_) {
            engine_.render(currentAnimation_);
        }
    }

    driver_.show(leds_);

    // Check if state needs saving (debounced)
    saveStateIfNeeded();
}

void LEDManager::setBrightness(uint8_t newBrightness) {
    brightness_ = newBrightness;
    markStateDirty();
}

void LEDManager::flashError() {
    // Render handled non-blockingly in update(); ~3 blinks at 200 ms.
    errorFlashUntilMs_ = millis() + 1200;
}

void LEDManager::performStartupFadeIn() {
    if (!initialized_ || !isConfigValid() || !leds_) {
        return;
    }

    // Use saved brightness, minimum 1 for visibility during fade
    uint8_t targetBrightness = brightness_;
    if (targetBrightness < 1) {
        targetBrightness = 1;
    }

    // Calculate delay to make fade take roughly the same time regardless of brightness
    // Lower brightness = longer delay per step to maintain visible fade
    int delayPerStep = max(5, 2000 / max((int)targetBrightness, 1));

    // Determine what to show based on saved state
    if (showAnimation_) {
        // For animations: start animation immediately but fade in brightness
        driver_.setBrightness(0);
        for (int i = 0; i <= targetBrightness; i++) {
            engine_.render(currentAnimation_);
            driver_.setBrightness(i);
            driver_.show(leds_);
            delay(delayPerStep);
        }
    } else if (whiteMode_) {
        // Fade in white
        fillColor(CRGB::White);
        for (int i = 0; i <= targetBrightness; i++) {
            driver_.setBrightness(i);
            driver_.show(leds_);
            delay(delayPerStep);
        }
    } else {
        // Fade in solid color (default to red if no color saved)
        CRGB startupColor = solidColor_;
        if (startupColor.r == 0 && startupColor.g == 0 && startupColor.b == 0) {
            startupColor = CRGB::Red;  // Default to red for first boot
        }
        fillColor(startupColor);
        for (int i = 0; i <= targetBrightness; i++) {
            driver_.setBrightness(i);
            driver_.show(leds_);
            delay(delayPerStep);
        }
    }

    // Ensure final brightness is exactly what was saved
    driver_.setBrightness(brightness_);
    driver_.show(leds_);
}

void LEDManager::setOTAMode(bool enabled) {
    otaMode_ = enabled;
    if (enabled) {
        lastOTAProgress_ = 255;  // Force the next showOTAProgress() to paint.
    }
}

void LEDManager::showOTAProgress(uint8_t progress) {
    if (!initialized_ || !isConfigValid() || !leds_) {
        return;
    }

    // Clamp progress to 0-100
    if (progress > 100) {
        progress = 100;
    }

    // Only update if progress changed to reduce jitter
    if (progress == lastOTAProgress_) {
        return;
    }
    lastOTAProgress_ = progress;

    // Calculate how many LEDs per strip should be lit based on progress
    int ledsPerStripToLight = (ledsPerStrip_ * progress) / 100;

    // Fill each strip left to right in unison
    // Account for daisy-chaining: even strips (0,2,4...) are reversed
    for (int strip = 0; strip < numStrips_; strip++) {
        int stripStart = strip * ledsPerStrip_;
        bool reversed = (strip % 2 == 0); // Even strips are reversed

        for (int led = 0; led < ledsPerStripToLight; led++) {
            // Gradient from cyan (start) to green (end) based on position in strip
            uint8_t ratio = (led * 255) / ledsPerStrip_;

            // Calculate LED index based on direction
            int ledIndex;
            if (reversed) {
                ledIndex = stripStart + (ledsPerStrip_ - 1 - led); // Fill from right to left
            } else {
                ledIndex = stripStart + led; // Fill from left to right
            }

            leds_[ledIndex] = CHSV(96 + (ratio / 4), 255, 255); // Cyan (96) to green (128)
        }
    }

    // Flash all LEDs green when complete
    if (progress >= 100) {
        for (int i = 0; i < totalLeds_; i++) {
            leds_[i] = CRGB::Green;
        }
    }

    driver_.setBrightness(150); // Full brightness for OTA progress
    driver_.show(leds_);
}

void LEDManager::fillColor(CRGB color) {
    if (!initialized_ || !leds_) {
        return;
    }

    for (int i = 0; i < totalLeds_; i++) {
        leds_[i] = color;
    }
}

void LEDManager::fillWhite() {
    fillColor(CRGB(255, 255, 255));
}

int LEDManager::getNumStrips() const {
    return numStrips_;
}

int LEDManager::getLedsPerStrip() const {
    return ledsPerStrip_;
}

int LEDManager::getTotalLeds() const {
    return totalLeds_;
}

bool LEDManager::isConfigValid() const {
    return (numStrips_ > 0 && ledsPerStrip_ > 0 && totalLeds_ > 0);
}

void LEDManager::loadConfiguration() {
    if (configLoaded_) {
        return;
    }

    preferences_.begin("led-config", true); // Read-only mode
    numStrips_ = preferences_.getInt("num_strips", 0);
    ledsPerStrip_ = preferences_.getInt("leds_per_strip", 0);
    totalLeds_ = numStrips_ * ledsPerStrip_;
    preferences_.end();
    configLoaded_ = true;

    Logger.info("Loaded LED config: %d strips, %d LEDs/strip, %d total",
                numStrips_, ledsPerStrip_, totalLeds_);
}

void LEDManager::loadState() {
    Preferences statePrefs;
    statePrefs.begin("led-state", true); // Read-only

    // Check if we have saved state
    if (!statePrefs.isKey("brightness")) {
        Logger.info("No saved LED state found, using defaults (red fade-in)");
        stateLoaded_ = false;
        statePrefs.end();
        return;
    }

    brightness_ = statePrefs.getUChar("brightness", 128);
    showAnimation_ = statePrefs.getBool("animation", false);
    whiteMode_ = statePrefs.getBool("white", false);
    vuMode_ = statePrefs.getBool("vu", false);
    currentAnimation_ = static_cast<AnimationType>(statePrefs.getUChar("anim_idx", 0));

    // Load solid color as packed RGB
    uint32_t packedColor = statePrefs.getUInt("color", 0xFF0000); // Default red
    solidColor_.r = (packedColor >> 16) & 0xFF;
    solidColor_.g = (packedColor >> 8) & 0xFF;
    solidColor_.b = packedColor & 0xFF;

    statePrefs.end();
    stateLoaded_ = true;

    Logger.info("Loaded LED state: bright=%d, anim=%d, white=%d, vu=%d, animIdx=%d, color=#%02X%02X%02X",
                brightness_, showAnimation_, whiteMode_, vuMode_, currentAnimation_,
                solidColor_.r, solidColor_.g, solidColor_.b);
}

void LEDManager::clearSavedState() {
    Preferences statePrefs;
    statePrefs.begin("led-state", false);
    statePrefs.clear();
    statePrefs.end();
    stateLoaded_ = false;
    Logger.info("LED state preferences cleared");
}

void LEDManager::saveState() {
    Preferences statePrefs;
    statePrefs.begin("led-state", false); // Read-write

    statePrefs.putUChar("brightness", brightness_);
    statePrefs.putBool("animation", showAnimation_);
    statePrefs.putBool("white", whiteMode_);
    statePrefs.putBool("vu", vuMode_);
    statePrefs.putUChar("anim_idx", static_cast<uint8_t>(currentAnimation_));

    // Pack color as RGB
    uint32_t packedColor = ((uint32_t)solidColor_.r << 16) | ((uint32_t)solidColor_.g << 8) | solidColor_.b;
    statePrefs.putUInt("color", packedColor);

    statePrefs.end();

    Logger.debug("LED state saved to preferences");
}

void LEDManager::saveStateIfNeeded() {
    if (!stateDirty_) {
        return;
    }

    // Check if enough time has passed since last change (debounce)
    if (millis() - stateChangedTime_ >= STATE_SAVE_DEBOUNCE_MS) {
        saveState();
        stateDirty_ = false;
    }
}

void LEDManager::markStateDirty() {
    stateDirty_ = true;
    stateChangedTime_ = millis();
}

void LEDManager::setAnimationEnabled(bool enabled) {
    showAnimation_ = enabled;
    markStateDirty();
}

void LEDManager::setCurrentAnimation(AnimationType animation) {
    currentAnimation_ = animation;
    markStateDirty();
}

void LEDManager::setVuMode(bool enabled) {
    vuMode_ = enabled;
    markStateDirty();
}

void LEDManager::setWhiteMode(bool enabled) {
    whiteMode_ = enabled;
    markStateDirty();
}

void LEDManager::setSolidColor(CRGB color) {
    solidColor_ = color;
    showAnimation_ = false;
    whiteMode_ = false;
    fillColor(color);
    markStateDirty();
}

bool LEDManager::allocateLedArrays() {
    if (leds_ || totalLeds_ <= 0) {
        return false;
    }

    Logger.debug("Allocating memory for %d LEDs", totalLeds_);
    leds_ = new CRGB[totalLeds_];
    return (leds_ != nullptr);
}

void LEDManager::deallocateLedArrays() {
    if (leds_) {
        delete[] leds_;
        leds_ = nullptr;
    }
}

void LEDManager::updateBrightness() {
    if (vuMode_) {
        driver_.setBrightness(audioLevel_);
    } else {
        driver_.setBrightness(brightness_);
    }
}
