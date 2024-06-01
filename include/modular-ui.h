//UI
#define LGFX_AUTODETECT // Autodetect board
#define LGFX_USE_V1     // set to use new version of library

#include <FastLED.h>
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <lv_conf.h>


//extern int audioLevel;

// FastLED
#define ARC_WIDTH_THICK LV_MAX(LV_DPI_DEF / 5, 5)
#define NUM_LEDS 145
#define NUM_STRIPS 5
#define NUM_PER_STRIP 29
#define DATA_PIN 10
#define MAX_BRIGHTNESS 255

void setupFastLED();
void handleLEDs();
void fillWhite();
void readFrequencies();
void colorFill(CRGB c);
void getVuLevels();
extern int vuValue[7];
extern int audioLevel;

enum animationOptions
{
  RAINBOW, 
  CYLON, 
  RGBCHASER,
  BEATSINE,
  ICEWAVES,
  PURPLERAIN,
  FIRE,
  MATRIX,
  VU
};

extern const char* animationDescription[];

extern uint8_t brightness;
extern bool showAnimation;
extern bool vu;
extern bool white;
extern animationOptions currentAnimation;


// UI
void setupScreen();
void setupBootUI();  
void addBootText(const char *text);  
void setupUI();  

// WIfi
void setupWifi();

// Web UI
void setupWebUi();
void webUiLoop();
void updateWebUi();
