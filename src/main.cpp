#include "modular-ui.h"
#include <ElegantOTA.h>

void setup(void)
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("started...");

  setupScreen();
  setupBootUI();
  setupWifi();
  setupUI();
  setupFastLED(); 
  setupWebUi();  
}

void loop()
{
  lv_timer_handler(); /* let the GUI do its work */
  handleLEDs();    
  webUiLoop();
  readFrequencies();
  ElegantOTA.loop();
}  
