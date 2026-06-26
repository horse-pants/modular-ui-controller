#pragma once

#include <Arduino.h>
#include "mqtt_client.h"

class LEDManager;

/**
 * @brief Bridges the LED controller to Home Assistant over MQTT.
 *
 * Uses Home Assistant's built-in MQTT Discovery — the device self-announces a
 * `light` (brightness + rgb + effect) and a `switch` (VU mode) on connect, so HA
 * auto-creates the entities with NO custom integration or add-on required.
 *
 * It is a second front-end over the same plumbing the WebSocket uses: inbound MQTT
 * commands are translated into UiCommand PODs posted to the UI owner (never touching
 * LVGL or LEDManager directly from the MQTT event task), and outbound state is
 * published from update() on the main loop whenever the LED snapshot changes.
 *
 * Built on the native IDF esp_mqtt_client (its own event task + auto-reconnect);
 * the event callback only copies messages onto a queue, honouring the project's
 * never-block-in-callbacks rule.
 */
class MqttManager {
public:
    explicit MqttManager(LEDManager* led);
    ~MqttManager();

    MqttManager(const MqttManager&) = delete;
    MqttManager& operator=(const MqttManager&) = delete;

    /// Load config from NVS and start the client if enabled + configured.
    void begin();

    /// Main-loop tick: drain inbound queue, apply pending reconfigure, publish state on diff.
    void update();

    /// Persist new broker config to NVS and schedule a client restart (called from web handler).
    void applyConfig(bool enabled, const String& host, uint16_t port,
                     const String& user, const String& pass, const String& prefix);

    // Config accessors (reflect the loaded NVS config) — used by the web settings API.
    bool isEnabled() const { return cfg_.enabled; }
    const String& host() const { return cfg_.host; }
    uint16_t port() const { return cfg_.port; }
    const String& user() const { return cfg_.user; }
    const String& pass() const { return cfg_.pass; }
    const String& prefix() const { return cfg_.prefix; }

private:
    struct Config {
        bool enabled = false;
        String host;
        uint16_t port = 1883;
        String user;
        String pass;
        String prefix = "modular-ui";
    };

    struct InboundMsg {
        char topic[96];
        char payload[256];
    };

    // Config / lifecycle
    void loadConfig();
    void buildTopics();
    void startClient();
    void stopClient();

    // MQTT event flow
    static void eventHandler(void* args, esp_event_base_t base, int32_t id, void* data);
    void onConnected();
    void enqueueInbound(const char* topic, int topicLen, const char* data, int dataLen);
    void handleInbound(const InboundMsg& msg);
    void handleLightCommand(const char* payload);
    void handleVuCommand(const char* payload);

    // Publishing
    void publishDiscovery();
    void publishAvailability(bool online);
    void publishStateIfChanged(bool force);
    void publish(const String& topic, const String& payload, int qos, bool retain);

    // Helpers
    const char* currentEffect() const;          // "Solid" / "White" / animation name
    String currentColorHex() const;             // "#RRGGBB" of the stored solid colour

    LEDManager* led_;
    Config cfg_;

    esp_mqtt_client_handle_t client_ = nullptr;
    bool connected_ = false;
    bool reconfigurePending_ = false;
    bool forcePublish_ = false;     // set on (re)connect; drained on the main loop
    QueueHandle_t inboundQueue_ = nullptr;

    // Stable identifiers / topic strings (members so their c_str() outlives client init).
    String shortId_;        // 6 hex chars from the MAC
    String objectId_;       // "modularui_<shortId>"
    String baseTopic_;      // "<prefix>/<shortId>"
    String topicLightSet_;
    String topicLightState_;
    String topicVuSet_;
    String topicVuState_;
    String topicStatus_;

    // Cached last-published snapshot for change detection.
    int lastBrightness_ = -1;
    bool lastAnim_ = false;
    int lastAnimIdx_ = -1;
    bool lastWhite_ = false;
    bool lastVu_ = false;
    uint32_t lastColor_ = 0xFFFFFFFFu;
    uint8_t lastOnBrightness_ = 255;   // remembered brightness for HA on/off restore
};

// Global MQTT manager instance (created in main.cpp, normal mode only).
extern MqttManager* g_mqttManager;
