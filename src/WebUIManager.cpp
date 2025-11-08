#include "WebUIManager.h"
#include "UIManager.h"
#include "WiFiManager.h"
#include "LEDManager.h"
#include "ColourWheel.h"

// Legacy global variables for backward compatibility
extern uint8_t brightness;
extern bool showAnimation;
extern bool vu;
extern bool white;
extern LEDManager::AnimationType currentAnimation;

// Global manager instances
extern UIManager* g_uiManager;
extern WiFiManager* g_wifiManager;
extern LEDManager* g_ledManager;
extern BrightnessSlider* g_brightnessSlider;
extern ColourWheel* g_colourWheel;

// Global WebUI manager instance
WebUIManager* g_webUIManager = nullptr;

WebUIManager::WebUIManager()
    : initialized_(false)
    , server_(80)
    , webSocket_("/ws")
{
}

WebUIManager::~WebUIManager() {
    // AsyncWebServer and AsyncWebSocket cleanup is handled automatically
}

bool WebUIManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    if (!initializeSPIFFS()) {
        Serial.println("Failed to initialize SPIFFS");
        return false;
    }
    
    initializeWebSocket();
    setupRoutes();
    setupAPIEndpoints();
    setupStaticFiles();
    
    // Initialize ElegantOTA
    ElegantOTA.begin(&server_);
    
    // Start the server
    server_.begin();
    Serial.println("Web UI Manager initialized successfully");
    
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

bool WebUIManager::initializeSPIFFS() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An error has occurred while mounting SPIFFS");
        return false;
    }
    Serial.println("SPIFFS mounted successfully");
    return true;
}

void WebUIManager::initializeWebSocket() {
    webSocket_.onEvent(staticWebSocketEventHandler);
    server_.addHandler(&webSocket_);
}

void WebUIManager::setupRoutes() {
    // Route for root / web page
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/index.html", "text/html", false);
    });
    
    // Debug route to test connectivity
    server_.on("/test", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "Server is working! IP: " + WiFi.softAPIP().toString());
    });
    
    // Captive portal detection routes
    server_.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    server_.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    server_.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    server_.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    server_.on("/redirect", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("/setup");
    });
    
    // WiFi Setup route - serve the HTML file
    server_.on("/setup", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/setup.html", "text/html");
    });
    
    // Factory reset GET endpoint to serve the HTML page
    server_.on("/factory-reset", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(SPIFFS, "/factory-reset.html", "text/html");
    });
}

void WebUIManager::setupAPIEndpoints() {
    // Get networks endpoint for the HTML page
    server_.on("/get-networks", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetNetworks(request);
    });
    
    // Scan networks endpoint - just return existing scan from startup
    server_.on("/scan-networks", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleScanNetworks(request);
    });
    
    // Handle WiFi configuration save
    server_.on("/save-wifi", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleWiFiSave(request);
    });
    
    // Get current WiFi settings endpoint
    server_.on("/get-current-settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleGetCurrentSettings(request);
    });
    
    // Factory reset POST endpoint to perform the reset
    server_.on("/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleFactoryReset(request);
    });
    
    // Legacy get-message endpoint (mostly unused)
    server_.on("/get-message", HTTP_GET, [](AsyncWebServerRequest* request) {
        String response = "";
        request->send(200, "application/json", response);
    });
}

void WebUIManager::setupStaticFiles() {
    // Serve static files from SPIFFS
    server_.serveStatic("/", SPIFFS, "/");
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
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
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
        Serial.println(hexValue);
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
        Serial.printf("Updated WiFi settings: Host=%s, SSID=%s\n", networkName.c_str(), wifiNetwork.c_str());
    } else {
        Serial.println("WiFi settings unchanged - skipping WiFi update");
    }
    
    // Save LED configuration preferences
    if (numStrips > 0 && ledsPerStrip > 0) {
        Preferences ledPrefs;
        ledPrefs.begin("led-config", false);
        ledPrefs.putInt("num_strips", numStrips);
        ledPrefs.putInt("leds_per_strip", ledsPerStrip);
        ledPrefs.end();
        Serial.printf("Saved LED config: %d strips, %d LEDs per strip\n", numStrips, ledsPerStrip);
    }
    
    // Read the HTML template and replace placeholders
    String html = "";
    try {
        if (SPIFFS.exists("/wifi-saved.html")) {
            File file = SPIFFS.open("/wifi-saved.html", "r");
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
    if (SPIFFS.exists("/factory-reset.html")) {
        File file = SPIFFS.open("/factory-reset.html", "r");
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
    if (!g_webUIManager) {
        g_webUIManager = new WebUIManager();
    }
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