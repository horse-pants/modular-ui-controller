#pragma once

#include <ESPAsyncWebServer.h>

/**
 * @brief Registers the app's HTTP routes + REST API on the shared web server.
 *
 * Split out of WebUIManager: these handlers are stateless — they touch only the
 * global managers (g_ledManager / g_uiManager), NVS, and LittleFS — so they live
 * as free functions rather than WebUIManager methods. The WiFi-setup routes
 * (/setup, /save-wifi, /factory-reset, …) are owned by the WiFiSetupManager
 * library, not here.
 */
namespace WebApi {

/// Register /test, the LED-config + settings REST API, and the static SPA.
void registerRoutes(AsyncWebServer* server);

}  // namespace WebApi
