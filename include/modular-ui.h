//UI
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

// =============================================================================
// COLOR PALETTE - All colors defined here for easy theming
// =============================================================================

// Primary accent (cyan neon)
#define UI_COLOR_PRIMARY        0x00D9FF    // Primary cyan
#define UI_COLOR_PRIMARY_DARK   0x00B8E6    // Darker cyan for gradients
#define UI_COLOR_PRIMARY_DARKER 0x008BB3    // Even darker cyan
#define UI_COLOR_PRIMARY_DIM    0x006080    // Dimmed cyan for subtle effects
#define UI_COLOR_PRIMARY_GLOW   0x00D9FF    // Glow color (same as primary)

// Secondary accent (magenta - for highlights/contrast)
#define UI_COLOR_ACCENT         0xFF0080    // Magenta accent
#define UI_COLOR_ACCENT_DARK    0xCC0066    // Darker magenta

// Backgrounds & surfaces
#define UI_COLOR_BACKGROUND     0x0A0A0F    // Near-black with slight blue tint
#define UI_COLOR_SURFACE        0x15151F    // Panel/card surface
#define UI_COLOR_SURFACE_LIGHT  0x252530    // Lighter surface (hover states)
#define UI_COLOR_SURFACE_DARK   0x0D0D12    // Darker inset areas

// Borders & dividers
#define UI_COLOR_BORDER         0x333340    // Default border
#define UI_COLOR_BORDER_LIGHT   0x444455    // Lighter border (focus)
#define UI_COLOR_BORDER_GLOW    0x00D9FF    // Glowing border (active)

// Text
#define UI_COLOR_TEXT           0xE0E0E0    // Primary text
#define UI_COLOR_TEXT_MUTED     0x808090    // Muted/secondary text
#define UI_COLOR_TEXT_DIM       0x505060    // Dimmed labels

// VU Meter colors (gradient from low to peak)
#define UI_COLOR_VU_GREEN       0x00FF66    // Low level
#define UI_COLOR_VU_YELLOW      0xFFFF00    // Mid level
#define UI_COLOR_VU_ORANGE      0xFF8800    // High level
#define UI_COLOR_VU_RED         0xFF3333    // Peak/clip

// Slider track colors
#define UI_COLOR_TRACK          0x252530    // Unfilled track
#define UI_COLOR_TRACK_LIGHT    0x353545    // Track gradient end

// Basic colors
#define UI_COLOR_BLACK          0x000000
#define UI_COLOR_WHITE          0xFFFFFF

// =============================================================================
// LAYOUT CONSTANTS - Spacing, sizing, etc.
// =============================================================================

// Padding & margins
#define UI_PADDING_SMALL        4
#define UI_PADDING_MEDIUM       8
#define UI_PADDING_LARGE        12
#define UI_PADDING_XLARGE       16

// Component spacing
#define UI_SPACING_TIGHT        6
#define UI_SPACING_NORMAL       10
#define UI_SPACING_LOOSE        16

// Border radius
#define UI_RADIUS_SMALL         4
#define UI_RADIUS_MEDIUM        8
#define UI_RADIUS_LARGE         12
#define UI_RADIUS_ROUND         999     // Fully round

// Border widths
#define UI_BORDER_THIN          1
#define UI_BORDER_NORMAL        2
#define UI_BORDER_THICK         3

// Button sizes
#define UI_BTN_HEIGHT           36
#define UI_BTN_HEIGHT_SMALL     30
#define UI_BTN_MIN_WIDTH        80
#define UI_BTN_COMPACT_WIDTH    56      // Narrow buttons (VU, WHITE)

// =============================================================================

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
