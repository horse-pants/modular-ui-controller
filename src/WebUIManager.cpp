#include "WebUIManager.h"
#include "UIManager.h"
// OLD: #include "WiFiManager.h"
// NEW: Using library version
#include <WiFiSetupManager.h>
#include <OTAManager.h>
#include <Logger.h>
#include "LEDManager.h"
#include "ColourWheel.h"
#include <FastLED.h>  // For CRGB color constants

// Legacy global variables for backward compatibility
extern uint8_t brightness;
extern bool showAnimation;
extern bool vu;
extern bool white;
extern LEDManager::AnimationType currentAnimation;

// Global manager instances
extern UIManager* g_uiManager;
// OLD: extern WiFiManager* g_wifiManager;
// NEW: Using library version WiFiSetupManager
extern WiFiSetupManager* g_wifiManager;
extern LEDManager* g_ledManager;
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;

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
            // Turn off animations during OTA
            if (g_ledManager) {
                g_ledManager->setAnimationEnabled(false);
            }
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
                // Flash red on failure
                if (g_ledManager) {
                    for (int i = 0; i < 3; i++) {
                        g_ledManager->fillColor(CRGB::Red);
                        delay(200);
                        g_ledManager->fillColor(CRGB::Black);
                        delay(200);
                    }
                }
            }
        });

        // Initialize OTA with the shared server
        g_otaManager->begin(server_);
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
    #include "WebFilesData.h"

    struct WebFile {
        const char* filename;
        const char* content;
    };

    WebFile files[] = {
        {"/index.html", INDEX_HTML},
        {"/led-config.html", LED_CONFIG_HTML},
        {"/style.css", STYLE_CSS},
        {"/script.js", SCRIPT_JS},
        {"/theme.js", THEME_JS}
    };

    Logger.debug("Checking web files...");
    int written = 0;
    int updated = 0;

    for (const auto& webFile : files) {
        bool needsWrite = false;

        if (!LittleFS.exists(webFile.filename)) {
            // File doesn't exist, write it
            needsWrite = true;
        } else {
            // File exists, check if content matches
            File existingFile = LittleFS.open(webFile.filename, "r");
            if (existingFile) {
                String existingContent = existingFile.readString();
                existingFile.close();

                String newContent = FPSTR(webFile.content);

                if (existingContent != newContent) {
                    Logger.debug("  %s has changed, updating...", webFile.filename);
                    needsWrite = true;
                    updated++;
                }
            } else {
                needsWrite = true;
            }
        }

        if (needsWrite) {
            File file = LittleFS.open(webFile.filename, "w");
            if (file) {
                file.print(FPSTR(webFile.content));
                file.close();
                Logger.debug("  Writing %s from PROGMEM... OK", webFile.filename);
                written++;
            } else {
                Logger.error("  Writing %s from PROGMEM... FAILED!", webFile.filename);
            }
        }
    }

    if (written > 0) {
        Logger.info("Wrote %d files from PROGMEM (%d updated, %d new)", written, updated, written - updated);
    } else {
        Logger.debug("All web files are up to date");
    }
}

void WebUIManager::initializeWebSocket() {
    webSocket_.onEvent(staticWebSocketEventHandler);
    server_->addHandler(&webSocket_);
}

void WebUIManager::setupRoutes() {
    Logger.debug("=== WebUIManager: Setting up routes ===");

    // Route for root / web page
    server_->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        Logger.debug("Route called: GET /");
        if (LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            Logger.debug("  /index.html exists, size: %d bytes", file.size());
            file.close();
        } else {
            Logger.error("  /index.html NOT FOUND!");
        }
        request->send(LittleFS, "/index.html", "text/html", false);
    });

    // Debug route to test connectivity
    server_->on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        Logger.debug("Route called: GET /test");
        request->send(200, "text/plain", "Server is working! IP: " + WiFi.localIP().toString());
    });

    Logger.debug("=== WebUIManager: Routes setup complete ===");
    // NOTE: WiFi setup routes (/setup, /factory-reset) are handled by
    // WiFiSetupManager library (serves embedded HTML with neutral theme)
}

void WebUIManager::setupAPIEndpoints() {
    // NOTE: WiFi setup endpoints (/get-networks, /save-wifi, /factory-reset POST)
    // are handled by WiFiSetupManager library

    // LED Configuration endpoints
    server_->on("/led-config", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/led-config.html", "text/html");
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

            String html = "<h1>LED Configuration Saved!</h1>";
            html += "<p>Strips: " + String(numStrips) + "</p>";
            html += "<p>LEDs per strip: " + String(ledsPerStrip) + "</p>";
            html += "<p>Total LEDs: " + String(numStrips * ledsPerStrip) + "</p>";
            html += "<p>Restarting...</p>";
            html += "<script>setTimeout(function(){ window.location.href='/'; }, 3000);</script>";

            request->send(200, "text/html", html);

            // Schedule restart
            extern bool g_restartRequested;
            g_restartRequested = true;
        } else {
            request->send(400, "text/plain", "Invalid LED configuration");
        }
    });

    // Legacy get-message endpoint (mostly unused)
    server_->on("/get-message", HTTP_GET, [](AsyncWebServerRequest* request) {
        String response = "";
        request->send(200, "application/json", response);
    });
}

void WebUIManager::setupStaticFiles() {
    // Serve static files from LittleFS
    server_->serveStatic("/", LittleFS, "/");
}

String WebUIManager::generateAnimationsResponse() {
    JsonDocument doc;
    doc["message"] = "animations";
    JsonArray animations = doc["animations"].to<JsonArray>();

    for (int i = LEDManager::RAINBOW; i <= LEDManager::SOUNDRIPPLE; i++) {
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
    vu_ctrl["state"] = vu;
    
    JsonObject white_ctrl = controls.add<JsonObject>();
    white_ctrl["name"] = "white";
    white_ctrl["state"] = white;
    
    JsonObject anim_ctrl = controls.add<JsonObject>();
    anim_ctrl["name"] = "animation";
    anim_ctrl["state"] = showAnimation;
    anim_ctrl["animation"] = currentAnimation;
    
    JsonObject color_ctrl = controls.add<JsonObject>();
    color_ctrl["name"] = "colour";
    color_ctrl["state"] = g_colourWheel ? g_colourWheel->getColorHex() : "#000000";
    
    JsonObject brightness_ctrl = controls.add<JsonObject>();
    brightness_ctrl["name"] = "brightness";
    brightness_ctrl["state"] = brightness;

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

void WebUIManager::handleVuMessage(const JsonDocument& request) {
    if (g_uiManager) {
        g_uiManager->setVuState((bool)request["value"]);
    }
    notifyClients();
}

void WebUIManager::handleWhiteMessage(const JsonDocument& request) {
    if (g_uiManager) {
        g_uiManager->setWhiteState((bool)request["value"]);
    }
    notifyClients();
}

void WebUIManager::handleBrightnessMessage(const JsonDocument& request) {
    int newBrightness = (int)request["value"];
    if (g_brightnessSlider) {
        // Trigger callback to update global state and notify other clients
        g_brightnessSlider->setBrightness(newBrightness, true, true);
    } else {
        brightness = newBrightness;
        if (g_ledManager) {
            g_ledManager->setBrightness(newBrightness);
        }
        notifyClients();
    }
}

void WebUIManager::handleAnimationMessage(const JsonDocument& request) {
    bool runAnimation = (bool)request["value"];
    if (g_uiManager) {
        if (runAnimation) {
            g_uiManager->setAnimation((int)request["animation"]);
        }
        g_uiManager->setAnimationState(runAnimation);
    }
    notifyClients();
}

void WebUIManager::handleColorMessage(const JsonDocument& request) {
    if (g_colourWheel) {
        String hexValue = (const char*)request["value"];
        Logger.debug("Hex value: %s", hexValue.c_str());
        g_colourWheel->setColor(hexValue);
    }
}

void WebUIManager::handleGetNetworks(AsyncWebServerRequest* request) {
    String networks = "";
    if (g_wifiManager) {
        networks = g_wifiManager->getScannedNetworks();
    }
    request->send(200, "text/plain", networks);
}

void WebUIManager::handleScanNetworks(AsyncWebServerRequest* request) {
    String networks = "";
    if (g_wifiManager) {
        networks = g_wifiManager->getScannedNetworks();
    }
    request->send(200, "text/plain", networks);
}

void WebUIManager::handleWiFiSave(AsyncWebServerRequest* request) {
    String networkName = "";
    String wifiNetwork = "";
    String wifiPassword = "";
    int numStrips = 0;
    int ledsPerStrip = 0;
    
    if (request->hasParam("network_name", true)) {
        networkName = request->getParam("network_name", true)->value();
    }
    if (request->hasParam("wifi_network", true)) {
        wifiNetwork = request->getParam("wifi_network", true)->value();
    }
    if (request->hasParam("wifi_password", true)) {
        wifiPassword = request->getParam("wifi_password", true)->value();
    }
    if (request->hasParam("num_strips", true)) {
        numStrips = request->getParam("num_strips", true)->value().toInt();
    }
    if (request->hasParam("leds_per_strip", true)) {
        ledsPerStrip = request->getParam("leds_per_strip", true)->value().toInt();
    }
    
    // Load current WiFi settings to check for changes
    Preferences prefs;
    prefs.begin("wifi", true); // Read-only first
    String currentNetworkName = prefs.getString("host_name", "");
    String currentWifiSSID = prefs.getString("ssid", "");
    prefs.end();
    
    // Check if WiFi settings have changed
    bool wifiChanged = (networkName != currentNetworkName) || 
                       (wifiNetwork != currentWifiSSID) || 
                       (wifiPassword.length() > 0); // Password provided means user wants to update it
    
    // Only update WiFi preferences if something changed
    if (wifiChanged) {
        prefs.begin("wifi", false); // Write mode
        prefs.putString("host_name", networkName);
        prefs.putString("ssid", wifiNetwork);
        
        // Only update password if one was provided
        if (wifiPassword.length() > 0) {
            prefs.putString("password", wifiPassword);
        }

        prefs.end();
        Logger.info("Updated WiFi settings: Host=%s, SSID=%s", networkName.c_str(), wifiNetwork.c_str());
    } else {
        Logger.debug("WiFi settings unchanged - skipping WiFi update");
    }

    // Save LED configuration preferences
    if (numStrips > 0 && ledsPerStrip > 0) {
        Preferences ledPrefs;
        ledPrefs.begin("led-config", false);
        ledPrefs.putInt("num_strips", numStrips);
        ledPrefs.putInt("leds_per_strip", ledsPerStrip);
        ledPrefs.end();
        Logger.info("Saved LED config: %d strips, %d LEDs per strip", numStrips, ledsPerStrip);
    }
    
    // Read the HTML template and replace placeholders
    String html = "";
    try {
        if (LittleFS.exists("/wifi-saved.html")) {
            File file = LittleFS.open("/wifi-saved.html", "r");
            if (file) {
                html = file.readString();
                file.close();
                // Replace placeholders
                html.replace("%NETWORK_NAME%", networkName);
                html.replace("%WIFI_NETWORK%", wifiNetwork);
            }
        }
    } catch (...) {
        // Handle any exceptions during file reading
        html = "";
    }
    
    if (html.length() == 0) {
        // Fallback if file doesn't exist - show what was actually changed
        html = "<h1>Configuration Saved!</h1>";
        if (wifiChanged) {
            html += "<p>WiFi Updated - Host: " + networkName + " | SSID: " + wifiNetwork + "</p>";
        } else {
            html += "<p>WiFi settings unchanged</p>";
        }
        if (numStrips > 0 && ledsPerStrip > 0) {
            html += "<p>LED Config: " + String(numStrips) + " strips, " + String(ledsPerStrip) + " LEDs per strip</p>";
        }
        html += "<p>Restarting...</p>";
    }
    
    request->send(200, "text/html", html);
    
    // Schedule restart - don't block the async web server thread
    extern bool g_restartRequested;
    g_restartRequested = true;
}

void WebUIManager::handleGetCurrentSettings(AsyncWebServerRequest* request) {
    Preferences prefs;
    prefs.begin("wifi", true); // Read-only mode
    
    bool hasSettings = prefs.isKey("host_name");
    String networkName = prefs.getString("host_name", "");
    String wifiSSID = prefs.getString("ssid", "");
    
    prefs.end();
    
    // Get LED configuration
    Preferences ledPrefs;
    ledPrefs.begin("led-config", true); // Read-only mode
    
    bool hasLedSettings = ledPrefs.isKey("num_strips");
    int numStrips = ledPrefs.getInt("num_strips", 0);
    int ledsPerStrip = ledPrefs.getInt("leds_per_strip", 0);
    int totalLeds = numStrips * ledsPerStrip;
    
    ledPrefs.end();
    
    String json = "{";
    json += "\"hasSettings\":" + String(hasSettings ? "true" : "false") + ",";
    json += "\"networkName\":\"" + networkName + "\",";
    json += "\"wifiSSID\":\"" + wifiSSID + "\",";
    json += "\"hasLedSettings\":" + String(hasLedSettings ? "true" : "false") + ",";
    json += "\"numStrips\":" + String(numStrips) + ",";
    json += "\"ledsPerStrip\":" + String(ledsPerStrip) + ",";
    json += "\"totalLeds\":" + String(totalLeds);
    json += "}";
    
    request->send(200, "application/json", json);
}

void WebUIManager::handleFactoryReset(AsyncWebServerRequest* request) {
    // Clear all preferences
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.clear(); // Clear all preferences in the "wifi" namespace
    prefs.end();
    
    // Also clear LED config preferences
    prefs.begin("led-config", false);
    prefs.clear();
    prefs.end();
    
    // Read the HTML template
    String html = "";
    if (LittleFS.exists("/factory-reset.html")) {
        File file = LittleFS.open("/factory-reset.html", "r");
        if (file) {
            html = file.readString();
            file.close();
        }
    }
    
    if (html.length() == 0) {
        // Fallback if file doesn't exist
        html = "<h1>Factory Reset Complete!</h1><p>All WiFi settings cleared.</p><p>Restarting...</p>";
    }
    
    request->send(200, "text/html", html);
    
    // Schedule restart - don't block the async web server thread
    extern bool g_restartRequested;
    g_restartRequested = true;
}

// Static WebSocket event handler (needed for C-style callback)
void WebUIManager::staticWebSocketEventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                              AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (g_webUIManager) {
        g_webUIManager->onWebSocketEvent(server, client, type, arg, data, len);
    }
}

// Legacy function compatibility
void setupWebUi() {
    // NOTE: This legacy function is deprecated
    // WebUIManager should now be constructed with WiFiSetupManager's web server
    // See main.cpp for proper initialization
    if (g_webUIManager) {
        g_webUIManager->initialize();
    }
}

void webUiLoop() {
    if (g_webUIManager) {
        g_webUIManager->update();
    }
}

void updateWebUi() {
    if (g_webUIManager) {
        g_webUIManager->notifyClients();
    }
}