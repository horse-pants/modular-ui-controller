#include <lvgl.h>

void setupVuGraph(lv_obj_t * tab);
void setupColourWheel(lv_obj_t * tab);
void setupWhiteButton(lv_obj_t * tab);
void setupEffectsList(lv_obj_t * tab);
void setupBrightnessSlider();
void setupVuButton();
void setRGB();
void setColourWheelValue(String hexstring);
String getColourWheelValue();
void setVuState(bool newState);
void setWhiteState(bool newState);
void setAnimationState(bool newState);
void setAnimation(int animation);
void setBrightness(int newBrightness);

extern lv_obj_t * whiteButton;
extern lv_obj_t * vuButton;
