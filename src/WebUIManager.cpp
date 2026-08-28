#include "WebUIManager.h"

#include "ui/EffectGrid.h"
#include "WebApi.h"
#include "ui/UIManager.h"
#include "ui/ui.h"
#include <WiFiSetupManager.h>
#include <OTAManager.h>
#include <Logger.h>
#include "led/LEDManager.h"   // g_ledManager
#include "ui/ColourWheel.h"
#include "audio/AudioTask.h"  // pause/resume audio task around OTA

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

    initializeWebSocket();
    WebApi::registerRoutes(server_);

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
                // OTA failed (no reboot). Release the strips back to normal and
                // ask for a non-blocking red flash — do NOT block here (this runs
                // on the AsyncTCP task; the old delay()-based blink stalled it ~1.2s
                // and never even rendered, since update() is suspended in OTA mode).
                if (g_ledManager) {
                    g_ledManager->setOTAMode(false);
                    g_ledManager->flashError();
                }
                // Bring the audio task back.
                resumeAudioTask();
            }
            // On success the device restarts, so the flag resets implicitly.
        });

        // Initialize OTA with the shared server
        // Blank partition label = no filesystem OTA. The web UI is linked into
        // the firmware now (WebAssets.h), so there is no filesystem to update and
        // the option would only be a way to wipe a partition by accident.
        g_otaManager->begin(server_, "", "", "");
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

void WebUIManager::initializeWebSocket() {
    webSocket_.onEvent(staticWebSocketEventHandler);
    server_->addHandler(&webSocket_);
}

String WebUIManager::generateAnimationsResponse() {
    JsonDocument doc;
    doc["message"] = "animations";
    JsonArray animations = doc["animations"].to<JsonArray>();

    for (int i = 0; i < ANIMATION_COUNT; i++) {
        AnimationType current = static_cast<AnimationType>(i);
        JsonObject anim = animations.add<JsonObject>();
        anim["name"] = animationDescription(current);
        anim["value"] = i;
        // The colour the device paints this effect's tile with. Sent rather than
        // duplicated in the Svelte app, so the two pickers can't drift apart.
        anim["colour"] = EffectGrid::signatureColor(i);
        anim["colour2"] = EffectGrid::signatureColorEnd(i);
        anim["audio"] = (i >= ICEWAVES);
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
        case WS_EVT_PING:
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
        // Deliberately not logged: the web wheel sends one of these per pointer
        // move, and a line each buried the whole log buffer in a single drag —
        // and pushed every one out to all WebSocket clients.
        UiCommand cmd;
        cmd.type = UiCommandType::SetColour;
        // cmd.colour is sized exactly for "#RRGGBB" + null. strlcpy is bounded and
        // always null-terminates (and, unlike snprintf("%s"), doesn't trip
        // -Wformat-truncation on the unbounded source string).
        strlcpy(cmd.colour, hexValue.c_str(), sizeof(cmd.colour));
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