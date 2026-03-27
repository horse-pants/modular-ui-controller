#include "LEDManager.h"
#include "UIManager.h"

// Global LED manager instance
LEDManager* g_ledManager = nullptr;

// Legacy compatibility - global state variables
uint8_t brightness = 128;
bool showAnimation = false;
bool vu = false;
bool white = false;
LEDManager::AnimationType currentAnimation = LEDManager::RAINBOW;
int vuValue[7] = {0, 0, 0, 0, 0, 0, 0};
int audioLevel = 0;

// Static animation descriptions ((A) = audio reactive)
const char* LEDManager::animationDescriptions_[] = {
    // Non-audio reactive
    "Rainbow",
    "Cylon",
    "RGB Chaser",
    "Beat Sine",
    "Plasma",
    "Sparkle",
    "Wave",
    "Comet",
    // Audio reactive
    "Ice Waves (A)",
    "Purple Rain (A)",
    "Fire (A)",
    "Matrix (A)",
    "VU (A)",
    "Ripple (A)",
    "Confetti (A)"
};

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
    , lastAnimationUpdate_(0)
{
    // Initialize VU levels array
    for (int i = 0; i < 7; i++) {
        vuLevels_[i] = 0;
    }
}

LEDManager::~LEDManager() {
    deallocateLedArrays();
    preferences_.end();
}

bool LEDManager::initialize() {
    if (initialized_) {
        return true;
    }

    loadConfiguration();

    if (!isConfigValid()) {
        Serial.println("Warning: No valid LED configuration found. LED functions disabled.");
        Serial.println("Please configure LED settings through the setup page.");
        return false;
    }

    if (!allocateLedArrays()) {
        Serial.println("Error: Failed to allocate memory for LEDs");
        return false;
    }

    // Initialize FastLED
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds_, totalLeds_);
    FastLED.clear();
    FastLED.setBrightness(0);

    // Load saved LED state (brightness, mode, color, animation, etc.)
    loadState();

    // Sync legacy global variables
    brightness = brightness_;
    showAnimation = showAnimation_;
    vu = vuMode_;
    white = whiteMode_;
    currentAnimation = currentAnimation_;

    initialized_ = true;
    Serial.printf("LED Manager initialized: %d strips, %d LEDs/strip, %d total\n",
                 numStrips_, ledsPerStrip_, totalLeds_);
    return true;
}

void LEDManager::update() {
    if (!initialized_ || !isConfigValid() || !leds_) {
        return;
    }

    updateBrightness();

    unsigned long currentTime = millis();
    if (currentTime - lastAnimationUpdate_ >= (unsigned long)getAnimationInterval()) {
        lastAnimationUpdate_ = currentTime;

        if (showAnimation_) {
            runAnimation();
        }
    }

    FastLED.show();

    // Check if state needs saving (debounced)
    saveStateIfNeeded();

    // Keep legacy variables in sync
    brightness = brightness_;
    showAnimation = showAnimation_;
    vu = vuMode_;
    white = whiteMode_;
    currentAnimation = currentAnimation_;
}

void LEDManager::setBrightness(uint8_t newBrightness) {
    brightness_ = newBrightness;
    brightness = brightness_; // Sync legacy global
    markStateDirty();
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
        FastLED.setBrightness(0);
        for (int i = 0; i <= targetBrightness; i++) {
            runAnimation();
            FastLED.setBrightness(i);
            FastLED.show();
            delay(delayPerStep);
        }
    } else if (whiteMode_) {
        // Fade in white
        fillColor(CRGB::White);
        for (int i = 0; i <= targetBrightness; i++) {
            FastLED.setBrightness(i);
            FastLED.show();
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
            FastLED.setBrightness(i);
            FastLED.show();
            delay(delayPerStep);
        }
    }

    // Ensure final brightness is exactly what was saved
    FastLED.setBrightness(brightness_);
    FastLED.show();
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

    FastLED.setBrightness(150); // Full brightness for OTA progress
    FastLED.show();
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

void LEDManager::updateVuLevels(const int* vuLevels, int audioLevel) {
    for (int i = 0; i < 7; i++) {
        vuLevels_[i] = vuLevels[i];
        vuValue[i] = vuLevels[i]; // Sync legacy global
    }
    audioLevel_ = audioLevel;
    audioLevel = audioLevel_; // Sync legacy global
}

int LEDManager::getVuForStrip(int strip) const {
    if (!isConfigValid() || strip < 0 || strip >= numStrips_) {
        return 0;
    }
    
    // If we have 7 or fewer strips, distribute VU levels across strips WITHOUT overlaps
    if (numStrips_ <= 7) {
        // Simple mapping that avoids overlaps by dividing VU bands evenly
        if (numStrips_ == 1) {
            // Single strip gets max of all VU bands
            int maxValue = 0;
            for (int i = 0; i < 7; i++) {
                maxValue = max(maxValue, vuLevels_[i]);
            }
            return maxValue;
        } else if (numStrips_ == 2) {
            // 2 strips: bass (0-3) and treble (4-6)
            if (strip == 0) {
                return max(max(vuLevels_[0], vuLevels_[1]), max(vuLevels_[2], vuLevels_[3]));
            } else {
                return max(max(vuLevels_[4], vuLevels_[5]), vuLevels_[6]);
            }
        } else if (numStrips_ == 3) {
            // 3 strips: bass (0-1), mid (2-4), treble (5-6)
            if (strip == 0) {
                return max(vuLevels_[0], vuLevels_[1]);
            } else if (strip == 1) {
                return max(max(vuLevels_[2], vuLevels_[3]), vuLevels_[4]);
            } else {
                return max(vuLevels_[5], vuLevels_[6]);
            }
        } else if (numStrips_ == 4) {
            // 4 strips: bass (0-1), low-mid (2-3), high-mid (4-5), treble (6)
            if (strip == 0) {
                return max(vuLevels_[0], vuLevels_[1]);
            } else if (strip == 1) {
                return max(vuLevels_[2], vuLevels_[3]);
            } else if (strip == 2) {
                return max(vuLevels_[4], vuLevels_[5]);
            } else {
                return vuLevels_[6];
            }
        } else if (numStrips_ == 5) {
            // 5 strips: each gets 1-2 VU bands
            if (strip == 0) {
                return max(vuLevels_[0], vuLevels_[1]);
            } else if (strip == 1) {
                return vuLevels_[2];
            } else if (strip == 2) {
                return vuLevels_[3];
            } else if (strip == 3) {
                return vuLevels_[4];
            } else {
                return max(vuLevels_[5], vuLevels_[6]);
            }
        } else if (numStrips_ == 6) {
            // 6 strips: VU 0 and 6 get their own, others pair up
            if (strip == 0) {
                return vuLevels_[0];
            } else if (strip == 1) {
                return vuLevels_[1];
            } else if (strip == 2) {
                return vuLevels_[2];
            } else if (strip == 3) {
                return vuLevels_[3];
            } else if (strip == 4) {
                return vuLevels_[4];
            } else {
                return max(vuLevels_[5], vuLevels_[6]);
            }
        } else { // 7 strips
            // 7 strips: one-to-one mapping
            return vuLevels_[strip];
        }
    } else {
        // More strips than VU bands - interpolate
        float vuIndex = (float)strip * 6.0f / (numStrips_ - 1);
        int lowIndex = (int)vuIndex;
        int highIndex = lowIndex + 1;
        
        if (highIndex >= 7) {
            return vuLevels_[6];
        }
        
        // Linear interpolation between adjacent VU values
        float fraction = vuIndex - lowIndex;
        return (int)(vuLevels_[lowIndex] * (1.0f - fraction) + vuLevels_[highIndex] * fraction);
    }
    
    return 0; // Fallback
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

const char* LEDManager::getAnimationDescription(AnimationType animation) {
    if (animation >= 0 && animation < sizeof(animationDescriptions_) / sizeof(animationDescriptions_[0])) {
        return animationDescriptions_[animation];
    }
    return "Unknown";
}

const char* const* LEDManager::getAnimationDescriptions() {
    return animationDescriptions_;
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

    Serial.printf("Loaded LED config: %d strips, %d LEDs/strip, %d total\n",
                 numStrips_, ledsPerStrip_, totalLeds_);
}

void LEDManager::loadState() {
    Preferences statePrefs;
    statePrefs.begin("led-state", true); // Read-only

    // Check if we have saved state
    if (!statePrefs.isKey("brightness")) {
        Serial.println("No saved LED state found, using defaults (red fade-in)");
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

    Serial.printf("Loaded LED state: bright=%d, anim=%d, white=%d, vu=%d, animIdx=%d, color=#%02X%02X%02X\n",
                 brightness_, showAnimation_, whiteMode_, vuMode_, currentAnimation_,
                 solidColor_.r, solidColor_.g, solidColor_.b);
}

void LEDManager::clearSavedState() {
    Preferences statePrefs;
    statePrefs.begin("led-state", false);
    statePrefs.clear();
    statePrefs.end();
    stateLoaded_ = false;
    Serial.println("LED state preferences cleared");
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

    Serial.println("LED state saved to preferences");
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
    
    Serial.printf("Allocating memory for %d LEDs\n", totalLeds_);
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
        FastLED.setBrightness(audioLevel_);
    } else {
        FastLED.setBrightness(brightness_);
    }
}

int LEDManager::getAnimationInterval() const {
    switch (currentAnimation_) {
        // Non-audio reactive
        case RAINBOW: return 10;
        case CYLON: return 30;
        case RGBCHASER: return 30;
        case BEATSINE: return 50;
        case PLASMA: return 20;
        case SPARKLE: return 30;
        case WAVE: return 25;
        case COMET: return 20;
        // Audio reactive
        case ICEWAVES: return 20;
        case PURPLERAIN: return 20;
        case FIRE: return 20;
        case MATRIX: return 50;
        case VU: return 5;
        case RIPPLE: return 15;
        case CONFETTI: return 10;
        default: return 100;
    }
}

void LEDManager::runAnimation() {
    switch (currentAnimation_) {
        // Non-audio reactive
        case RAINBOW: animationRainbow(); break;
        case CYLON: animationCylon(); break;
        case RGBCHASER: animationRgbChaser(); break;
        case BEATSINE: animationBeatSine(); break;
        case PLASMA: animationPlasma(); break;
        case SPARKLE: animationSparkle(); break;
        case WAVE: animationWave(); break;
        case COMET: animationComet(); break;
        // Audio reactive
        case ICEWAVES: animationIceWaves(); break;
        case PURPLERAIN: animationPurpleRain(); break;
        case FIRE: animationFire(); break;
        case MATRIX: animationMatrix(); break;
        case VU: animationVu(); break;
        case RIPPLE: animationRipple(); break;
        case CONFETTI: animationConfetti(); break;
        default: break;
    }
}

// Animation implementations
void LEDManager::fadeAll(int amount) {
    for (int i = 0; i < totalLeds_; i++) {
        CRGB colour = leds_[i];
        int red = colour.r - amount;
        if (red <= 0) red = 0;
        
        int green = colour.g - amount;
        if (green <= 0) green = 0;
        
        int blue = colour.b - amount;
        if (blue <= 0) blue = 0;
        
        colour.r = red;
        colour.g = green;
        colour.b = blue;
        leds_[i] = colour;
    }
}

void LEDManager::fadeRed(int amount) {
    for (int i = 0; i < totalLeds_; i++) {
        CRGB colour = leds_[i];
        int red = colour.r - amount;
        if (red <= 0) red = 0;
        
        colour.r = red;
        leds_[i] = colour;
    }
}

void LEDManager::fadeGreen(int amount) {
    for (int i = 0; i < totalLeds_; i++) {
        CRGB colour = leds_[i];
        int green = colour.g - amount;
        if (green <= 0) green = 0;
        
        colour.g = green;
        leds_[i] = colour;
    }
}

void LEDManager::animationRainbow() {
    static uint8_t hue = 0;
    for (int i = 0; i < totalLeds_; i++) {
        leds_[i] = CHSV((i * 256 / totalLeds_) + hue, 255, 255);
    }
    hue++;
}

void LEDManager::animationCylon() {
    static bool goingLeft = true;
    static uint8_t animationLed = 0;

    leds_[animationLed] = CRGB::Red;
    if (numStrips_ >= 2) {
        leds_[((ledsPerStrip_ * 2) - 1) - animationLed] = CRGB::Red;
    }
    if (numStrips_ >= 3) {
        leds_[animationLed + (ledsPerStrip_ * 2)] = CRGB::Red;
    }
    if (numStrips_ >= 4) {
        leds_[((ledsPerStrip_ * 4) - 1) - animationLed] = CRGB::Red;
    }
    if (numStrips_ >= 5) {
        leds_[animationLed + (ledsPerStrip_ * 4)] = CRGB::Red;
    }
    
    fadeAll(35);

    if (goingLeft) {
        animationLed++;
        if (animationLed == ledsPerStrip_ - 1) {
            goingLeft = false;
        }
    } else {
        animationLed--;
        if (animationLed == 0) {
            goingLeft = true;
        }
    }
}

void LEDManager::animationRgbChaser() {
    static CRGB colour = CRGB::Red;
    static int colourCount = 0;
    static uint8_t animationLed = 0;
    
    leds_[animationLed] = colour;
    animationLed++;
    if (animationLed == totalLeds_) {
        animationLed = 0;

        colourCount++;
        if (colourCount == 3) colourCount = 0;
        switch (colourCount) {
            case 0: colour = CRGB::Red; break;
            case 1: colour = CRGB::Green; break;
            case 2: colour = CRGB::Blue; break;
            default: break;
        }
    }
}

void LEDManager::animationBeatSine() {
    uint16_t beatA = beatsin16(30, 0, 255);
    uint16_t beatB = beatsin16(20, 0, 255);
    fill_rainbow(leds_, totalLeds_, (beatA + beatB) / 2, 2);
}

void LEDManager::animationIceWaves() {
    fadeAll(15);
    for (int strip = 0; strip < numStrips_; strip++) {
        moveFromCentre(strip);
        int vuLevel = getVuForStrip(strip);
        int rChannel = map(vuLevel, 0, 255, 100, 0);
        int gChannel = map(vuLevel, 0, 255, 0, 255);
        leds_[getCentreOfStrip(strip)] = CRGB(rChannel, gChannel, 255);
    }
}

void LEDManager::animationPurpleRain() {
    fadeAll(15);
    for (int strip = 0; strip < numStrips_; strip++) {
        moveFromCentre(strip);
        int vuLevel = getVuForStrip(strip);
        int rChannel = map(vuLevel, 0, 255, 0, 255);
        leds_[getCentreOfStrip(strip)] = CRGB(rChannel, 0, 255);
    }
}

void LEDManager::animationFire() {
    fadeGreen(15);
    fadeRed(7);
    for (int strip = 0; strip < numStrips_; strip++) {
        moveFromCentre(strip);
        int vuLevel = getVuForStrip(strip);
        int gChannel = map(vuLevel, 0, 255, 0, 255);
        leds_[getCentreOfStrip(strip)] = CRGB(255, gChannel, 0);
    }
}

void LEDManager::animationMatrix() {
    moveDown();
    fadeGreen(15);
    
    for (int strip = 0; strip < numStrips_; strip++) {
        int vuLevel = getVuForStrip(strip);
        if (vuLevel > 180) {
            leds_[getRandomLed(numStrips_, strip)] = CRGB::Green;
        }
    }
}

void LEDManager::animationVu() {
    fadeAll(20);
    for (int strip = 0; strip < numStrips_; strip++) {
        fillFromCentre(strip, getVuForStrip(strip), CRGB::Green, CRGB::Orange, CRGB::Red);
    }
}

// Helper method implementations
int LEDManager::getCentreOfStrip(int strip) const {
    return (strip * ledsPerStrip_) + (ledsPerStrip_ / 2);
}

void LEDManager::fillFromCentre(int strip, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3) {
    int ledVu = map(vuValue, 0, 255, 0, (ledsPerStrip_ + 1) / 2);
    int centre = getCentreOfStrip(strip);
    leds_[centre] = colour1;
    for (int i = 1; i <= ledVu; i++) {
        leds_[centre + i] = pickColour(i, vuValue, colour1, colour2, colour3);
        leds_[centre - i] = pickColour(i, vuValue, colour1, colour2, colour3);
    }
}

void LEDManager::moveFromCentre(int strip) {
    int centre = getCentreOfStrip(strip);
    int ledsPerSide = ledsPerStrip_ / 2;

    for (int i = ledsPerSide; i >= 0; i--) {
        leds_[centre + i] = leds_[centre + i - 1];
        leds_[centre - i] = leds_[centre - i + 1];
    }
}

void LEDManager::moveDown() {
    for (int strip = numStrips_ - 1; strip > 0; strip--) {
        for (int led = 0; led < ledsPerStrip_; led++) {
            if (strip % 2 == 1) {
                leds_[(ledsPerStrip_ * strip) + (ledsPerStrip_ - led) - 1] = 
                    leds_[(ledsPerStrip_ * (strip - 1)) + led];
            } else {
                leds_[led + (ledsPerStrip_ * strip)] = 
                    leds_[(ledsPerStrip_ * (strip - 1)) + (ledsPerStrip_ - led) - 1];
            }
        }
    }
}

int LEDManager::getRandomLed(int divisions, int division) const {
    int range = (ledsPerStrip_ + 1) / divisions;
    int led = rand() % range;
    return led + (range * division);
}

CRGB LEDManager::pickColour(int led, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3) const {
    if (led >= 0 && led <= 7) {
        return colour1;
    } else if (led >= 8 && led <= 11) {
        return colour2;
    } else {
        return colour3;
    }
}

int LEDManager::xyToIndex(int x, int y) const {
    // Convert 2D coordinates to linear index accounting for snake wiring
    // y = row (strip), x = column position within strip
    // Even rows (0, 2, 4...) are wired right-to-left (reversed)
    // Odd rows (1, 3, 5...) are wired left-to-right (normal)
    if (x < 0 || x >= ledsPerStrip_ || y < 0 || y >= numStrips_) {
        return -1;  // Out of bounds
    }

    int index;
    if (y % 2 == 0) {
        // Even row - reversed (right to left)
        index = (y * ledsPerStrip_) + (ledsPerStrip_ - 1 - x);
    } else {
        // Odd row - normal (left to right)
        index = (y * ledsPerStrip_) + x;
    }
    return index;
}

// New animation: Plasma - colorful swirling sine wave patterns
void LEDManager::animationPlasma() {
    static uint16_t time = 0;
    time += 1;

    for (int y = 0; y < numStrips_; y++) {
        for (int x = 0; x < ledsPerStrip_; x++) {
            // Create plasma effect using multiple sine waves
            uint8_t v1 = sin8(x * 10 + time);
            uint8_t v2 = sin8(y * 15 + time * 2);
            uint8_t v3 = sin8((x + y) * 8 + time);
            uint8_t v4 = sin8(sqrt16((x * x + y * y) * 64) + time * 3);

            uint8_t hue = (v1 + v2 + v3 + v4) / 4;
            uint8_t brightness = 200 + sin8(time + x * 5) / 5;

            int idx = xyToIndex(x, y);
            if (idx >= 0) {
                leds_[idx] = CHSV(hue, 255, brightness);
            }
        }
    }
}

// New animation: Sparkle - twinkling stars effect
void LEDManager::animationSparkle() {
    static uint8_t sparkleHue = 0;

    // Fade existing LEDs
    fadeAll(25);

    // Add random sparkles
    int numSparkles = max(1, totalLeds_ / 25);  // Scale sparkles with total LEDs
    for (int i = 0; i < numSparkles; i++) {
        if (random8() < 90) {  // ~35% chance per sparkle slot
            int idx = random16(totalLeds_);
            // Mostly colored sparkles, occasional white accent
            if (random8() < 30) {  // ~12% white
                leds_[idx] = CRGB::White;
            } else {
                // Wide hue range for variety
                leds_[idx] = CHSV(sparkleHue + random8(160), 220, 255);
            }
        }
    }
    sparkleHue += 3;  // Faster hue rotation for more color variety
}

// New animation: Wave - horizontal color waves traveling across rows
void LEDManager::animationWave() {
    static uint16_t offset = 0;
    offset += 3;

    for (int y = 0; y < numStrips_; y++) {
        // Each row has a phase offset for wave effect
        uint16_t rowPhase = offset + (y * 40);

        for (int x = 0; x < ledsPerStrip_; x++) {
            // Create wave using sine
            uint8_t wave = sin8(x * 8 + rowPhase);

            // Map wave to hue with row-based offset
            uint8_t hue = wave + (y * 30);
            uint8_t brightness = 150 + (wave / 3);

            int idx = xyToIndex(x, y);
            if (idx >= 0) {
                leds_[idx] = CHSV(hue, 255, brightness);
            }
        }
    }
}

// New animation: Ripple - audio-reactive ripples emanating from random points
void LEDManager::animationRipple() {
    static uint8_t rippleAge[8] = {0};    // Age of up to 8 ripples
    static int8_t rippleX[8] = {0};        // Ripple center X
    static int8_t rippleY[8] = {0};        // Ripple center Y
    static uint8_t rippleHue[8] = {0};     // Ripple color
    static uint8_t nextRipple = 0;
    static uint8_t baseHue = 0;
    static uint8_t spawnCooldown = 0;

    // Slower fade to keep ripples visible longer
    fadeAll(12);

    // Ambient background glow that pulses with audio
    uint8_t ambientBright = 15 + (audioLevel_ / 10);
    for (int i = 0; i < totalLeds_; i++) {
        leds_[i] += CHSV(baseHue + 128, 255, ambientBright);
    }
    baseHue++;

    // Decrease cooldown
    if (spawnCooldown > 0) spawnCooldown--;

    // Spawn new ripple on audio - lower threshold and faster spawning
    if (audioLevel_ > 60 && spawnCooldown == 0) {
        // Spawn rate scales with audio intensity
        int spawnChance = map(audioLevel_, 60, 255, 30, 200);
        if (random8() < spawnChance) {
            rippleX[nextRipple] = random8(ledsPerStrip_);
            rippleY[nextRipple] = random8(numStrips_);
            rippleHue[nextRipple] = baseHue + random8(64);
            rippleAge[nextRipple] = 1;
            nextRipple = (nextRipple + 1) % 8;
            spawnCooldown = 3;  // Brief cooldown between spawns
        }
    }

    // Draw and age all active ripples
    for (int r = 0; r < 8; r++) {
        if (rippleAge[r] > 0 && rippleAge[r] < 60) {  // Longer lifespan
            int radius = rippleAge[r];
            uint8_t brightness = 255 - (rippleAge[r] * 4);  // Slower fade

            // Draw ripple ring with thicker edge
            for (int y = 0; y < numStrips_; y++) {
                for (int x = 0; x < ledsPerStrip_; x++) {
                    int dx = x - rippleX[r];
                    int dy = (y - rippleY[r]) * 3;  // Scale Y since rows are fewer
                    int dist = sqrt16(dx * dx + dy * dy);

                    // Wider ring (radius -2 to +2)
                    if (dist >= radius - 2 && dist <= radius + 2) {
                        int idx = xyToIndex(x, y);
                        if (idx >= 0) {
                            uint8_t fade = 255 - abs(dist - radius) * 50;
                            leds_[idx] += CHSV(rippleHue[r], 255, scale8(brightness, fade));
                        }
                    }
                }
            }
            rippleAge[r]++;
        }
    }
}

// New animation: Comet - shooting comets with trails
void LEDManager::animationComet() {
    static int16_t cometPos[3] = {0, 0, 0};          // Position along path
    static int8_t cometRow[3] = {0, 2, 4};           // Which row each comet is on
    static int8_t cometDir[3] = {1, -1, 1};          // Direction: 1 = right, -1 = left
    static uint8_t cometHue[3] = {0, 85, 170};       // Color for each comet

    fadeAll(40);

    for (int c = 0; c < min(3, numStrips_); c++) {
        // Ensure comet row is valid
        if (cometRow[c] >= numStrips_) {
            cometRow[c] = c % numStrips_;
        }

        // Draw comet head and tail
        for (int t = 0; t < 12; t++) {
            int x = cometPos[c] - (t * cometDir[c]);
            if (x >= 0 && x < ledsPerStrip_) {
                int idx = xyToIndex(x, cometRow[c]);
                if (idx >= 0) {
                    uint8_t brightness = 255 - (t * 20);
                    leds_[idx] += CHSV(cometHue[c], 200, brightness);
                }
            }
        }

        // Move comet
        cometPos[c] += cometDir[c] * 2;

        // Bounce or wrap to next row
        if (cometPos[c] >= ledsPerStrip_ + 10) {
            cometPos[c] = ledsPerStrip_ - 1;
            cometDir[c] = -1;
            cometRow[c] = (cometRow[c] + 1) % numStrips_;
            cometHue[c] += 30;
        } else if (cometPos[c] < -10) {
            cometPos[c] = 0;
            cometDir[c] = 1;
            cometRow[c] = (cometRow[c] + 1) % numStrips_;
            cometHue[c] += 30;
        }
    }
}

// New animation: Confetti - audio-reactive colored dots with slow fade
void LEDManager::animationConfetti() {
    static uint8_t confettiHue = 0;

    // Very gentle fade so dots linger and fade out smoothly
    fadeAll(3);

    // Gentler scaling of dots with audio level
    int numDots = 0;
    if (audioLevel_ > 50) {
        // Chance-based spawning scaled by audio
        int spawnChance = map(audioLevel_, 50, 255, 30, 120);
        if (random8() < spawnChance) {
            numDots = 1;
            // Only occasionally spawn 2 on loud hits
            if (audioLevel_ > 180 && random8() < 50) {
                numDots = 2;
            }
        }
    }

    // Brightness scales with audio
    uint8_t dotBright = map(audioLevel_, 0, 255, 180, 255);

    for (int i = 0; i < numDots; i++) {
        int pos = random16(totalLeds_);
        // Saturated colors, white only on very loud hits
        uint8_t sat = (audioLevel_ > 220 && random8() < 25) ? 0 : 255;
        leds_[pos] = CHSV(confettiHue + random8(96), sat, dotBright);
    }

    confettiHue += 1;
}

// Legacy wrapper functions for backward compatibility
// These functions call the appropriate LEDManager methods

void setupFastLED() {
    if (!g_ledManager) {
        g_ledManager = new LEDManager();
    }
    if (g_ledManager) {
        g_ledManager->initialize();
    }
}

void handleLEDs() {
    if (g_ledManager) {
        g_ledManager->update();
    }
}

void fillWhite() {
    if (g_ledManager) {
        g_ledManager->fillWhite();
    }
}

void colorFill(CRGB c) {
    if (g_ledManager) {
        g_ledManager->fillColor(c);
    }
}

void getVuLevels() {
    // VU levels are automatically updated by VuGraph::update() in the main loop
    // This function is kept for compatibility but doesn't need to do anything
    // since the global vuValue array is already being synchronized
}

int getVuForStrip(int strip) {
    if (g_ledManager) {
        return g_ledManager->getVuForStrip(strip);
    }
    return 0;
}

int getNumStrips() {
    if (g_ledManager) {
        return g_ledManager->getNumStrips();
    }
    return 0;
}

int getLedsPerStrip() {
    if (g_ledManager) {
        return g_ledManager->getLedsPerStrip();
    }
    return 0;
}

int getTotalLeds() {
    if (g_ledManager) {
        return g_ledManager->getTotalLeds();
    }
    return 0;
}

bool isLedConfigValid() {
    if (g_ledManager) {
        return g_ledManager->isConfigValid();
    }
    return false;
}