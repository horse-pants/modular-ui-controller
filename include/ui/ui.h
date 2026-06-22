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

// Global UI manager instance
extern UIManager* g_uiManager;

// Global component instances (for components that still need direct access)
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;
extern EffectsList* g_effectsList;
extern WhiteButton* g_whiteButton;
extern VuButton* g_vuButton;
extern VuGraph* g_vuGraph;

