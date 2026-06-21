#pragma once

#include <stdint.h>

/**
 * @brief One snapshot of the audio analysis.
 *
 * Produced by the audio task (steady ~200 Hz) and consumed by the VU widget on
 * the render task and by LED animations on the loop task. Plain POD, copied
 * whole through AudioBus — never reference shared mutable audio state directly.
 */
struct AudioFrame {
    int bands[7];   ///< per-band level, 0–255 (filtered)
    int overall;    ///< overall volume, 0–255 (filtered)
    uint32_t seq;   ///< publish counter (freshness; lets consumers spot new frames)
};
