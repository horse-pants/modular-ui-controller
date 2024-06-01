#include "modular-ui.h"
#include <WiFi.h>
#include <Preferences.h>

void setupNetwork();
void connectToNetwork();
void ta_event_cb(lv_event_t * e);
void event_handler(lv_event_t * e);

const int maxConnectionAttempts = 10;
int currentConnectionAttempt = 0;
String hostName = "modular-ui-12u.local";
String wifiSsid;
String wifiPassword;
Preferences preferences;
String newSsid;
lv_obj_t * kb;

void setupWifi(){
  preferences.begin("wifi", false);
  wifiSsid = preferences.getString("ssid", String());
  wifiPassword = preferences.getString("password", String());
  if(wifiSsid == NULL){
    setupNetwork();
  }
  else{
    connectToNetwork();
  }
}

void connectToNetwork(){
  String bootText  = "Connecting to '" + wifiSsid + "'\r\n";
  addBootText(bootText.c_str());
  lv_timer_handler();
  
  WiFi.setHostname(hostName.c_str()); //define hostname
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED && currentConnectionAttempt < maxConnectionAttempts) {
    Serial.print(".");
    addBootText(".");
    lv_timer_handler(); /* let the GUI do its work */
    delay(500);
    currentConnectionAttempt++;
  }

  if(WiFi.status() == WL_CONNECTED)
  {
    lv_obj_clean(lv_scr_act());
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(wifiSsid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
  }
  else
  {
    bootText  = "\r\nFailed to connect, scanning networks\r\n";
    addBootText(bootText.c_str());
    lv_timer_handler();
    setupNetwork();
  }
}

void setupNetwork(){
  lv_obj_clean(lv_scr_act());
  String networks;
  WiFi.disconnect();
  int n = WiFi.scanNetworks();
  newSsid = WiFi.SSID(0);
  for (int i = 0; i < n; ++i) {
    networks += WiFi.SSID(i) + "\n";     
  }
  /*Create a normal drop down list*/
  lv_obj_t * dd = lv_dropdown_create(lv_scr_act());
  lv_dropdown_set_options(dd, networks.c_str());

  lv_obj_align(dd, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_add_event_cb(dd, event_handler, LV_EVENT_ALL, NULL);

  /*Create the password box*/
  lv_obj_t * pwd_ta = lv_textarea_create(lv_scr_act());
  lv_textarea_set_text(pwd_ta, "");
  lv_textarea_set_password_mode(pwd_ta, false);
  lv_textarea_set_one_line(pwd_ta, true);
  lv_obj_set_width(pwd_ta, lv_pct(80));
  lv_obj_add_event_cb(pwd_ta, ta_event_cb, LV_EVENT_ALL, NULL);
  lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 80);

  /*Create a label and position it above the text box*/
  lv_obj_t * pwd_label = lv_label_create(lv_scr_act());
  lv_label_set_text(pwd_label, "Password:");
  lv_obj_align_to(pwd_label, pwd_ta, LV_ALIGN_OUT_TOP_LEFT, 0, 0);

  /*Create a keyboard*/
  kb = lv_keyboard_create(lv_scr_act());
  lv_obj_set_size(kb,  LV_HOR_RES, LV_VER_RES / 2);

  lv_keyboard_set_textarea(kb, pwd_ta); /*Focus it on one of the text areas to start*/

  while(true){
    //ESP.restart();  
    lv_timer_handler();
  }
}

void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        /*Focus on the clicked text area*/
        if(kb != NULL) lv_keyboard_set_textarea(kb, ta);
    }

    else if(code == LV_EVENT_READY) {
        LV_LOG_USER("Ready, current text: %s", lv_textarea_get_text(ta));
        preferences.putString("ssid", newSsid);
        preferences.putString("password", lv_textarea_get_text(ta));
        ESP.restart();  
    }
}

void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        char buf[32];
        lv_dropdown_get_selected_str(obj, buf, sizeof(buf));
        LV_LOG_USER("Option: %s", buf);
        newSsid = buf;
    }
}

