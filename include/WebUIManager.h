#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <LittleFS.h>

#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>

/**
 * @brief Modern C++ Web UI Manager class
 * 
 * Handles web server, WebSocket communication, API endpoints, and file serving
 * with proper RAII resource management and clean interfaces.
 */
class WebUIManager {
public:
    /**
     * @brief Constructor
     * @param webServer Pointer to shared web server instance (from WiFiSetupManager)
     */
    WebUIManager(AsyncWebServer* webServer);

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~WebUIManager();

    // Disable copy operations to prevent resource issues
    WebUIManager(const WebUIManager&) = delete;
    WebUIManager& operator=(const WebUIManager&) = delete;

    /**
     * @brief Initialize web server and all endpoints
     * @return true if successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Update web UI manager (handle WebSocket cleanup)
     */
    void update();
    
    /**
     * @brief Notify all WebSocket clients with state update
     */
    void notifyClients();
    
    /**
     * @brief Check if web UI is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get WebSocket instance for external use (e.g., Logger)
     * @return Pointer to AsyncWebSocket instance
     */
    AsyncWebSocket* getWebSocket() { return &webSocket_; }

private:
    // Member variables
    bool initialized_;
    AsyncWebServer* server_;  // Pointer to shared server (from WiFiSetupManager)
    AsyncWebSocket webSocket_;
    
    // Private methods
    bool initializeLittleFS();
    void writeEmbeddedFilesToFS();
    void initializeWebSocket();
    void setupRoutes();
    void setupAPIEndpoints();
    void setupStaticFiles();
    
    // WebSocket handling
    void handleWebSocketMessage(void* arg, uint8_t* data, size_t len);
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, 
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // API response generators
    String generateAnimationsResponse();
    String generateStateResponse();
    String getCurrentSettingsResponse();
    
    // WebSocket message handlers
    void handleConnectMessage();
    void handleVuMessage(const JsonDocument& request);
    void handleWhiteMessage(const JsonDocument& request);
    void handleBrightnessMessage(const JsonDocument& request);
    void handleAnimationMessage(const JsonDocument& request);
    void handleColorMessage(const JsonDocument& request);
    
    // Route handlers
    void handleWiFiSave(AsyncWebServerRequest* request);
    void handleFactoryReset(AsyncWebServerRequest* request);
    void handleGetCurrentSettings(AsyncWebServerRequest* request);
    void handleGetNetworks(AsyncWebServerRequest* request);
    void handleScanNetworks(AsyncWebServerRequest* request);
    
    // Static WebSocket event handler (needed for C-style callback)
    static void staticWebSocketEventHandler(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                          AwsEventType type, void* arg, uint8_t* data, size_t len);
};

// Forward declaration
class OTAManager;

// Global WebUI manager instance
extern WebUIManager* g_webUIManager;

// Global OTA manager instance
extern OTAManager* g_otaManager;

// Legacy function compatibility - these will call WebUIManager methods
void setupWebUi();
void webUiLoop();
void updateWebUi();