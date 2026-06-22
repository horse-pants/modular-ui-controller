#pragma once

#include <stdint.h>

/// Number of audio analysis bands (MSGEQ7 → 7). Single source of truth shared by
/// the audio task, AudioBus, the VU widget, and the LED animation engine.
static constexpr int AUDIO_BAND_COUNT = 7;

/**
 * @brief One snapshot of the audio analysis.
 *
 * Produced by the audio task (steady ~200 Hz) and consumed by the VU widget on
 * the render task and by LED animations on the loop task. Plain POD, copied
 * whole through AudioBus — never reference shared mutable audio state directly.
 */
struct AudioFrame {
    int bands[AUDIO_BAND_COUNT];  ///< per-band level, 0–255 (filtered)
    int overall;                  ///< overall volume, 0–255 (filtered)
    uint32_t seq;                 ///< publish counter (freshness; spot new frames)
};
