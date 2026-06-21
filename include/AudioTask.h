#pragma once

/**
 * @brief Start the audio sampling task (core 0).
 *
 * Owns the MSGEQ7 analyzer and the per-band ExponentialFilters, samples at a
 * steady cadence via vTaskDelayUntil (jitter-free), and publishes AudioFrames to
 * g_audioBus. Call once, in normal mode. The VU widget and LED animations are
 * pure consumers of g_audioBus — they no longer sample.
 */
void startAudioTask();

/**
 * @brief Suspend / resume the audio task.
 *
 * Used during OTA: the task samples GPIO12 (ADC2, shared with WiFi) on core 0,
 * and under WiFi-saturated OTA traffic that read contends badly and helps starve
 * IDLE0 (task watchdog). Suspending it during the firmware write removes that
 * core-0 load. Safe because LEDManager stops reading g_audioBus in OTA mode and
 * the visualiser isn't active, so nothing consumes audio frames meanwhile.
 */
void pauseAudioTask();
void resumeAudioTask();
