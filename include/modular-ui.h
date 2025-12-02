//UI
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

// Common UI Colors
#define UI_COLOR_PRIMARY        0x00D9FF    // Primary cyan
#define UI_COLOR_PRIMARY_DARK   0x00B8E6    // Darker cyan for gradients/hover
#define UI_COLOR_PRIMARY_DARKER 0x008BB3    // Even darker cyan for bottom gradients
#define UI_COLOR_PRIMARY_DIM    0x006080    // Dimmed cyan for subtle effects
#define UI_COLOR_BACKGROUND     0x0a0a0a    // Dark background
#define UI_COLOR_SURFACE        0x1a1a1a    // Surface/card background
#define UI_COLOR_SURFACE_LIGHT  0x2a2a2a    // Lighter surface
#define UI_COLOR_BORDER         0x444444    // Default border
#define UI_COLOR_TEXT           0xe0e0e0    // Primary text
#define UI_COLOR_TEXT_MUTED     0xb0b0b0    // Muted text
#define UI_COLOR_BLACK          0x000000    // Pure black
#define UI_COLOR_WHITE          0xffffff    // Pure white

#include <FastLED.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <lv_conf.h>
// OLD: #include "BootUI.h"
// NEW: Using library version
#include <WiFiSetupBootUI.h>
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

// OLD: Global BootUI instance (built-in version)
// extern BootUI* g_bootUI;
// NEW: Global WiFiSetupBootUI instance (library version)
extern WiFiSetupBootUI* g_bootUI;
void cleanupBootUI();
void cleanupBrightnessSlider();  

// WiFi - modernized, handled through WiFiManager

// Web UI
void setupWebUi();
void webUiLoop();
void updateWebUi();
