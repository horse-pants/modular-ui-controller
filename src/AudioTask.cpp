#include "AudioTask.h"
#include "AudioBus.h"
#include "AudioAnalyzer.h"
#include <Arduino.h>
#include <Filter.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Logger.h>

namespace {

constexpr int NUM_BANDS = 7;
constexpr uint32_t SAMPLE_PERIOD_MS = 5;  // ~200 Hz; held steady by vTaskDelayUntil

TaskHandle_t s_audioTaskHandle = nullptr;

void audioTask(void*) {
    // MSGEQ7 wiring is settled on the WT32-SC01 Plus: strobe 13, reset 21,
    // analog 12 (GPIO12 = ADC2 — shared with WiFi, see WIP ADC2 gotcha).
    Analyzer analyzer(13, 21, 12);
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
