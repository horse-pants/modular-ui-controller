#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <lvgl.h>

/**
 * @brief Modern C++ WiFi Manager class
 * 
 * Handles WiFi connection, AP mode, network scanning, and configuration
 * with proper RAII resource management and clean interfaces.
 */
class WiFiManager {
public:
    /**
     * @brief Constructor
     */
    WiFiManager();
    
    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~WiFiManager();
    
    // Disable copy operations to prevent resource issues
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    
    /**
     * @brief Initialize and setup WiFi connection
     * @return true if successful, false otherwise
     */
    bool initialize();
    
    /**
     * @brief Update WiFi manager (handle DNS, etc.)
     */
    void update();
    
    /**
     * @brief Get scanned networks as comma-separated string
     * @return String containing network list with RSSI
     */
    String getScannedNetworks() const { return scannedNetworks_; }
    
    /**
     * @brief Check if in setup mode (AP mode for configuration)
     * @return true if in setup mode, false if configured and connected
     */
    bool isInSetupMode() const;
    
    /**
     * @brief Check if WiFi is connected
     * @return true if connected, false otherwise
     */
    bool isConnected() const;
    
    /**
     * @brief Get current IP address
     * @return IP address as string
     */
    String getIPAddress() const;
    
    /**
     * @brief Get current hostname
     * @return hostname as string
     */
    String getHostname() const { return hostName_; }

private:
    // Configuration constants
    static const int MAX_CONNECTION_ATTEMPTS = 10;
    static const char* DEFAULT_AP_NAME;
    static const char* DEFAULT_AP_PASSWORD;
    static const char* DEFAULT_HOSTNAME;
    
    // Member variables
    String hostName_;
    String wifiSsid_;
    String wifiPassword_;
    String apName_;
    String apPassword_;
    String scannedNetworks_;
    String newSsid_;
    
    int currentConnectionAttempt_;
    bool initialized_;
    
    Preferences preferences_;
    DNSServer dnsServer_;
    lv_obj_t* keyboard_;
    
    // Private methods
    void scanNetworks();
    void loadConfiguration();
    bool connectToNetwork();
    void startAPMode();
    void showAPInfoScreen();
    void handleDNS();
    
    // LVGL event handlers
    static void textAreaEventCallback(lv_event_t* e);
    static void eventHandler(lv_event_t* e);
};

// Global WiFi manager instance
extern WiFiManager* g_wifiManager;