#include "modular-ui.h"
#include "UIManager.h"
// OLD: #include "WiFiManager.h"
// OLD: BootUI* g_bootUI = nullptr;
// NEW: Using library version
#include <WiFiSetupManager.h>
#include <WiFiSetupBootUI.h>
#include <OTAManager.h>
#include <Logger.h>
#include "LEDManager.h"
#include "WebUIManager.h"
#include <memory>

// Global UI manager and component instances
extern UIManager* g_uiManager;
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;
extern EffectsList* g_effectsList;
extern WhiteButton* g_whiteButton;
extern VuButton* g_vuButton;
extern VuGraph* g_vuGraph;

// OLD: Global WiFi manager instance (old built-in version)
// extern WiFiManager* g_wifiManager;
// NEW: Global WiFi setup manager instance (library version)
WiFiSetupManager* g_wifiManager = nullptr;

// Global LED manager instance
extern LEDManager* g_ledManager;

// Global WebUI manager instance
extern WebUIManager* g_webUIManager;

// OLD: Global BootUI instance (old built-in version)
// BootUI* g_bootUI = nullptr;
// NEW: Global WiFiSetupBootUI instance (library version)
WiFiSetupBootUI* g_bootUI = nullptr;

// Global restart flag for async operations
bool g_restartRequested = false;
unsigned long g_restartTime = 0;

void setup(void)
{
  Serial.begin(115200);
  delay(1000);

  // Initialize logger early (before WiFi so we can log WiFi setup)
  Logger.begin(200, true, true);  // 200 log entries, Serial enabled, WebSocket enabled
  Logger.info("ModularUI Controller Starting...");

  // Initialize UI Manager
  if (!g_uiManager) {
    g_uiManager = new UIManager();
  }

  if (g_uiManager) {
    // Always initialize the basic screen for LVGL
    g_uiManager->initializeScreen();

    // OLD: Initialize BootUI (built-in version)
    // g_bootUI = new BootUI();
    // if (g_bootUI && g_bootUI->initialize()) {
    //   g_bootUI->addText("ModularUI Controller Starting...\r\n");
    // }

    // NEW: Initialize WiFiSetupBootUI (library version)
    g_bootUI = new WiFiSetupBootUI();

    // NEW: Create theme matching your UI colors (cyan theme)
    static WiFiSetupTheme cyanTheme;
    cyanTheme.primaryColor = UI_COLOR_PRIMARY;           // 0x00D9FF cyan
    cyanTheme.backgroundColor = UI_COLOR_BACKGROUND;     // 0x0a0a0a
    cyanTheme.surfaceColor = UI_COLOR_SURFACE;           // 0x1a1a1a
    cyanTheme.surfaceLight = UI_COLOR_SURFACE_LIGHT;     // 0x2a2a2a
    cyanTheme.textColor = UI_COLOR_TEXT;                 // 0xe0e0e0
    cyanTheme.borderColor = UI_COLOR_BORDER;             // 0x444444
    // Web UI colors (for future web integration)
    cyanTheme.webPrimaryColor = "#00D9FF";
    cyanTheme.webPrimaryDark = "#00B8E6";
    cyanTheme.webBackgroundColor = "#0a0a0a";
    cyanTheme.webSurfaceColor = "#1a1a1a";
    cyanTheme.webTextColor = "#e0e0e0";
    cyanTheme.webTextSecondary = "#b0b0b0";
    cyanTheme.webBorderColor = "#444444";

    // NEW: Create WiFi Setup Manager config (library version with callback)
    if (!g_wifiManager) {
      WiFiSetupConfig config;
      config.defaultAPName = "ModularUI-Setup";
      config.defaultAPPassword = "modularui123";
      config.statusCallback = g_bootUI; // Use boot UI as callback
      config.theme = &cyanTheme;        // Use your cyan theme!
      g_wifiManager = new WiFiSetupManager(config);

      // Register Logger endpoints BEFORE starting WiFi (before web server begins)
      Logger.registerEndpoints(g_wifiManager->getWebServer());
      Logger.info("Logger web endpoints registered");
    }

    // Initialize boot UI with cyan theme
    if (g_bootUI && g_wifiManager) {
      // Pass theme to match your UI - screen size auto-detected (320x480)
      // For other displays, specify dimensions: bootUI->initialize("TITLE", &theme, 536, 240)
      if (g_bootUI->initialize("MODULAR UI CONTROLLER", &cyanTheme)) {
        g_bootUI->addText("ModularUI Controller Starting...\r\n");
      }
    }

    // OLD: Initialize WiFi Manager (built-in version)
    // if (!g_wifiManager) {
    //   g_wifiManager = new WiFiManager();
    // }
    // if (g_wifiManager) {
    //   g_wifiManager->initialize();
    // }

    // Start WiFi setup
    if (g_wifiManager) {
      g_wifiManager->begin();
      Logger.info("Logger web interface available at /logs");
    }

    // Only initialize full UI components and LED manager if not in setup mode
    if (g_wifiManager && !g_wifiManager->isInSetupMode()) {
      // Initialize full UI components
      if (g_uiManager->initializeUI()) {
        // Update global pointers for components that still need them
        g_brightnessSlider = g_uiManager->getBrightnessSlider();
        g_colourWheel = g_uiManager->getColourWheel();
        g_effectsList = g_uiManager->getEffectsList();
        g_whiteButton = g_uiManager->getWhiteButton();
        g_vuButton = g_uiManager->getVuButton();
        g_vuGraph = g_uiManager->getVuGraph();
      }
      
      // Initialize LED Manager
      if (!g_ledManager) {
        g_ledManager = new LEDManager();
      }
      if (g_ledManager) {
        g_ledManager->initialize();
      }
    }
    // If in setup mode, the WiFi manager will show the AP setup screen
  }
  
  // Initialize WebUI Manager with shared web server from WiFiSetupManager
  if (!g_webUIManager && g_wifiManager) {
    g_webUIManager = new WebUIManager(g_wifiManager->getWebServer());
  }
  if (g_webUIManager) {
    g_webUIManager->initialize();

    // Attach WebSocket to Logger for real-time log broadcasting
    Logger.attachWebSocket(g_webUIManager->getWebSocket());
  }
  
  // Perform startup fade-in after all initialization is complete (only if not in setup mode)
  if (g_ledManager && g_wifiManager && !g_wifiManager->isInSetupMode()) {
    g_ledManager->performStartupFadeIn();
  }
}

void loop()
{
  // LVGL 9 requires tick updates for proper timing
  static uint32_t lastTick = 0;
  uint32_t currentMillis = millis();
  lv_tick_inc(currentMillis - lastTick);
  lastTick = currentMillis;

  // Update UI manager (includes VU graph updates and LVGL timer)
  if (g_uiManager) {
    g_uiManager->update();

    // If in setup mode, UI manager might not be fully initialized,
    // so ensure LVGL timer runs anyway
    if (g_wifiManager && g_wifiManager->isInSetupMode()) {
      lv_timer_handler();
    }
  } else {
    lv_timer_handler(); /* let the GUI do its work */
  }
  
  // Update WiFi manager (handles DNS in AP mode)
  if (g_wifiManager) {
    g_wifiManager->update();
  }
  
  // Update LED manager
  if (g_ledManager) {
    g_ledManager->update();
  }
  
  // Update WebUI manager
  if (g_webUIManager) {
    g_webUIManager->update();
  }

  // Update OTA manager
  if (g_otaManager) {
    g_otaManager->loop();
  }

  // Handle restart requests from async operations
  if (g_restartRequested && g_restartTime == 0) {
    g_restartTime = millis();
  }
  if (g_restartTime > 0 && millis() - g_restartTime > 2000) {
    ESP.restart();
  }
}

// Cleanup functions - called when needed
// OLD: Cleanup for built-in BootUI
// void cleanupBootUI() {
//   if (g_bootUI) {
//     delete g_bootUI;
//     g_bootUI = nullptr;
//   }
// }

// NEW: Cleanup for library WiFiSetupBootUI
void cleanupBootUI() {
  if (g_bootUI) {
    g_bootUI->cleanup(); // Call cleanup method
    delete g_bootUI;
    g_bootUI = nullptr;
  }
}

void cleanupBrightnessSlider() {
  if (g_brightnessSlider) {
    delete g_brightnessSlider;
    g_brightnessSlider = nullptr;
  }
}
