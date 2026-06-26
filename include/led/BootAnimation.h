#pragma once

#include "led/LedHelpers.h"
#include "led/Animations.h"

class LedDriver;
class AnimationEngine;

/**
 * @brief One-shot boot visual for the LED strips.
 *
 * A diagonal rainbow-field "dispersion wipe" sweeps top-left → bottom-right across
 * the strip grid, then cross-fades from the final dispersion frame into the saved
 * state (solid colour / white / animation). Blocking on purpose: it runs once in
 * setup() before the render task takes over LVGL, mirroring the old fade-in.
 *
 * Kept out of LEDManager (which would otherwise creep back toward a god object) —
 * it only needs the driver, the engine, and a snapshot of the saved control state.
 */
namespace BootAnimation {

void runDispersion(LedDriver& driver, AnimationEngine& engine, CRGB* leds,
                   int numStrips, int ledsPerStrip, int totalLeds,
                   uint8_t brightness, bool showAnimation, bool whiteMode,
                   AnimationType currentAnimation, const CRGB& solidColor);

}  // namespace BootAnimation
