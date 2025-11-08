#include "WiFiManager.h"
#include "modular-ui.h"
#include "ui.h"

// Static constants
const char* WiFiManager::DEFAULT_AP_NAME = "ModularUI-Setup";
const char* WiFiManager::DEFAULT_AP_PASSWORD = "modularui123";  
const char* WiFiManager::DEFAULT_HOSTNAME = "modular-ui";

// Global instance
WiFiManager* g_wifiManager = nullptr;

WiFiManager::WiFiManager()
    : hostName_(DEFAULT_HOSTNAME)
    , apName_(DEFAULT_AP_NAME)
    , apPassword_(DEFAULT_AP_PASSWORD)
    , currentConnectionAttempt_(0)
    , initialized_(false)
    , keyboard_(nullptr)
{
}

WiFiManager::~WiFiManager() {
    if (keyboard_) {
        lv_obj_del(keyboard_);
        keyboard_ = nullptr;
    }
    preferences_.end();
}

bool WiFiManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Scan for WiFi networks first
    scanNetworks();
    
    // Load configuration from preferences
    loadConfiguration();
    
    // Check if we have valid configuration
    preferences_.begin("wifi", true); // Read-only check
    bool hasConfig = preferences_.isKey("host_name");
    preferences_.end();
    
    if (!hasConfig || hostName_.isEmpty()) {
        startAPMode();
    } else {
        if (!connectToNetwork()) {
            startAPMode();
        }
    }
    
    initialized_ = true;
    return true;
}

void WiFiManager::update() {
    if (!initialized_) {
        return;
    }
    
    // Handle DNS in AP mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        handleDNS();
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() const {
    if (isConnected()) {
        return WiFi.localIP().toString();
    } else if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

bool WiFiManager::isInSetupMode() const {
    return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA;
}

void WiFiManager::scanNetworks() {
    String bootText = "Scanning WiFi networks...\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    lv_tick_inc(5);  // Update tick for LVGL 9
    lv_timer_handler();

    int n = WiFi.scanNetworks();
    scannedNetworks_ = "";
    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            if (i > 0) scannedNetworks_ += ",";
            scannedNetworks_ += WiFi.SSID(i) + "|" + String(WiFi.RSSI(i));
        }
    }
}

void WiFiManager::loadConfiguration() {
    preferences_.begin("wifi", true); // Read-only
    bool hasConfig = preferences_.isKey("host_name");
    if (hasConfig) {
        hostName_ = preferences_.getString("host_name", DEFAULT_HOSTNAME);
        wifiSsid_ = preferences_.getString("ssid", "");
        wifiPassword_ = preferences_.getString("password", "");
    }
    preferences_.end();
}

bool WiFiManager::connectToNetwork() {
    String fullHostname = hostName_ + ".local";
    String bootText = "Connecting to '" + wifiSsid_ + "'\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    bootText = "As '" + fullHostname + "'\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    lv_tick_inc(5);  // Update tick for LVGL 9
    lv_timer_handler();

    WiFi.disconnect(true);
    WiFi.setHostname(fullHostname.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid_.c_str(), wifiPassword_.c_str());
    Serial.println("");

    // Wait for connection
    currentConnectionAttempt_ = 0;
    while (WiFi.status() != WL_CONNECTED && currentConnectionAttempt_ < MAX_CONNECTION_ATTEMPTS) {
        Serial.print(".");
        if (g_bootUI) {
            g_bootUI->addText(".");
        }
        lv_tick_inc(500);  // Update tick for LVGL 9
        lv_timer_handler();
        delay(500);
        currentConnectionAttempt_++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        // Clean up boot UI before transitioning to main UI
        if (g_bootUI) {
            String ipText = "IP address: " + WiFi.localIP().toString() + "\r\n";
            g_bootUI->addText(ipText.c_str());
            lv_refr_now(NULL);

            lv_timer_handler();
        }
        delay(2000);

        cleanupBootUI();
        lv_obj_clean(lv_scr_act());
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(wifiSsid_);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed!");
        return false;
    }
}

void WiFiManager::startAPMode() {
    String bootText = "Starting Access Point Mode\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    Serial.println("Starting Access Point Mode");
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName_.c_str(), apPassword_.c_str());
    
    // Start DNS server for captive portal
    dnsServer_.start(53, "*", WiFi.softAPIP());
    
    bootText = "AP Name: " + apName_ + "\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    bootText = "Password: " + apPassword_ + "\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    bootText = "IP: " + WiFi.softAPIP().toString() + "\r\n";
    if (g_bootUI) {
        g_bootUI->addText(bootText.c_str());
    }
    
    Serial.print("AP Name: ");
    Serial.println(apName_);
    Serial.print("Password: ");
    Serial.println(apPassword_);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());
    
    // Clean up boot UI and show AP info screen
    cleanupBootUI();
    lv_obj_clean(lv_scr_act());

    // Small delay to ensure screen is cleaned
    delay(100);
    lv_tick_inc(100);  // Update tick for LVGL 9
    lv_timer_handler();

    showAPInfoScreen();
}

void WiFiManager::showAPInfoScreen() {
    lv_obj_t* screen = lv_scr_act();
    
    // Create main container
    lv_obj_t* cont = lv_obj_create(screen);
    lv_obj_set_size(cont, LV_PCT(90), LV_PCT(80));
    lv_obj_center(cont);
    lv_obj_set_style_bg_color(cont, lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(cont, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_border_width(cont, 2, 0);
    lv_obj_set_style_radius(cont, 10, 0);
    
    // Title
    lv_obj_t* title = lv_label_create(cont);
    lv_label_set_text(title, "WiFi Setup Required");
    lv_obj_set_style_text_color(title, lv_color_hex(UI_COLOR_PRIMARY), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // Instructions
    lv_obj_t* instructions = lv_label_create(cont);
    String instructionText = "1. Connect to WiFi network:\n   " + apName_ + 
                           "\n\n2. Use password:\n   " + apPassword_ + 
                           "\n\n3. Open web browser and go to:\n   " + getIPAddress() + 
                           "\n\n4. Configure your WiFi settings";
    lv_label_set_text(instructions, instructionText.c_str());
    lv_obj_set_style_text_color(instructions, lv_color_hex(UI_COLOR_TEXT), 0);
    lv_obj_set_width(instructions, LV_PCT(90));
    lv_obj_align(instructions, LV_ALIGN_CENTER, 0, 0);
}

void WiFiManager::handleDNS() {
    dnsServer_.processNextRequest();
}

// Static event handlers
void WiFiManager::textAreaEventCallback(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    if (code == LV_EVENT_FOCUSED && g_wifiManager && g_wifiManager->keyboard_) {
        lv_keyboard_set_textarea(g_wifiManager->keyboard_, ta);
        lv_obj_clear_flag(g_wifiManager->keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (code == LV_EVENT_DEFOCUSED && g_wifiManager && g_wifiManager->keyboard_) {
        lv_keyboard_set_textarea(g_wifiManager->keyboard_, NULL);
        lv_obj_add_flag(g_wifiManager->keyboard_, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (code == LV_EVENT_READY) {
        if (g_wifiManager) {
            g_wifiManager->newSsid_ = String(lv_textarea_get_text(ta));
        }
    }
}

void WiFiManager::eventHandler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
    
    if (code == LV_EVENT_READY) {
        // Handle keyboard events if needed
        if (g_wifiManager && g_wifiManager->keyboard_) {
            lv_obj_add_flag(g_wifiManager->keyboard_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}