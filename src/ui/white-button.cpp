#include "modular-ui.h"
#include "ui.h"

lv_obj_t * whiteButton;

void whiteButtonEvent(lv_event_t * e);

void setupWhiteButton(lv_obj_t * tab)
{
  lv_obj_t * label;

  whiteButton = lv_btn_create(tab);
  lv_obj_add_event_cb(whiteButton, whiteButtonEvent, LV_EVENT_ALL, NULL);
  lv_obj_align(whiteButton, LV_ALIGN_BOTTOM_MID, 10, 0);
  lv_obj_add_flag(whiteButton, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(whiteButton, LV_SIZE_CONTENT);

  label = lv_label_create(whiteButton);
  lv_label_set_text(label, "White");
  lv_obj_center(label);
    //lv_obj_add_state(whiteButton, LV_STATE_CHECKED);

}

void whiteButtonEvent(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_VALUE_CHANGED) {
      white = !white;    
      if(white)
      {
        showAnimation = false;
        fillWhite();
      }
      else
      {
        setRGB();
      }
      updateWebUi();
  } 
}

void setWhiteState(bool newState){
  white = newState;    
  if(white)
  {
    lv_obj_add_state(whiteButton, LV_STATE_CHECKED);
    showAnimation = false;
    fillWhite();
  }
  else
  {
    lv_obj_clear_state(whiteButton, LV_STATE_CHECKED);
    setRGB();
  }
}