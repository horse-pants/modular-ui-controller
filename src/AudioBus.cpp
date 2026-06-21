#include "AudioBus.h"

// The single shared instance (mirrors the global-managers pattern in main.cpp).
AudioBus g_audioBus;

void AudioBus::publish(const AudioFrame& frame) {
    const uint32_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);          // mark write in progress (odd)
    std::atomic_thread_fence(std::memory_order_release);   // data writes stay after the odd mark

    frame_ = frame;
    frame_.seq = (s / 2) + 1;                              // monotonic publish count

    seq_.store(s + 2, std::memory_order_release);          // mark stable (even); publishes data
}

AudioFrame AudioBus::latest() const {
    AudioFrame out;
    uint32_t before;
    uint32_t after;
    do {
        before = seq_.load(std::memory_order_acquire);
        out = frame_;
        std::atomic_thread_fence(std::memory_order_acquire);
        after = seq_.load(std::memory_order_relaxed);
    } while ((before & 1u) || before != after);            // retry if write straddled the copy
    return out;
}
