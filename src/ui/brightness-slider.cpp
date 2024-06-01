#include "modular-ui.h"
#include "ui.h"

lv_obj_t * brightnessSlider;

void brightnessSliderEvent(lv_event_t * e);

void setupBrightnessSlider()
{
  /*Create a slider in the center of the display*/
  brightnessSlider = lv_slider_create(lv_scr_act());
  lv_obj_set_width(brightnessSlider, 150);     
  lv_obj_set_height(brightnessSlider, 30);     
  lv_obj_align(brightnessSlider, LV_ALIGN_TOP_LEFT, 20, 70);
  lv_obj_add_event_cb(brightnessSlider, brightnessSliderEvent, LV_EVENT_VALUE_CHANGED, NULL);

  static lv_style_t style_indic1;

  lv_style_init(&style_indic1);
  lv_style_set_bg_opa(&style_indic1, LV_OPA_COVER);
  lv_style_set_bg_color(&style_indic1, lv_color_make(0,0,0));
  lv_style_set_bg_grad_color(&style_indic1, lv_color_make(255,255,255));
  lv_style_set_bg_grad_dir(&style_indic1, LV_GRAD_DIR_HOR);

  lv_obj_add_style(brightnessSlider, &style_indic1, LV_PART_INDICATOR);
  lv_slider_set_range(brightnessSlider, 0, MAX_BRIGHTNESS);
  lv_slider_set_value(brightnessSlider, brightness, LV_ANIM_ON);
}

void brightnessSliderEvent(lv_event_t * e)
{  
  int sliderValue = (int)lv_slider_get_value(brightnessSlider);
  brightness = sliderValue;
  vu = false;
  lv_obj_clear_state(vuButton, LV_STATE_CHECKED);
  updateWebUi();
}

void setBrightness(int newBrightness)
{  
  lv_slider_set_value(brightnessSlider, newBrightness, LV_ANIM_ON);
  brightness = newBrightness;
  vu = false;
  lv_obj_clear_state(vuButton, LV_STATE_CHECKED);  
}