#pragma once

#include <lvgl.h>
#include <Arduino.h>  // For String class

// Forward declarations
class BrightnessSlider;
class ColourWheel;
class ColourTab;
class EffectGrid;
class WhiteButton;
class VuButton;
class VuGraph;
class UIManager;

// Global UI manager instance
extern UIManager* g_uiManager;

// Global component instances (for components that still need direct access)
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;
extern EffectGrid* g_effectGrid;
extern WhiteButton* g_whiteButton;
extern VuButton* g_vuButton;
extern VuGraph* g_vuGraph;

// The Colour tab, so the Effects tab can keep its effect bar in step without
// either one having to know about UIManager.
extern ColourTab* g_colourTab;
