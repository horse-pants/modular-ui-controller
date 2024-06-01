#include "modular-ui.h"
#include "ui.h"
#include <sstream> 

using namespace std;

lv_obj_t *roller;

void rollerEvent(lv_event_t * e);

void setupEffectsList(lv_obj_t * tab)
{
  roller = lv_roller_create(tab);

  String options;

  for ( int i = RAINBOW; i <= VU; i++ )
  {
    options.concat(animationDescription[i]);    
    options.concat("\n");    
  }

  lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_INFINITE);

  lv_roller_set_visible_row_count(roller, 4);
  lv_obj_set_width(roller, 300);
  lv_obj_set_height(roller, 300);
  lv_obj_center(roller);
  lv_obj_add_event_cb(roller, rollerEvent, LV_EVENT_VALUE_CHANGED, NULL);  

  static lv_style_t style_main;
    lv_style_init(&style_main);
    lv_style_set_text_font(&style_main, &lv_font_montserrat_22);
 lv_obj_add_style(roller, &style_main, LV_PART_MAIN);

   static lv_style_t style_sel;
    lv_style_init(&style_sel);
    lv_style_set_text_font(&style_sel, &lv_font_montserrat_28);
 lv_obj_add_style(roller, &style_sel, LV_PART_SELECTED);
}

void rollerEvent(lv_event_t * e)
{  
  int rollerValue = (int)lv_roller_get_selected(roller);
  showAnimation = true;
  currentAnimation = static_cast<animationOptions>(rollerValue);

  lv_obj_clear_state(whiteButton, LV_STATE_CHECKED);
  white = false;
  FastLED.clear();   
  updateWebUi();  
}

void setAnimationState(bool newState){
  showAnimation = newState;    
  if(showAnimation)
  {
    lv_obj_clear_state(whiteButton, LV_STATE_CHECKED);
    white = false;
    FastLED.clear();  
  }
  else
  {
    setRGB();
  }  
}

void setAnimation(int animation){
  showAnimation = true;
  currentAnimation = static_cast<animationOptions>(animation);
  lv_obj_clear_state(whiteButton, LV_STATE_CHECKED);
  white = false;
  FastLED.clear();  
  lv_roller_set_selected(roller, animation, LV_ANIM_ON);   
}