//UI
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

#include <FastLED.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <lv_conf.h>
#include "BootUI.h"
#include "BrightnessSlider.h"
#include "LEDManager.h"
#include "WebUIManager.h"

// FastLED
#define ARC_WIDTH_THICK LV_MAX(LV_DPI_DEF / 5, 5)

// Legacy compatibility - these macros now use the global LEDManager instance
#define NUM_STRIPS (g_ledManager ? g_ledManager->getNumStrips() : 0)
#define NUM_PER_STRIP (g_ledManager ? g_ledManager->getLedsPerStrip() : 0)
#define NUM_LEDS (g_ledManager ? g_ledManager->getTotalLeds() : 0)

// Legacy compatibility - global state variables are maintained for backward compatibility
extern uint8_t brightness;
extern bool showAnimation;
extern bool vu;
extern bool white;
extern LEDManager::AnimationType currentAnimation;
extern int vuValue[7];
extern int audioLevel;

// Legacy animation enum - now typedef'd to the class enum
typedef LEDManager::AnimationType animationOptions;

// Legacy function names for backward compatibility - these will call LEDManager methods
void setupFastLED();
void handleLEDs();
void fillWhite();
void colorFill(CRGB c);
void getVuLevels();
int getVuForStrip(int strip);
int getNumStrips();
int getLedsPerStrip(); 
int getTotalLeds();
bool isLedConfigValid();


// UI - modernized, setup handled directly through UIManager in main.cpp

// Global BootUI instance - will be replaced with proper dependency injection later
extern BootUI* g_bootUI;
void cleanupBootUI();
void cleanupBrightnessSlider();  

// WiFi - modernized, handled through WiFiManager

// Web UI
void setupWebUi();
void webUiLoop();
void updateWebUi();
