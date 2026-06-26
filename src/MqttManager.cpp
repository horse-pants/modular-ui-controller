#include "MqttManager.h"

#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Logger.h>

#include "UiCommand.h"
#include "ui/UIManager.h"        // UIManager::postUiCommand
#include "ui/ui.h"               // g_uiManager
#include "led/LEDManager.h"
#include "led/Animations.h"

// Global instance.
MqttManager* g_mqttManager = nullptr;

// NVS namespace for broker config. Single-user device — no migration/back-compat.
static const char* NVS_NS = "mqtt";

namespace {
// Effects that aren't animations. Kept first in the effect list so the indices
// after them line up with AnimationType when we need them.
constexpr const char* EFFECT_SOLID = "Solid";
constexpr const char* EFFECT_WHITE = "White";

// 24-bit value → zero-padded 6-char lowercase hex. Mirrors ColourWheel's
// ltoa-based approach (avoids snprintf("%02X"), which trips -Wformat-truncation
// because the compiler assumes the int could need up to 8 hex digits).
String toHex6(uint32_t value) {
    char buf[10] = "";
    ltoa(static_cast<long>(value & 0xFFFFFFu), buf, 16);
    String s = buf;
    while (s.length() < 6) s = "0" + s;
    return s;
}
}  // namespace

MqttManager::MqttManager(LEDManager* led) : led_(led) {}

MqttManager::~MqttManager() {
    stopClient();
    if (inboundQueue_) {
        vQueueDelete(inboundQueue_);
        inboundQueue_ = nullptr;
    }
}

void MqttManager::begin() {
    inboundQueue_ = xQueueCreate(8, sizeof(InboundMsg));

    // Stable id from the MAC (last 3 bytes) — survives reflashes, unique per device.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    shortId_ = toHex6((static_cast<uint32_t>(mac[3]) << 16) |
                      (static_cast<uint32_t>(mac[4]) << 8) | mac[5]);
    objectId_ = String("modularui_") + shortId_;

    if (led_) {
        uint8_t b = led_->getBrightness();
        if (b > 0) lastOnBrightness_ = b;
    }

    loadConfig();
    if (cfg_.enabled && cfg_.host.length() > 0) {
        startClient();
    }
}

void MqttManager::loadConfig() {
    Preferences prefs;
    prefs.begin(NVS_NS, true);  // read-only
    cfg_.enabled = prefs.getBool("enabled", false);
    cfg_.host = prefs.getString("host", "homeassistant.local");
    cfg_.port = static_cast<uint16_t>(prefs.getUInt("port", 1883));
    cfg_.user = prefs.getString("user", "");
    cfg_.pass = prefs.getString("pass", "");
    cfg_.prefix = prefs.getString("prefix", "modular-ui");
    prefs.end();
}

void MqttManager::buildTopics() {
    baseTopic_ = cfg_.prefix + "/" + shortId_;
    topicLightSet_ = baseTopic_ + "/light/set";
    topicLightState_ = baseTopic_ + "/light/state";
    topicVuSet_ = baseTopic_ + "/vu/set";
    topicVuState_ = baseTopic_ + "/vu/state";
    topicStatus_ = baseTopic_ + "/status";
}

void MqttManager::startClient() {
    buildTopics();

    esp_mqtt_client_config_t mcfg = {};
    mcfg.broker.address.hostname = cfg_.host.c_str();
    mcfg.broker.address.port = cfg_.port;
    mcfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mcfg.credentials.client_id = objectId_.c_str();
    if (cfg_.user.length() > 0) {
        mcfg.credentials.username = cfg_.user.c_str();
        mcfg.credentials.authentication.password = cfg_.pass.c_str();
    }
    // Last will → device goes "offline" if it drops without a clean disconnect.
    mcfg.session.last_will.topic = topicStatus_.c_str();
    mcfg.session.last_will.msg = "offline";
    mcfg.session.last_will.msg_len = 7;
    mcfg.session.last_will.qos = 1;
    mcfg.session.last_will.retain = true;

    client_ = esp_mqtt_client_init(&mcfg);
    if (!client_) {
        Logger.error("MQTT: client init failed");
        return;
    }
    esp_mqtt_client_register_event(
        client_, static_cast<esp_mqtt_event_id_t>(MQTT_EVENT_ANY), eventHandler, this);
    esp_mqtt_client_start(client_);
    Logger.info("MQTT: connecting to %s:%u as %s", cfg_.host.c_str(),
                static_cast<unsigned>(cfg_.port), objectId_.c_str());
}

void MqttManager::stopClient() {
    connected_ = false;
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
}

void MqttManager::applyConfig(bool enabled, const String& host, uint16_t port,
                              const String& user, const String& pass, const String& prefix) {
    Preferences prefs;
    prefs.begin(NVS_NS, false);  // read-write
    prefs.putBool("enabled", enabled);
    prefs.putString("host", host);
    prefs.putUInt("port", port);
    prefs.putString("user", user);
    prefs.putString("pass", pass);
    prefs.putString("prefix", prefix.length() ? prefix : String("modular-ui"));
    prefs.end();

    // Defer the actual client restart to update() on the main loop — esp_mqtt
    // stop/destroy can block and this runs on the AsyncTCP task.
    reconfigurePending_ = true;
    Logger.info("MQTT: config saved (enabled=%d host=%s)", enabled ? 1 : 0, host.c_str());
}

void MqttManager::update() {
    if (reconfigurePending_) {
        reconfigurePending_ = false;
        stopClient();
        loadConfig();
        if (cfg_.enabled && cfg_.host.length() > 0) {
            startClient();
        }
    }

    // Drain inbound commands (parsed + applied on the main loop, not the MQTT task).
    if (inboundQueue_) {
        InboundMsg msg;
        while (xQueueReceive(inboundQueue_, &msg, 0) == pdTRUE) {
            handleInbound(msg);
        }
    }

    if (connected_) {
        publishStateIfChanged(forcePublish_);
        forcePublish_ = false;
    }
}

// ── MQTT event flow ────────────────────────────────────────────────────────

void MqttManager::eventHandler(void* args, esp_event_base_t /*base*/, int32_t id, void* data) {
    auto* self = static_cast<MqttManager*>(args);
    auto* ev = static_cast<esp_mqtt_event_handle_t>(data);
    switch (static_cast<esp_mqtt_event_id_t>(id)) {
        case MQTT_EVENT_CONNECTED:
            self->connected_ = true;
            self->onConnected();
            break;
        case MQTT_EVENT_DISCONNECTED:
            self->connected_ = false;
            break;
        case MQTT_EVENT_DATA:
            // Small single-chunk payloads only; ignore fragmented continuations.
            if (ev->topic_len > 0 && ev->current_data_offset == 0) {
                self->enqueueInbound(ev->topic, ev->topic_len, ev->data, ev->data_len);
            }
            break;
        default:
            break;
    }
}

void MqttManager::onConnected() {
    Logger.info("MQTT: connected");
    publishDiscovery();
    publishAvailability(true);
    esp_mqtt_client_subscribe(client_, topicLightSet_.c_str(), 1);
    esp_mqtt_client_subscribe(client_, topicVuSet_.c_str(), 1);
    // Defer the initial full state publish to update() on the main loop so the
    // change-detection cache is only ever mutated from one task. (onConnected runs
    // on the MQTT event task; discovery/availability/subscribe above don't touch it.)
    forcePublish_ = true;
}

void MqttManager::enqueueInbound(const char* topic, int topicLen, const char* data, int dataLen) {
    if (!inboundQueue_) return;
    InboundMsg msg;
    int tl = topicLen < (int)sizeof(msg.topic) - 1 ? topicLen : (int)sizeof(msg.topic) - 1;
    int dl = dataLen < (int)sizeof(msg.payload) - 1 ? dataLen : (int)sizeof(msg.payload) - 1;
    memcpy(msg.topic, topic, tl);
    msg.topic[tl] = '\0';
    memcpy(msg.payload, data, dl);
    msg.payload[dl] = '\0';
    xQueueSend(inboundQueue_, &msg, 0);
}

void MqttManager::handleInbound(const InboundMsg& msg) {
    if (topicLightSet_ == msg.topic) {
        handleLightCommand(msg.payload);
    } else if (topicVuSet_ == msg.topic) {
        handleVuCommand(msg.payload);
    }
}

void MqttManager::handleLightCommand(const char* payload) {
    if (!g_uiManager) return;

    JsonDocument doc;
    if (deserializeJson(doc, payload)) {
        Logger.warning("MQTT: bad light payload");
        return;
    }

    // Colour takes the device into solid-colour mode (disables animation/white).
    if (doc["color"].is<JsonObjectConst>()) {
        JsonObjectConst c = doc["color"].as<JsonObjectConst>();
        uint8_t r = static_cast<uint8_t>(c["r"] | 0);
        uint8_t g = static_cast<uint8_t>(c["g"] | 0);
        uint8_t b = static_cast<uint8_t>(c["b"] | 0);
        UiCommand cmd;
        cmd.type = UiCommandType::SetColour;
        String hex = "#" + toHex6((static_cast<uint32_t>(r) << 16) |
                                  (static_cast<uint32_t>(g) << 8) | b);
        strlcpy(cmd.colour, hex.c_str(), sizeof(cmd.colour));
        g_uiManager->postUiCommand(cmd);
    } else if (doc["effect"].is<const char*>()) {
        const char* eff = doc["effect"];
        if (strcmp(eff, EFFECT_WHITE) == 0) {
            UiCommand cmd;
            cmd.type = UiCommandType::SetWhite;
            cmd.boolValue = true;
            g_uiManager->postUiCommand(cmd);
        } else if (strcmp(eff, EFFECT_SOLID) == 0) {
            // Re-assert the stored colour to force plain solid mode.
            UiCommand cmd;
            cmd.type = UiCommandType::SetColour;
            strlcpy(cmd.colour, currentColorHex().c_str(), sizeof(cmd.colour));
            g_uiManager->postUiCommand(cmd);
        } else {
            for (int i = 0; i < ANIMATION_COUNT; i++) {
                if (strcmp(eff, animationDescription(static_cast<AnimationType>(i))) == 0) {
                    UiCommand cmd;
                    cmd.type = UiCommandType::SetAnimation;
                    cmd.boolValue = true;
                    cmd.intValue = i;
                    g_uiManager->postUiCommand(cmd);
                    break;
                }
            }
        }
    }

    // Brightness / on-off. Explicit brightness wins; otherwise state ON restores the
    // last non-zero brightness and state OFF drops output to zero.
    bool haveBrightness = doc["brightness"].is<int>();
    int target = -1;
    if (haveBrightness) {
        target = doc["brightness"].as<int>();
    } else if (doc["state"].is<const char*>()) {
        const char* st = doc["state"];
        if (strcmp(st, "OFF") == 0) {
            target = 0;
        } else if (strcmp(st, "ON") == 0 && led_ && led_->getBrightness() == 0) {
            target = lastOnBrightness_ > 0 ? lastOnBrightness_ : 255;
        }
    }
    if (target >= 0) {
        UiCommand cmd;
        cmd.type = UiCommandType::SetBrightness;
        cmd.intValue = target < 255 ? target : 255;
        g_uiManager->postUiCommand(cmd);
    }
}

void MqttManager::handleVuCommand(const char* payload) {
    if (!g_uiManager) return;
    UiCommand cmd;
    cmd.type = UiCommandType::SetVu;
    cmd.boolValue = (strcmp(payload, "ON") == 0 || strcmp(payload, "on") == 0 ||
                     strcmp(payload, "1") == 0 || strcmp(payload, "true") == 0);
    g_uiManager->postUiCommand(cmd);
}

// ── Publishing ─────────────────────────────────────────────────────────────

void MqttManager::publish(const String& topic, const String& payload, int qos, bool retain) {
    if (!client_) return;
    esp_mqtt_client_publish(client_, topic.c_str(), payload.c_str(), payload.length(), qos, retain);
}

void MqttManager::publishDiscovery() {
    JsonDocument dev;
    dev["identifiers"][0] = shortId_;
    dev["name"] = "Modular UI Controller";
    dev["manufacturer"] = "James Cartwright";
    dev["model"] = "ESP32-S3 Modular UI";

    // Light entity (JSON schema): brightness + rgb + effect.
    {
        JsonDocument doc;
        doc["schema"] = "json";
        doc["name"] = "Modular UI LEDs";
        doc["unique_id"] = objectId_ + "_light";
        doc["command_topic"] = topicLightSet_;
        doc["state_topic"] = topicLightState_;
        doc["availability_topic"] = topicStatus_;
        doc["brightness"] = true;
        doc["supported_color_modes"][0] = "rgb";
        doc["effect"] = true;
        JsonArray effects = doc["effect_list"].to<JsonArray>();
        effects.add(EFFECT_SOLID);
        effects.add(EFFECT_WHITE);
        for (int i = 0; i < ANIMATION_COUNT; i++) {
            effects.add(animationDescription(static_cast<AnimationType>(i)));
        }
        doc["device"] = dev.as<JsonObjectConst>();

        String out;
        serializeJson(doc, out);
        publish("homeassistant/light/" + objectId_ + "/config", out, 1, true);
    }

    // VU mode switch.
    {
        JsonDocument doc;
        doc["name"] = "Modular UI VU Mode";
        doc["unique_id"] = objectId_ + "_vu";
        doc["command_topic"] = topicVuSet_;
        doc["state_topic"] = topicVuState_;
        doc["availability_topic"] = topicStatus_;
        doc["icon"] = "mdi:music-note";
        doc["device"] = dev.as<JsonObjectConst>();

        String out;
        serializeJson(doc, out);
        publish("homeassistant/switch/" + objectId_ + "_vu/config", out, 1, true);
    }

    Logger.info("MQTT: published HA discovery for %s", objectId_.c_str());
}

void MqttManager::publishAvailability(bool online) {
    publish(topicStatus_, online ? "online" : "offline", 1, true);
}

void MqttManager::publishStateIfChanged(bool force) {
    if (!led_) return;

    uint8_t bri = led_->getBrightness();
    bool anim = led_->isAnimationEnabled();
    int animIdx = static_cast<int>(led_->getCurrentAnimation());
    bool white = led_->isWhiteModeEnabled();
    bool vu = led_->isVuModeEnabled();
    CRGB col = led_->getSolidColor();
    uint32_t packed = (uint32_t(col.r) << 16) | (uint32_t(col.g) << 8) | col.b;

    bool lightChanged = force || bri != lastBrightness_ || anim != lastAnim_ ||
                        animIdx != lastAnimIdx_ || white != lastWhite_ || packed != lastColor_;
    bool vuChanged = force || vu != lastVu_;

    if (lightChanged) {
        if (bri > 0) lastOnBrightness_ = bri;

        JsonDocument doc;
        doc["state"] = bri > 0 ? "ON" : "OFF";
        doc["brightness"] = bri;
        doc["color_mode"] = "rgb";
        JsonObject c = doc["color"].to<JsonObject>();
        c["r"] = col.r;
        c["g"] = col.g;
        c["b"] = col.b;
        doc["effect"] = currentEffect();

        String out;
        serializeJson(doc, out);
        publish(topicLightState_, out, 0, true);

        lastBrightness_ = bri;
        lastAnim_ = anim;
        lastAnimIdx_ = animIdx;
        lastWhite_ = white;
        lastColor_ = packed;
    }

    if (vuChanged) {
        publish(topicVuState_, vu ? "ON" : "OFF", 0, true);
        lastVu_ = vu;
    }
}

// ── Helpers ────────────────────────────────────────────────────────────────

const char* MqttManager::currentEffect() const {
    if (!led_) return EFFECT_SOLID;
    if (led_->isAnimationEnabled()) {
        return animationDescription(led_->getCurrentAnimation());
    }
    if (led_->isWhiteModeEnabled()) {
        return EFFECT_WHITE;
    }
    return EFFECT_SOLID;
}

String MqttManager::currentColorHex() const {
    CRGB c = led_ ? led_->getSolidColor() : CRGB(255, 255, 255);
    return "#" + toHex6((static_cast<uint32_t>(c.r) << 16) |
                        (static_cast<uint32_t>(c.g) << 8) | c.b);
}
