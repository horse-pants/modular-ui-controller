#include "modular-ui.h"
#include "ui.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include <Arduino_JSON.h>


//  /*
//   AsyncElegantOTA Demo Example - This example will work for both ESP8266 & ESP32 microcontrollers.
//   -----
//   Author: Ayush Sharma ( https://github.com/ayushsharma82 )
  
//   Important Notice: Star the repository on Github if you like the library! :)
//   Repository Link: https://github.com/ayushsharma82/AsyncElegantOTA
// */

// // Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Initialize SPIFFS
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}


String getAnimations(){
  JSONVar myArray;

    myArray["message"] = "animations";
    myArray["animations"] = "test";

    for ( int i = RAINBOW; i <= VU; i++ )
{
   animationOptions current = static_cast<animationOptions>(i);
   myArray["animations"][i]["name"] = animationDescription[current];
   myArray["animations"][i]["value"] = i;
}


  String jsonString = JSON.stringify(myArray);
  
  return jsonString;
}
String getOutputStates(){
  JSONVar myArray;
  //for (int i =0; i<NUM_OUTPUTS; i++){
    myArray["message"] = "states";
    myArray["controls"][0]["name"] = "vu";
    myArray["controls"][0]["state"] = vu;
    myArray["controls"][1]["name"] = "white";
    myArray["controls"][1]["state"] = white;
    myArray["controls"][2]["name"] = "animation";
    myArray["controls"][2]["state"] = showAnimation;
    myArray["controls"][2]["animation"] = currentAnimation;
    myArray["controls"][3]["name"] = "colour";
    myArray["controls"][3]["state"] = getColourWheelValue();
  //}
  String jsonString = JSON.stringify(myArray);
  
  return jsonString;
}

void notifyClients(String state) {
  ws.textAll(state);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    JSONVar request = JSON.parse(message);

    String snuh = request["message"].operator const char *();


    if (snuh == "connect") {
      notifyClients(getAnimations());
      notifyClients(getOutputStates());
    }
    else if (snuh == "vu") {
      setVuState(request["value"].operator bool());
      notifyClients(getOutputStates());
    }   
    else if (snuh == "white") {
      setWhiteState(request["value"].operator bool());
      notifyClients(getOutputStates());
    }
    else if (snuh == "brightness") {
      setBrightness(request["value"].operator int());
      notifyClients(getOutputStates());
    }
    else if (snuh == "animation") {
      bool runAnimation = request["value"].operator bool();
      if(runAnimation){        
        setAnimation(request["animation"].operator int());
      }

      setAnimationState(runAnimation);
      notifyClients(getOutputStates());
    }
    else if (snuh == "colour"){
      setColourWheelValue(request["value"].operator const char *());
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}


void setupWebUi(){
  initSPIFFS();

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html",false);
  });

  server.serveStatic("/", SPIFFS, "/");


  ElegantOTA.begin(&server);
  server.begin();
}

void webUiLoop() {
  ws.cleanupClients();
}

void updateWebUi(){
  notifyClients(getOutputStates());
}