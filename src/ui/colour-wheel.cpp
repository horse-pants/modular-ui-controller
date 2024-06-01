#include "modular-ui.h"
#include "ui.h"

lv_obj_t * cw;

void colourWheelEvent(lv_event_t * e);
String createRGB(int r, int g, int b);

void setupColourWheel(lv_obj_t * tab)
{
  cw = lv_colorwheel_create(tab, true);
  lv_obj_set_size(cw, 200, 200);
  lv_obj_center(cw);

  static lv_style_t style_indic;
  lv_style_init(&style_indic);
  lv_style_set_arc_width(&style_indic, ARC_WIDTH_THICK);

  lv_obj_add_style(cw, &style_indic, LV_PART_MAIN);  
  lv_obj_add_event_cb(cw, colourWheelEvent, LV_EVENT_VALUE_CHANGED, NULL);
}

void colourWheelEvent(lv_event_t * e)
{
  setRGB();  
}


void setRGB()
{
  lv_color16_t color = lv_colorwheel_get_rgb(cw);
  lv_color32_t c32;
  c32.full = lv_color_to32(color);
  showAnimation = false;
  FastLED.clear();
  colorFill(CRGB( c32.ch.red, c32.ch.green, c32.ch.blue));
  lv_obj_clear_state(whiteButton, LV_STATE_CHECKED);
  white = false;
  updateWebUi();
}

void setColourWheelValue(String hexstring){

Serial.println(hexstring);
  // Get rid of '#' and convert it to integer
  int number = (int) strtol( &hexstring[1], NULL, 16);
  lv_color_t color = lv_color_hex(number);

  lv_colorwheel_set_rgb(cw, color);
  setRGB();  
}

String getColourWheelValue()
{
  lv_color16_t color = lv_colorwheel_get_rgb(cw);
  lv_color32_t c32;
  c32.full = lv_color_to32(color);

  String colorString = createRGB(c32.ch.red, c32.ch.green, c32.ch.blue);

  return String(colorString);
}

String createRGB(int r, int g, int b)
{   
  long value = ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
  char myHex[10] = "";
  ltoa(value, myHex,16); //convert to c string base 16
  String stringValue = String(myHex);
    
  while(stringValue.length() < 6)
  {
    stringValue = "0" + stringValue;
  }

  return "#" + stringValue;
}