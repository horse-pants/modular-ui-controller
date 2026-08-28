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

namespace {

/// UI_COLOR_* are 0xRRGGBB; the library's web theme wants CSS strings. Formatted
/// here rather than written out a second time by hand — the two copies had
/// already drifted (the portal's background was #0a0a0a against the device's
/// #0A0A0F, its border #444444 against #333340).
String cssColor(uint32_t rgb) {
    // Oversized: %06X takes an unsigned, so -Wformat-truncation assumes the
    // worst case even though the mask bounds it to six digits.
    char buf[16];
    snprintf(buf, sizeof(buf), "#%06X", (unsigned)(rgb & 0xFFFFFFu));
    return String(buf);
}

}  // namespace

void WifiBootManager::initializeTheme() {
    // Everything comes from modular-ui.h so the captive portal, the boot screen
    // and the app UI are the same palette by construction (theming.md).

    // LVGL boot screen.
    theme_.primaryColor = UI_COLOR_PRIMARY;
    theme_.backgroundColor = UI_COLOR_BACKGROUND;
    theme_.surfaceColor = UI_COLOR_SURFACE;
    theme_.surfaceLight = UI_COLOR_SURFACE_LIGHT;
    theme_.textColor = UI_COLOR_TEXT;
    theme_.borderColor = UI_COLOR_BORDER;

    // Web pages (/setup, /update, /logs, /factory-reset).
    theme_.webPrimaryColor = cssColor(UI_COLOR_PRIMARY);
    theme_.webPrimaryDark = cssColor(UI_COLOR_PRIMARY_DARK);
    theme_.webBackgroundColor = cssColor(UI_COLOR_BACKGROUND);
    theme_.webSurfaceColor = cssColor(UI_COLOR_SURFACE);
    theme_.webTextColor = cssColor(UI_COLOR_TEXT);
    theme_.webTextSecondary = cssColor(UI_COLOR_TEXT_MUTED);
    theme_.webBorderColor = cssColor(UI_COLOR_BORDER);

    // The library's stylesheet also uses these two, which its named fields don't
    // cover, so they stay its blue-grey defaults unless set here. cssVariables is
    // the documented escape hatch; keys carry no leading "--".
    theme_.cssVariables["surface-elevated"] = cssColor(UI_COLOR_SURFACE_LIGHT);
    theme_.cssVariables["surface-sunken"] = cssColor(UI_COLOR_SURFACE_DARK);
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
