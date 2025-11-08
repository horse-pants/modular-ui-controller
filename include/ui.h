#pragma once

#include <lvgl.h>
#include <Arduino.h>  // For String class

// Forward declarations
class BrightnessSlider;
class ColourWheel;
class EffectsList;
class WhiteButton;
class VuButton;
class VuGraph;
class UIManager;

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

// Global UI manager instance
extern UIManager* g_uiManager;

// Global component instances (for components that still need direct access)
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;
extern EffectsList* g_effectsList;
extern WhiteButton* g_whiteButton;
extern VuButton* g_vuButton;
extern VuGraph* g_vuGraph;

