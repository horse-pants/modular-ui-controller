#include "WebApi.h"

#include <LittleFS.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Logger.h>
#include "led/LEDManager.h"
#include "ui/UIManager.h"
#include "ui/ui.h"

// Defined in main.cpp — set true to schedule a deferred restart from a handler
// (the loop performs the actual ESP.restart() after a short delay).
extern bool g_restartRequested;

void WebApi::registerRoutes(AsyncWebServer* server) {
    Logger.debug("=== WebApi: Setting up routes ===");

    // NOTE: WiFi setup routes (/setup, /factory-reset, /get-networks, /save-wifi)
    // are handled by the WiFiSetupManager library, not here.

    server->on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Server is working! IP: " + WiFi.localIP().toString());
    });

    // LED Configuration page — served by the Svelte SPA. We send the gzipped
    // index.html and let the Svelte router show the LedConfig view based on
    // window.location.pathname. AsyncFileResponse auto-detects the .gz sibling.
    server->on("/led-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/web/index.html", "text/html");
    });

    server->on("/get-led-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        Preferences ledPrefs;
        ledPrefs.begin("led-config", true); // Read-only

        bool hasLedSettings = ledPrefs.isKey("num_strips");
        int numStrips = ledPrefs.getInt("num_strips", 0);
        int ledsPerStrip = ledPrefs.getInt("leds_per_strip", 0);
        int totalLeds = numStrips * ledsPerStrip;

        ledPrefs.end();

        String json = "{";
        json += "\"hasLedSettings\":" + String(hasLedSettings ? "true" : "false") + ",";
        json += "\"numStrips\":" + String(numStrips) + ",";
        json += "\"ledsPerStrip\":" + String(ledsPerStrip) + ",";
        json += "\"totalLeds\":" + String(totalLeds);
        json += "}";

        request->send(200, "application/json", json);
    });

    server->on("/save-led-config", HTTP_POST, [](AsyncWebServerRequest* request) {
        int numStrips = 0;
        int ledsPerStrip = 0;

        if (request->hasParam("num_strips", true)) {
            numStrips = request->getParam("num_strips", true)->value().toInt();
        }
        if (request->hasParam("leds_per_strip", true)) {
            ledsPerStrip = request->getParam("leds_per_strip", true)->value().toInt();
        }

        if (numStrips > 0 && ledsPerStrip > 0) {
            Preferences ledPrefs;
            ledPrefs.begin("led-config", false);
            ledPrefs.putInt("num_strips", numStrips);
            ledPrefs.putInt("leds_per_strip", ledsPerStrip);
            ledPrefs.end();

            Logger.info("Saved LED config: %d strips, %d LEDs per strip", numStrips, ledsPerStrip);

            request->send(200, "application/json", "{\"ok\":true}");

            // Schedule restart — the Svelte client shows the "Restarting..." view.
            g_restartRequested = true;
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid LED configuration\"}");
        }
    });

    // Settings: idle screensaver (enable + idle timeout). Applied live, no restart.
    server->on("/get-settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        bool enabled = g_uiManager ? g_uiManager->isScreensaverEnabled() : true;
        uint32_t idleSec = g_uiManager ? g_uiManager->getScreensaverIdleMs() / 1000 : 30;

        String json = "{";
        json += "\"screensaverEnabled\":" + String(enabled ? "true" : "false") + ",";
        json += "\"screensaverTimeoutSec\":" + String(idleSec);
        json += "}";

        request->send(200, "application/json", json);
    });

    server->on("/save-settings", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!g_uiManager) {
            request->send(503, "application/json", "{\"ok\":false,\"error\":\"UI not ready\"}");
            return;
        }

        // Default to the current values so a partial form doesn't clobber them.
        bool enabled = g_uiManager->isScreensaverEnabled();
        uint32_t idleSec = g_uiManager->getScreensaverIdleMs() / 1000;

        if (request->hasParam("screensaver_enabled", true)) {
            String v = request->getParam("screensaver_enabled", true)->value();
            enabled = (v == "true" || v == "1" || v == "on");
        }
        if (request->hasParam("screensaver_timeout_sec", true)) {
            idleSec = request->getParam("screensaver_timeout_sec", true)->value().toInt();
        }

        g_uiManager->setScreensaverConfig(enabled, idleSec * 1000);
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // Clear saved LED state (for testing first-boot experience)
    server->on("/clear-led-state", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (g_ledManager) {
            g_ledManager->clearSavedState();
        }

        Logger.info("LED state preferences cleared via web UI");
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // Legacy get-message endpoint (mostly unused)
    server->on("/get-message", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "application/json", "");
    });

    // Svelte-built UI lives under /web/ and is gzipped. AsyncStaticWebHandler
    // auto-detects .gz siblings, so a request for /app.js transparently serves
    // /web/app.js.gz with Content-Encoding: gzip. Root (/) falls back to
    // index.html. Registered last so the specific routes above win.
    server->serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    Logger.debug("=== WebApi: Routes setup complete ===");
}
