#include "modular-ui.h"
#include "UIManager.h"
#include "WifiBootManager.h"
#include "LEDManager.h"
#include "WebUIManager.h"
#include <OTAManager.h>
#include <Logger.h>

// Global manager instances
extern UIManager* g_uiManager;
extern LEDManager* g_ledManager;
extern WebUIManager* g_webUIManager;
extern WifiBootManager* g_wifiBootManager;
extern OTAManager* g_otaManager;

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
        g_ledManager->performStartupFadeIn();
        g_uiManager->syncWithLEDState();
    }
}

void loop()
{
    // LVGL tick updates
    static uint32_t lastTick = 0;
    uint32_t currentMillis = millis();
    lv_tick_inc(currentMillis - lastTick);
    lastTick = currentMillis;

    // Update managers
    if (g_uiManager) {
        g_uiManager->update();
        if (g_wifiBootManager && g_wifiBootManager->isInSetupMode()) {
            lv_timer_handler();
        }
    } else {
        lv_timer_handler();
    }

    if (g_wifiBootManager) g_wifiBootManager->update();
    if (g_ledManager) g_ledManager->update();
    if (g_webUIManager) g_webUIManager->update();
    if (g_otaManager) g_otaManager->loop();

    // Handle restart requests
    if (g_restartRequested && g_restartTime == 0) {
        g_restartTime = millis();
    }
    if (g_restartTime > 0 && millis() - g_restartTime > 2000) {
        ESP.restart();
    }
}
