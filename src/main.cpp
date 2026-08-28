#include "modular-ui.h"
#include "ui/UIManager.h"
#include "WifiBootManager.h"
#include "led/LEDManager.h"
#include "WebUIManager.h"
#include "MqttManager.h"
#include "audio/AudioTask.h"
#include <OTAManager.h>
#include <Logger.h>
#include <esp_system.h>

// Global manager instances
extern UIManager* g_uiManager;
extern LEDManager* g_ledManager;
extern WebUIManager* g_webUIManager;
extern WifiBootManager* g_wifiBootManager;
extern OTAManager* g_otaManager;
extern MqttManager* g_mqttManager;

// Why the last boot happened. On an unexplained reboot this one line separates a
// firmware panic from a task watchdog from a brownout (145 LEDs can pull enough
// to sag a weak supply) — guessing between those costs far more than logging it.
static const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external pin";
        case ESP_RST_SW:        return "software restart";
        case ESP_RST_PANIC:     return "PANIC / exception";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog";
        case ESP_RST_WDT:       return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep sleep wake";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (supply sag)";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "unknown";
    }
}

// Global restart flag for async operations
bool g_restartRequested = false;
unsigned long g_restartTime = 0;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Initialize logger early
    Logger.begin(200, true, true);
    Logger.info("ModularUI Controller Starting...");
    Logger.info("Reset reason: %s", resetReasonName(esp_reset_reason()));

    // PSRAM diagnostic — the full-screen visualiser canvas needs it. If found=0
    // here, the platformio.ini memory_type doesn't match the module's PSRAM.
    Logger.info("PSRAM: found=%d, size=%u bytes, free=%u bytes",
                psramFound() ? 1 : 0,
                (unsigned)ESP.getPsramSize(),
                (unsigned)ESP.getFreePsram());

    // Initialize UI Manager (screen only at first)
    g_uiManager = new UIManager();
    g_uiManager->initializeScreen();

    // Initialize network (WiFi + boot UI)
    g_wifiBootManager = new WifiBootManager();
    g_wifiBootManager->initialize();

    // Initialize full UI and LEDs if not in setup mode
    if (!g_wifiBootManager->isInSetupMode()) {
        g_uiManager->initializeUI();

        g_ledManager = new LEDManager();
        g_ledManager->initialize();
    }

    // Initialize WebUI Manager
    g_webUIManager = new WebUIManager(g_wifiBootManager->getWebServer());
    g_webUIManager->initialize();
    Logger.attachWebSocket(g_webUIManager->getWebSocket());

    // Startup sequence (only if not in setup mode)
    if (g_ledManager) {
        g_ledManager->performStartupDispersion();
        g_uiManager->syncWithLEDState();

        // Home Assistant bridge — needs WiFi (connected in normal mode) and the
        // LED manager. Skipped in captive-portal/setup mode along with the rest.
        g_mqttManager = new MqttManager(g_ledManager);
        g_mqttManager->begin();
    }

    // Hand LVGL ownership to the render task — only in normal mode, and only
    // after all startup lv_* (syncWithLEDState) has completed so nothing races
    // the task during boot. In setup mode the boot UI keeps driving LVGL on the
    // loop, so no render task is created.
    if (!g_wifiBootManager->isInSetupMode()) {
        // Audio task first so frames are flowing before the render task and LED
        // animations start pulling from g_audioBus.
        startAudioTask();
        g_uiManager->startRenderTask();
    }
}

void loop()
{
    // LVGL tick comes from lv_tick_set_cb(millis) registered in
    // UIManager::initializeScreen() — no manual lv_tick_inc here.

    // LVGL ownership:
    //  - Normal mode: the render task owns all lv_* (started at end of setup).
    //    The loop must NOT touch LVGL here — it only runs the non-UI managers.
    //  - Setup mode / no UI manager: no render task, so drive LVGL on the loop
    //    as before to keep the boot UI alive.
    if (!(g_uiManager && g_uiManager->isRenderTaskRunning())) {
        if (g_uiManager) {
            g_uiManager->update();
            if (g_wifiBootManager && g_wifiBootManager->isInSetupMode()) {
                lv_timer_handler();
            }
        } else {
            lv_timer_handler();
        }
    }

    if (g_wifiBootManager) g_wifiBootManager->update();
    if (g_ledManager) g_ledManager->update();
    if (g_webUIManager) g_webUIManager->update();
    if (g_mqttManager) g_mqttManager->update();
    if (g_otaManager) g_otaManager->loop();

    // With the render task owning LVGL, the loop no longer blocks on the flush.
    // LEDManager::update()'s driver_.show() paces us when strips are configured,
    // but yield explicitly so loopTask never 100%-spins core 1 (starving IDLE1)
    // when LEDs are absent. 1 ms cap is far above any LED frame rate.
    if (g_uiManager && g_uiManager->isRenderTaskRunning()) {
        vTaskDelay(1);
    }

    // Handle restart requests
    if (g_restartRequested && g_restartTime == 0) {
        g_restartTime = millis();
    }
    if (g_restartTime > 0 && millis() - g_restartTime > 2000) {
        ESP.restart();
    }
}
