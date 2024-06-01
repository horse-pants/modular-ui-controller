#include "modular-ui.h"

uint8_t brightness = 128;
bool showAnimation = false;
bool vu = false;
bool white = false;
animationOptions currentAnimation = RAINBOW;

CRGB leds[NUM_LEDS];
CRGB *ledsRGB = (CRGB *) &leds[0];

const char* animationDescription[] = {
  "Rainbow",
  "Cylon",
  "RGB Chaser",
  "Beat Sine",
  "Ice Waves",
  "Purple Rain",
  "Fire",
  "Matrix",
  "VU"  
};

void startup();
void setLedBrightness();
int getAnimationInterval();
void cylon();
void rainbow();
void rgbChaser();
void beatSine();
void iceWaves();
void puepleRain();
void fire();
void matrix();
void vuAnimation();

void handleLEDs()
{
  setLedBrightness();

  EVERY_N_MILLIS_I(thistimer, 100) 
  {
    thistimer.setPeriod(getAnimationInterval());
    if(showAnimation)
    {
      switch(currentAnimation)
      {
        case CYLON: cylon(); break;
        case RAINBOW: rainbow(); break;
        case RGBCHASER: rgbChaser(); break;
        case BEATSINE: beatSine(); break;
        case ICEWAVES: iceWaves(); break;
        case PURPLERAIN: puepleRain(); break;
        case FIRE: fire(); break;
        case MATRIX: matrix(); break;
        case VU: vuAnimation(); break;
        default: break;
      }  
    }
  }
  
  FastLED.show();
}

int getAnimationInterval()
{
  switch(currentAnimation)
  {
    case CYLON: return 30;
    case RAINBOW: return 10;
    case RGBCHASER: return 30;
    case BEATSINE: return 50;
    case ICEWAVES: return 20;
    case PURPLERAIN: return 20;
    case FIRE: return 20;
    case MATRIX: return 50;
    case VU: return 5;
    default: return 100;
  }  
}

void setupFastLED()
{  
  // FastLED
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(ledsRGB, NUM_LEDS);
  FastLED.clear();    
  FastLED.setBrightness(0);  
  colorFill(CRGB::Red);
  startup();
}

void startup(){
  for(int i = 0; i <= brightness; i++){
    FastLED.setBrightness(i);
    FastLED.show();
    delay(25);
  }
}

void setLedBrightness()
{
  //FastLED.setBrightness(brightness); 
  if(vu)
  {
    FastLED.setBrightness(audioLevel); 
  }
  else
  {
    FastLED.setBrightness(brightness);  
  }  
}

void colorFill(CRGB c){
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i] = c;
  }
}

void fillWhite(){
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i] = CRGB(255, 255, 255);
  }
}


// Animations
void fadeAll(int amount) 
{ 
  for(int i = 0; i < NUM_LEDS; i++) 
  { 
    CRGB colour = leds[i];
    int red = colour.r - amount;
    if(red <= 0) 
    {
      red = 0;
    }
    
    int green = colour.g - amount;
    if(green <= 0) 
    {
      green = 0;
    }
    
    int blue = colour.b - amount;
    if(blue <= 0) 
    {
      blue = 0;
    }
    
    colour.r = red;
    colour.g = green;
    colour.b = blue;
    leds[i] = colour;
  }
}

void fadeRed(int amount) 
{ 
  for(int i = 0; i < NUM_LEDS; i++) 
  { 
    CRGB colour = leds[i];
    int red = colour.r - amount;
    if(red <= 0) 
    {
      red = 0;
    }
    
    int green = colour.g;
    int blue = colour.b;
    
    colour.r = red;
    colour.g = green;
    colour.b = blue;
    leds[i] = colour;
  }
}

void fadeGreen(int amount) 
{ 
  for(int i = 0; i < NUM_LEDS; i++) 
  { 
    CRGB colour = leds[i];
    int red = colour.r;
    
    int green = colour.g - amount;
    if(green <= 0) 
    {
      green = 0;
    }

    int blue = colour.b;
    
    colour.r = red;
    colour.g = green;
    colour.b = blue;
    leds[i] = colour;
  }
}

void cylon5(){
  static bool goingLeft = true;
  static uint8_t animationLed = 0;

  CRGB snuh = leds[animationLed];
  leds[animationLed] = CRGB::Red;
  leds[((NUM_PER_STRIP * 2) - 1) -  animationLed] = CRGB::Red;
  leds[((NUM_PER_STRIP * 4) - 1) -  animationLed] = CRGB::Red;
  leds[animationLed + (NUM_PER_STRIP * 2)] = CRGB::Red;
  leds[animationLed + (NUM_PER_STRIP * 4)] = CRGB::Red;
  fadeAll(35);

  if(goingLeft)
  {
    animationLed++;
    if(animationLed == NUM_PER_STRIP - 1)
    {
      goingLeft = false;
    }
  }
  else
  {
    animationLed--;
    if(animationLed == 0)
    {
      goingLeft = true;
    }
  }
}

void cylon3(){
  static bool goingLeft = true;
  static uint8_t animationLed = 0;

  CRGB snuh = leds[animationLed];
  leds[animationLed] = CRGB::Red;
  leds[((NUM_PER_STRIP * 2) - 1) -  animationLed] = CRGB::Red;
  leds[animationLed + (NUM_PER_STRIP * 2)] = CRGB::Red;
  fadeAll(35);

  if(goingLeft)
  {
    animationLed++;
    if(animationLed == NUM_PER_STRIP - 1)
    {
      goingLeft = false;
    }
  }
  else
  {
    animationLed--;
    if(animationLed == 0)
    {
      goingLeft = true;
    }
  }
}
void cylon(){
  cylon5();
}

void rainbow(){
  static uint8_t hue;
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i] = CHSV((i * 256 / NUM_LEDS) + hue, 255, 255);
  }
  hue++;
}

void rgbChaser(){
  static CRGB colour = CRGB::Red;    
  static int colourCount = 0;  
  static uint8_t animationLed = 0;   
  leds[animationLed] = colour;    
  animationLed++;
  if(animationLed == NUM_LEDS)
  {
    animationLed = 0;

    colourCount++;
    if(colourCount == 3) colourCount = 0;
    switch(colourCount)
    {
      case 0: colour = CRGB::Red; break;
      case 1: colour = CRGB::Green; break;
      case 2: colour = CRGB::Blue; break;
      default: break;
    }
  }
}

void beatSine(){
  uint16_t beatA = beatsin16(30, 0, 255);
  uint16_t beatB = beatsin16(20, 0, 255);
  fill_rainbow(leds, NUM_LEDS, (beatA+beatB)/2, 2);
}

int getCentreOfStrip(int strip){
  return (strip * NUM_PER_STRIP) + (NUM_PER_STRIP / 2);
}

CRGB pickColour(int led, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3){
  if(led >= 0 && led <= 7){
    return colour1;
  }
  else if (led >= 8 && led <= 11){
    return colour2;
  }
  else{
    return colour3;
  }
}

void fillFromCentre(int strip, int vuValue, CRGB colour1, CRGB colour2, CRGB colour3){
  int ledVu = map(vuValue , 0, 255, 0, (NUM_PER_STRIP + 1) / 2);
  int centre = getCentreOfStrip(strip);
  leds[centre] = colour1;
  for(int i = 1; i<= ledVu; i++){
    leds[centre + i]= pickColour(i, vuValue, colour1, colour2, colour3);
    leds[centre - i]= pickColour(i, vuValue, colour1, colour2, colour3);
  }
}

void vuAnimation(){
  getVuLevels();
  fadeAll(20);
  for(int strip = 0; strip < NUM_STRIPS; strip++){
    fillFromCentre(strip, vuValue[strip], CRGB::Green, CRGB::Orange, CRGB::Red);
  }
}


void moveFromCentre(int strip){
  int centre = getCentreOfStrip(strip);
  int ledsPerSide = (NUM_PER_STRIP) / 2;

  for(int i = ledsPerSide; i>= 0; i--){
    leds[centre + i]= leds[centre + i - 1];
    leds[centre - i]= leds[centre - i + 1];
  }
}

void moveDown(){
  for(int strip = NUM_STRIPS - 1; strip > 0; strip--){
    for(int led = 0; led < NUM_PER_STRIP; led++){
      if(strip % 2 == 1){
        leds[(NUM_PER_STRIP * strip) + (NUM_PER_STRIP - led) - 1] = leds[(NUM_PER_STRIP * (strip - 1)) + led];
      }
      else{
        leds[led + (NUM_PER_STRIP * strip)] = leds[(NUM_PER_STRIP * (strip - 1)) + (NUM_PER_STRIP - led) - 1];        
      }      
    }
  }  
}

void iceWaves(){
  getVuLevels();
  fadeAll(15);
  for(int strip = 0; strip < NUM_STRIPS; strip++){
    moveFromCentre(strip);
    int rChannel = map(vuValue[strip] , 0, 255, 100, 0);
    int gChannel = map(vuValue[strip] , 0, 255, 0, 255);
    leds[getCentreOfStrip(strip)] = CRGB(rChannel, gChannel, 255);
  }
}

void puepleRain(){
  getVuLevels();
  fadeAll(15);
  for(int strip = 0; strip < NUM_STRIPS; strip++){
    moveFromCentre(strip);
    int rChannel = map(vuValue[strip] , 0, 255, 0, 255);
    leds[getCentreOfStrip(strip)] = CRGB(rChannel, 0, 255);
  }
}

void fire(){
  getVuLevels();
  fadeGreen(15);
  fadeRed(7);
  for(int strip = 0; strip < NUM_STRIPS; strip++){
    moveFromCentre(strip);
    int gChannel = map(vuValue[strip] , 0, 255, 0, 255);
    leds[getCentreOfStrip(strip)] = CRGB(255, gChannel, 0);
  }
}

int getRandomLed(int divisions, int division){
  int range = (NUM_PER_STRIP + 1) / divisions;
  int led = rand() % range;
  return led + (range * division);
}

void matrix(){
  getVuLevels();
  moveDown();
  fadeGreen(15);
  for(int vu = 0; vu < 5; vu++){
    if(vuValue[vu] > 180){
      leds[getRandomLed(5, vu)] = CRGB::Green;
    }
  }
}

