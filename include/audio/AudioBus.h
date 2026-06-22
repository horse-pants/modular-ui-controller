#pragma once

#include "audio/AudioFrame.h"
#include <atomic>

/**
 * @brief Single-producer / multi-consumer "latest value wins" hand-off.
 *
 * The audio task publishes; the render task (VU widget) and loop task (LED
 * animations) read. Implemented as a seqlock: a reader retries if a publish
 * overlapped its copy, so a returned AudioFrame is always internally consistent
 * (never a torn mix of old/new bands). Lighter than a queue, and consumers only
 * ever want the freshest sample — not every one.
 *
 * Lock-free: a 32-bit atomic sequence counter, no FreeRTOS primitives, safe to
 * call from any task/core.
 */
class AudioBus {
public:
    /** Publish a new frame (audio task only — single producer). */
    void publish(const AudioFrame& frame);

    /** Return the latest consistent frame (any consumer task). */
    AudioFrame latest() const;

private:
    AudioFrame frame_{};
    std::atomic<uint32_t> seq_{0};  // even = stable, odd = write in progress
};

extern AudioBus g_audioBus;
