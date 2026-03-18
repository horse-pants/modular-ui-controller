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

// Static animation descriptions (🎵 = audio reactive)
const char* LEDManager::animationDescriptions_[] = {
    "Rainbow",
    "Cylon",
    "RGB Chaser",
    "Beat Sine",
    "Ice Waves (A)",
    "Purple Rain (A)",
    "Fire (A)",
    "Matrix (A)",
    "VU (A)"
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
    
    // Keep legacy variables in sync
    brightness = brightness_;
    showAnimation = showAnimation_;
    vu = vuMode_;
    white = whiteMode_;
    currentAnimation = currentAnimation_;
}

void LEDManager::setBrightness(uint8_t brightness) {
    brightness_ = brightness;
    brightness = brightness_; // Sync legacy global
}

void LEDManager::performStartupFadeIn() {
    if (!initialized_ || !isConfigValid() || !leds_) {
        return;
    }

    // Startup sequence - fade in red from black
    // Ensure we have a minimum brightness for the fade-in
    uint8_t targetBrightness = brightness_;
    if (targetBrightness < 64) {
        targetBrightness = 128; // Default to 128 if somehow set too low
    }

    fillColor(CRGB::Red);
    for (int i = 0; i <= targetBrightness; i++) {
        FastLED.setBrightness(i);
        FastLED.show();
        delay(25);
    }

    // Keep red color visible after fade-in
    fillColor(CRGB::Red);
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
        case CYLON: return 30;
        case RAINBOW: return 10;
        case RGBCHASER: return 30;
        case BEATSINE: return 50;
        case ICEWAVES: return 20;
        case PURPLERAIN: return 20;
        case FIRE: return 20;
        case MATRIX: return 50;
        case VU: return 5;
        default: return 100;
    }
}

void LEDManager::runAnimation() {
    switch (currentAnimation_) {
        case RAINBOW: animationRainbow(); break;
        case CYLON: animationCylon(); break;
        case RGBCHASER: animationRgbChaser(); break;
        case BEATSINE: animationBeatSine(); break;
        case ICEWAVES: animationIceWaves(); break;
        case PURPLERAIN: animationPurpleRain(); break;
        case FIRE: animationFire(); break;
        case MATRIX: animationMatrix(); break;
        case VU: animationVu(); break;
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