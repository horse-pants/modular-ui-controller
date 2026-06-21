#include "WebUIManager.h"
#include "UIManager.h"
#include "ui.h"
#include <WiFiSetupManager.h>
#include <OTAManager.h>
#include <Logger.h>
#include "LEDManager.h"
#include "ColourWheel.h"
#include "LedHelpers.h"  // CRGB color constants
#include "AudioTask.h"   // pause/resume audio task around OTA

// Global manager instances
extern LEDManager* g_ledManager;

// Global WebUI manager instance
WebUIManager* g_webUIManager = nullptr;

// Global OTA manager instance
OTAManager* g_otaManager = nullptr;

WebUIManager::WebUIManager(AsyncWebServer* webServer)
    : initialized_(false)
    , server_(webServer)
    , webSocket_("/ws")
{
}

WebUIManager::~WebUIManager() {
    // AsyncWebSocket cleanup is handled automatically
    // Note: server_ is NOT owned by us, so we don't delete it
}

bool WebUIManager::initialize() {
    if (initialized_) {
        return true;
    }

    if (!server_) {
        Logger.error("WebUIManager: No web server provided!");
        return false;
    }

    if (!initializeLittleFS()) {
        Logger.error("Failed to initialize LittleFS");
        return false;
    }

    initializeWebSocket();
    setupRoutes();
    setupAPIEndpoints();
    setupStaticFiles();

    // Initialize OTA Manager with callbacks
    if (!g_otaManager) {
        g_otaManager = new OTAManager();
    }

    if (g_otaManager) {
        // Set start callback
        g_otaManager->setStartCallback([]() {
            // Take the LED strips out of normal update() so the OTA progress
            // display owns them — otherwise the main loop slams brightness
            // back to the user's saved value between progress ticks and the
            // bar flickers the whole way.
            if (g_ledManager) {
                g_ledManager->setAnimationEnabled(false);
                g_ledManager->setOTAMode(true);  // stops LEDManager reading g_audioBus
            }
            // Suspend the core-0 audio task: its ADC2 (WiFi-shared) reads contend
            // with the OTA traffic and help starve IDLE0 (task watchdog). Done
            // after setOTAMode so no one is consuming audio frames.
            pauseAudioTask();
        });

        // Set LED progress callback
        g_otaManager->setLEDProgressCallback([](uint8_t progress) {
            if (g_ledManager) {
                g_ledManager->showOTAProgress(progress);
            }
        });

        // Set screen progress callback
        g_otaManager->setScreenProgressCallback([](uint8_t progress, OTAManager::Stage stage) {
            if (g_uiManager) {
                if (stage == OTAManager::Stage::STARTING) {
                    g_uiManager->showOTAScreen();
                } else if (stage == OTAManager::Stage::IN_PROGRESS || stage == OTAManager::Stage::COMPLETE) {
                    g_uiManager->updateOTAProgress(progress);
                } else if (stage == OTAManager::Stage::FAILED) {
                    g_uiManager->hideOTAScreen();
                }
            }
        });

        // Set end callback for failure handling
        g_otaManager->setEndCallback([](bool success) {
            if (!success) {
                // Flash red on failure — keep otaMode_ on so update() doesn't
                // fight the flash, then release the strips back to normal.
                if (g_ledManager) {
                    for (int i = 0; i < 3; i++) {
                        g_ledManager->fillColor(CRGB::Red);
                        delay(200);
                        g_ledManager->fillColor(CRGB::Black);
                        delay(200);
                    }
                    g_ledManager->setOTAMode(false);
                }
                // OTA failed (no reboot) — bring the audio task back.
                resumeAudioTask();
            }
            // On success the device restarts, so the flag resets implicitly.
        });

        // Initialize OTA with the shared server
        g_otaManager->begin(server_, "", "", "spiffs");
    }

    // NOTE: server_->begin() is called by WiFiSetupManager, not here
    Logger.info("Web UI Manager initialized successfully");

    initialized_ = true;
    return true;
}

void WebUIManager::update() {
    if (!initialized_) {
        return;
    }
    
    // Clean up disconnected WebSocket clients
    webSocket_.cleanupClients();
}

void WebUIManager::notifyClients() {
    if (!initialized_) {
        return;
    }
    
    String stateResponse = generateStateResponse();
    webSocket_.textAll(stateResponse);
}

bool WebUIManager::initializeLittleFS() {
    Logger.info("=== Initializing LittleFS ===");

    // Try to mount without auto-format first to see actual errors
    if (!LittleFS.begin(false)) {
        Logger.warning("LittleFS Mount Failed - uploaded image cannot be mounted!");
        Logger.warning("This means the uploaded filesystem has wrong block/page parameters.");
        Logger.info("Trying to mount with format-on-fail as fallback...");

        // As fallback, allow formatting
        if (!LittleFS.begin(true)) {
            Logger.error("LittleFS Mount Failed even with format!");
            return false;
        }
        Logger.warning("WARNING: LittleFS was formatted blank. Files were NOT loaded from uploaded image.");
    } else {
        Logger.info("LittleFS mounted successfully from uploaded image!");
    }

    // List files to verify filesystem contents
    File root = LittleFS.open("/");
    if (!root) {
        Logger.error("ERROR: Failed to open root directory");
        return false;
    }

    // Check if files exist, write from PROGMEM if missing
    // NOTE: This now just logs that files are loaded from LittleFS image
    writeEmbeddedFilesToFS();

    Logger.debug("Files in LittleFS:");
    File file = root.openNextFile();
    int fileCount = 0;
    while (file) {
        Logger.debug("  - %s (%d bytes)", file.name(), file.size());
        file = root.openNextFile();
        fileCount++;
    }
    Logger.debug("Total files found: %d", fileCount);
    Logger.info("=== LittleFS initialization complete ===");

    return true;
}

void WebUIManager::writeEmbeddedFilesToFS() {
    // Web files are uploaded directly to LittleFS via PlatformIO's build filesystem feature
    Logger.debug("writeEmbeddedFilesToFS: Skipped (files loaded from LittleFS image)");
}

void WebUIManager::initializeWebSocket() {
    webSocket_.onEvent(staticWebSocketEventHandler);
    server_->addHandler(&webSocket_);
}

void WebUIManager::setupRoutes() {
    Logger.debug("=== WebUIManager: Setting up routes ===");

    // Root (/) is handled by the static handler in setupStaticFiles() via
    // setDefaultFile("index.html"). AsyncFileResponse auto-detects .gz siblings
    // and sets Content-Encoding: gzip transparently.

    server_->on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Server is working! IP: " + WiFi.localIP().toString());
    });

    Logger.debug("=== WebUIManager: Routes setup complete ===");
    // NOTE: WiFi setup routes (/setup, /factory-reset) are handled by
    // WiFiSetupManager library (serves embedded HTML with neutral theme)
}

void WebUIManager::setupAPIEndpoints() {
    // NOTE: WiFi setup endpoints (/get-networks, /save-wifi, /factory-reset POST)
    // are handled by WiFiSetupManager library

    // LED Configuration page — served by the Svelte SPA. We send the gzipped
    // index.html and let the Svelte router show the LedConfig view based on
    // window.location.pathname. AsyncFileResponse auto-detects the .gz sibling.
    server_->on("/led-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/web/index.html", "text/html");
    });

    server_->on("/get-led-config", HTTP_GET, [](AsyncWebServerRequest* request) {
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

    server_->on("/save-led-config", HTTP_POST, [](AsyncWebServerRequest* request) {
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
            extern bool g_restartRequested;
            g_restartRequested = true;
        } else {
            request->send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid LED configuration\"}");
        }
    });

    // Settings: idle screensaver (enable + idle timeout). Applied live, no restart.
    server_->on("/get-settings", HTTP_GET, [](AsyncWebServerRequest* request) {
        bool enabled = g_uiManager ? g_uiManager->isScreensaverEnabled() : true;
        uint32_t idleSec = g_uiManager ? g_uiManager->getScreensaverIdleMs() / 1000 : 30;

        String json = "{";
        json += "\"screensaverEnabled\":" + String(enabled ? "true" : "false") + ",";
        json += "\"screensaverTimeoutSec\":" + String(idleSec);
        json += "}";

        request->send(200, "application/json", json);
    });

    server_->on("/save-settings", HTTP_POST, [](AsyncWebServerRequest* request) {
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
    server_->on("/clear-led-state", HTTP_POST, [](AsyncWebServerRequest* request) {
        extern LEDManager* g_ledManager;
        if (g_ledManager) {
            g_ledManager->clearSavedState();
        }

        Logger.info("LED state preferences cleared via web UI");
        request->send(200, "application/json", "{\"ok\":true}");
    });

    // Legacy get-message endpoint (mostly unused)
    server_->on("/get-message", HTTP_GET, [](AsyncWebServerRequest* request) {
        String response = "";
        request->send(200, "application/json", response);
    });
}

void WebUIManager::setupStaticFiles() {
    // Svelte-built UI lives under /web/ and is gzipped. AsyncStaticWebHandler
    // auto-detects .gz siblings, so a request for /app.js transparently serves
    // /web/app.js.gz with Content-Encoding: gzip.
    server_->serveStatic("/", LittleFS, "/web/").setDefaultFile("index.html");

    // Cache the bundle aggressively — vite emits hash-free names but we control
    // the rebuild via the firmware reflash, so a long max-age is safe.
    // (Skipped: AsyncStaticWebHandler::setCacheControl would force a single value
    //  for everything under /, including the standalone led-config pages.)
}

String WebUIManager::generateAnimationsResponse() {
    JsonDocument doc;
    doc["message"] = "animations";
    JsonArray animations = doc["animations"].to<JsonArray>();

    for (int i = LEDManager::RAINBOW; i <= LEDManager::CONFETTI; i++) {
        LEDManager::AnimationType current = static_cast<LEDManager::AnimationType>(i);
        JsonObject anim = animations.add<JsonObject>();
        anim["name"] = LEDManager::getAnimationDescription(current);
        anim["value"] = i;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

String WebUIManager::generateStateResponse() {
    JsonDocument doc;
    doc["message"] = "states";
    JsonArray controls = doc["controls"].to<JsonArray>();
    
    JsonObject vu_ctrl = controls.add<JsonObject>();
    vu_ctrl["name"] = "vu";
    vu_ctrl["state"] = g_ledManager ? g_ledManager->isVuModeEnabled() : false;

    JsonObject white_ctrl = controls.add<JsonObject>();
    white_ctrl["name"] = "white";
    white_ctrl["state"] = g_ledManager ? g_ledManager->isWhiteModeEnabled() : false;

    JsonObject anim_ctrl = controls.add<JsonObject>();
    anim_ctrl["name"] = "animation";
    anim_ctrl["state"] = g_ledManager ? g_ledManager->isAnimationEnabled() : false;
    anim_ctrl["animation"] = g_ledManager ? static_cast<int>(g_ledManager->getCurrentAnimation()) : 0;

    JsonObject color_ctrl = controls.add<JsonObject>();
    color_ctrl["name"] = "colour";
    color_ctrl["state"] = g_colourWheel ? g_colourWheel->getColorHex() : "#000000";

    JsonObject brightness_ctrl = controls.add<JsonObject>();
    brightness_ctrl["name"] = "brightness";
    brightness_ctrl["state"] = g_ledManager ? g_ledManager->getBrightness() : 128;

    String output;
    serializeJson(doc, output);
    return output;
}

void WebUIManager::handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String message = (char*)data;
        JsonDocument request;
        deserializeJson(request, message);

        String messageType = (const char*)request["message"];

        if (messageType == "connect") {
            handleConnectMessage();
        } else if (messageType == "vu") {
            handleVuMessage(request);
        } else if (messageType == "white") {
            handleWhiteMessage(request);
        } else if (messageType == "brightness") {
            handleBrightnessMessage(request);
        } else if (messageType == "animation") {
            handleAnimationMessage(request);
        } else if (messageType == "colour") {
            handleColorMessage(request);
        }
    }
}

void WebUIManager::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                   AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Logger.debug("WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Logger.debug("WebSocket client #%u disconnected", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void WebUIManager::handleConnectMessage() {
    webSocket_.textAll(generateAnimationsResponse());
    webSocket_.textAll(generateStateResponse());
}

// These handlers run on the AsyncTCP task. They must NOT touch LVGL directly —
// instead they post a POD command to the UI owner, which applies it on the task
// that owns lv_* (see UIManager::applyUiCommand). notifyClients() is issued
// there too, after the state has actually changed.
void WebUIManager::handleVuMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetVu;
        cmd.boolValue = (bool)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleWhiteMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetWhite;
        cmd.boolValue = (bool)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleBrightnessMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetBrightness;
        cmd.intValue = (int)request["value"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleAnimationMessage(const JsonDocument& request) {
    if (g_uiManager) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetAnimation;
        cmd.boolValue = (bool)request["value"];
        cmd.intValue = (int)request["animation"];
        g_uiManager->postUiCommand(cmd);
    }
}

void WebUIManager::handleColorMessage(const JsonDocument& request) {
    if (g_uiManager) {
        String hexValue = (const char*)request["value"];
        Logger.debug("Hex value: %s", hexValue.c_str());
        UiCommand cmd;
        cmd.type = UiCommandType::SetColour;
        snprintf(cmd.colour, sizeof(cmd.colour), "%s", hexValue.c_str());
        g_uiManager->postUiCommand(cmd);
    }
}

// Static WebSocket event handler (needed for C-style callback)
void WebUIManager::staticWebSocketEventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (g_webUIManager) {
        g_webUIManager->onWebSocketEvent(server, client, type, arg, data, len);
    }
}

// Web UI notification helper
void updateWebUi() {
    if (g_webUIManager) {
        g_webUIManager->notifyClients();
    }
}