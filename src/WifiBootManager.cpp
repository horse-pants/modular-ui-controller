#include "WifiBootManager.h"
#include "modular-ui.h"
#include <Logger.h>
#include <Preferences.h>

WifiBootManager* g_wifiBootManager = nullptr;

WifiBootManager::WifiBootManager()
    : wifiManager_(nullptr)
    , bootUI_(nullptr)
    , initialized_(false)
{
    initializeTheme();
}

WifiBootManager::~WifiBootManager() {
    if (bootUI_) {
        bootUI_->cleanup();
        delete bootUI_;
        bootUI_ = nullptr;
    }
    if (wifiManager_) {
        delete wifiManager_;
        wifiManager_ = nullptr;
    }
}

void WifiBootManager::initializeTheme() {
    // LVGL colors (from modular-ui.h)
    theme_.primaryColor = UI_COLOR_PRIMARY;
    theme_.backgroundColor = UI_COLOR_BACKGROUND;
    theme_.surfaceColor = UI_COLOR_SURFACE;
    theme_.surfaceLight = UI_COLOR_SURFACE_LIGHT;
    theme_.textColor = UI_COLOR_TEXT;
    theme_.borderColor = UI_COLOR_BORDER;

    // Web UI colors
    theme_.webPrimaryColor = "#00D9FF";
    theme_.webPrimaryDark = "#00B8E6";
    theme_.webBackgroundColor = "#0a0a0a";
    theme_.webSurfaceColor = "#1a1a1a";
    theme_.webTextColor = "#e0e0e0";
    theme_.webTextSecondary = "#b0b0b0";
    theme_.webBorderColor = "#444444";
}

void WifiBootManager::loadHostname() {
    if (!bootUI_) return;

    Preferences prefs;
    prefs.begin("wifi", true);  // Read-only
    String hostname = prefs.getString("host_name", "");
    prefs.end();

    if (hostname.length() > 0) {
        bootUI_->setDeviceName(hostname);
    }
}

bool WifiBootManager::initialize(const char* apName, const char* apPassword) {
    if (initialized_) {
        return true;
    }

    // Create boot UI
    bootUI_ = new WiFiSetupBootUI();
    if (!bootUI_) {
        Logger.error("WifiBootManager: Failed to create boot UI");
        return false;
    }

    // Initialize boot UI with theme
    if (!bootUI_->initialize("MODULAR UI CONTROLLER", &theme_)) {
        Logger.error("WifiBootManager: Failed to initialize boot UI");
        return false;
    }
    bootUI_->addText("ModularUI Controller Starting...\r\n");

    // Create WiFi setup manager
    WiFiSetupConfig config;
    config.defaultAPName = apName;
    config.defaultAPPassword = apPassword;
    config.statusCallback = bootUI_;
    config.theme = &theme_;

    wifiManager_ = new WiFiSetupManager(config);
    if (!wifiManager_) {
        Logger.error("WifiBootManager: Failed to create WiFi manager");
        return false;
    }

    // Register Logger endpoints before starting WiFi
    Logger.registerEndpoints(wifiManager_->getWebServer());
    Logger.info("Logger web endpoints registered");

    // Load hostname before begin() so it's available for callbacks
    loadHostname();

    // Start WiFi
    wifiManager_->begin();
    Logger.info("Logger web interface available at /logs");

    initialized_ = true;
    return true;
}

void WifiBootManager::update() {
    if (wifiManager_) {
        wifiManager_->update();
    }
}

bool WifiBootManager::isInSetupMode() const {
    return wifiManager_ ? wifiManager_->isInSetupMode() : true;
}

AsyncWebServer* WifiBootManager::getWebServer() {
    return wifiManager_ ? wifiManager_->getWebServer() : nullptr;
}

void WifiBootManager::addBootText(const char* text) {
    if (bootUI_) {
        bootUI_->addText(text);
    }
}

void WifiBootManager::cleanupBootUI() {
    if (bootUI_) {
        bootUI_->cleanup();
        delete bootUI_;
        bootUI_ = nullptr;
    }
}
