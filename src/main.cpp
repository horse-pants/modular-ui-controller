#include "modular-ui.h"
#include "UIManager.h"
#include "WiFiManager.h"
#include "LEDManager.h"
#include "WebUIManager.h"
#include <ElegantOTA.h>
#include <memory>

// Global UI manager and component instances
extern UIManager* g_uiManager;
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;
extern EffectsList* g_effectsList;
extern WhiteButton* g_whiteButton;
extern VuButton* g_vuButton;
extern VuGraph* g_vuGraph;

// Global WiFi manager instance
extern WiFiManager* g_wifiManager;

// Global LED manager instance
extern LEDManager* g_ledManager;

// Global WebUI manager instance
extern WebUIManager* g_webUIManager;

// Global BootUI instance
BootUI* g_bootUI = nullptr;

// Global restart flag for async operations
bool g_restartRequested = false;
unsigned long g_restartTime = 0;

void setup(void)
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("started...");

  // Initialize UI Manager
  if (!g_uiManager) {
    g_uiManager = new UIManager();
  }
  
  if (g_uiManager) {
    // Always initialize the basic screen for LVGL
    g_uiManager->initializeScreen();
    
    // Initialize BootUI
    g_bootUI = new BootUI();
    if (g_bootUI && g_bootUI->initialize()) {
      g_bootUI->addText("ModularUI Controller Starting...\r\n");
    }
    
    // Initialize WiFi Manager
    if (!g_wifiManager) {
      g_wifiManager = new WiFiManager();
    }
    if (g_wifiManager) {
      g_wifiManager->initialize();
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
  
  // Initialize WebUI Manager
  if (!g_webUIManager) {
    g_webUIManager = new WebUIManager();
  }
  if (g_webUIManager) {
    g_webUIManager->initialize();
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
  
  // Handle restart requests from async operations
  if (g_restartRequested && g_restartTime == 0) {
    g_restartTime = millis();
  }
  if (g_restartTime > 0 && millis() - g_restartTime > 2000) {
    ESP.restart();
  }
  
  ElegantOTA.loop();
}

// Cleanup functions - called when needed
void cleanupBootUI() {
  if (g_bootUI) {
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
