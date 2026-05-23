#pragma once

#include <Arduino.h>
#include <WiFiSetupManager.h>
#include <WiFiSetupBootUI.h>

/**
 * @brief Network manager that wraps WiFi setup, boot UI, and theming
 *
 * Encapsulates all network-related initialization including:
 * - WiFiSetupManager (handles WiFi connection/AP mode)
 * - WiFiSetupBootUI (displays connection status on screen)
 * - Theme configuration for consistent styling
 */
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // Disable copy operations
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    /**
     * @brief Initialize network manager and start WiFi
     * @param apName Default AP name for setup mode
     * @param apPassword Default AP password for setup mode
     * @return true if initialization successful
     */
    bool initialize(const char* apName = "ModularUI-Setup",
                    const char* apPassword = "modularui123");

    /**
     * @brief Update network manager (call in loop)
     */
    void update();

    /**
     * @brief Check if in WiFi setup mode (AP mode)
     */
    bool isInSetupMode() const;

    /**
     * @brief Get the shared web server instance
     */
    AsyncWebServer* getWebServer();

    /**
     * @brief Get the theme for use by other components
     */
    WiFiSetupTheme* getTheme() { return &theme_; }

    /**
     * @brief Get boot UI for status updates
     */
    WiFiSetupBootUI* getBootUI() { return bootUI_; }

    /**
     * @brief Add text to boot UI console
     */
    void addBootText(const char* text);

    /**
     * @brief Clean up boot UI after setup complete
     */
    void cleanupBootUI();

private:
    WiFiSetupManager* wifiManager_;
    WiFiSetupBootUI* bootUI_;
    WiFiSetupTheme theme_;
    bool initialized_;

    void initializeTheme();
    void loadHostname();
};

// Global network manager instance
extern NetworkManager* g_networkManager;
