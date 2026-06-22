#include "audio/AudioTask.h"
#include "audio/AudioBus.h"
#include "audio/AudioAnalyzer.h"
#include "pins.h"
#include <Arduino.h>
#include <audio/Filter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Logger.h>

namespace {

constexpr int NUM_BANDS = AUDIO_BAND_COUNT;  // shared constant (see AudioFrame.h)
constexpr uint32_t SAMPLE_PERIOD_MS = 5;  // ~200 Hz; held steady by vTaskDelayUntil

TaskHandle_t s_audioTaskHandle = nullptr;

void audioTask(void*) {
    // MSGEQ7 wiring (settled) — see pins.h. GPIO12 = ADC2 (WiFi-shared; the task
    // is paused during OTA to avoid ADC2/WiFi contention).
    Analyzer analyzer(pins::MSGEQ7_STROBE, pins::MSGEQ7_RESET, pins::MSGEQ7_ANALOG);
    analyzer.Init();

    // Filters live here now (one home — consumers must NOT re-filter). Weights
    // match the previous VuGraph values so the meter feel is unchanged.
    ExponentialFilter<int> bandFilters[NUM_BANDS] = {
        ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0),
        ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0),
        ExponentialFilter<int>(10, 0), ExponentialFilter<int>(10, 0),
        ExponentialFilter<int>(10, 0),
    };
    ExponentialFilter<int> overallFilter(10, 0);

    const TickType_t period = pdMS_TO_TICKS(SAMPLE_PERIOD_MS);
    TickType_t lastWake = xTaskGetTickCount();

    for (;;) {
        int raw[NUM_BANDS];
        analyzer.ReadFreq(raw);  // blocking MSGEQ7 read (~546 us) — this task's own time now

        AudioFrame frame{};
        int total = 0;
        for (int i = 0; i < NUM_BANDS; i++) {
            const int mapped = map(raw[i], 0, 4096, 0, 255);
            bandFilters[i].Filter(mapped);
            frame.bands[i] = bandFilters[i].Current();
            total += frame.bands[i];
        }
        overallFilter.Filter(total / NUM_BANDS);
        frame.overall = overallFilter.Current();

        g_audioBus.publish(frame);

        vTaskDelayUntil(&lastWake, period);
    }
}

}  // namespace

void pauseAudioTask() {
    if (s_audioTaskHandle) {
        vTaskSuspend(s_audioTaskHandle);
    }
}

void resumeAudioTask() {
    if (s_audioTaskHandle) {
        vTaskResume(s_audioTaskHandle);
    }
}

void startAudioTask() {
    if (s_audioTaskHandle) {
        return;  // already running
    }
    // Core 0 (next to WiFi/AsyncTCP). Small stack — just the analyzer read +
    // filters. Priority 1; vTaskDelayUntil keeps the cadence steady regardless.
    const BaseType_t ok = xTaskCreatePinnedToCore(
        audioTask, "audio", 4096, nullptr, 1, &s_audioTaskHandle, 0);
    if (ok != pdPASS) {
        s_audioTaskHandle = nullptr;
        Logger.error("Failed to create audio task");
    } else {
        Logger.info("Audio task started (core 0, ~200 Hz)");
    }
}
