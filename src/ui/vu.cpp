#include "modular-ui.h"
#include "ui.h"
#include "AudioAnalyzer.h"
#include <Filter.h>

#define NUM_VU_CHANNELS 7

using namespace std;

lv_obj_t * label1;
lv_obj_t * vuButton;
lv_obj_t * chart;
lv_chart_series_t * ser1;
int vuValue[7];

ExponentialFilter<int> filters[NUM_VU_CHANNELS] = {
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0),
  ExponentialFilter<int>(10, 0)
};

Analyzer Audio = Analyzer(13,21,12);//Strobe pin ->4  RST pin ->5 Analog Pin ->A0
ExponentialFilter<int> audioFilter(10, 0) ;
int audioLevel = 0;

void vuHandler(lv_event_t * e);

void setupVuButton()
{
  
  lv_obj_t * label;

  vuButton = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(vuButton, vuHandler, LV_EVENT_ALL, NULL);
  lv_obj_align(vuButton, LV_ALIGN_TOP_RIGHT, -20, 70);
  lv_obj_add_flag(vuButton, LV_OBJ_FLAG_CHECKABLE);
  lv_obj_set_height(vuButton, LV_SIZE_CONTENT);

  label = lv_label_create(vuButton);
  lv_label_set_text(label, "Follow VU");
  lv_obj_center(label);

  Audio.Init();
}

int getOverAllVolume(){
  int totalVolume = 0;
  for(int i = 0; i< NUM_VU_CHANNELS; i++){
    totalVolume += filters[i].Current();
  }
  audioFilter.Filter(totalVolume / NUM_VU_CHANNELS);
  return audioFilter.Current();
}



void getVuLevels5() {
  for (int i = 0; i < NUM_STRIPS; ++i) {
    if(i == 0){
      int maxVu = max(filters[i].Current(), filters[i + 1].Current());
      vuValue[i] = maxVu;

    }
    else if (i > 0 && i < 4){
      vuValue[i] = filters[i + 1].Current();
    }
    else if (i == 4){
      int maxVu = max(filters[i + 1].Current(), filters[i + 2].Current());
      vuValue[i] = maxVu;
    }
  }
}

void getVuLevels3() {
  for (int i = 0; i < NUM_STRIPS; ++i) {
    if(i == 0){
      int maxVu = max(filters[0].Current(), filters[1].Current());
      vuValue[i] = maxVu;
    }
    else if (i == 1){
      int maxVu = max(filters[2].Current(), filters[3].Current());
      maxVu = max(maxVu, filters[4].Current());
      vuValue[i] = maxVu;
    }
    else if (i == 2){
      int maxVu = max(filters[5].Current(), filters[6].Current());
      vuValue[i] = maxVu;
    }
  }
}

void getVuLevels() {
  getVuLevels5();
}

void readFrequencies(){
  int FreqVal[7];

  Audio.ReadFreq(FreqVal);

  for(int i = 0; i< NUM_VU_CHANNELS; i++){
    int mappedValue = map(FreqVal[i] , 0, 4096, 0, 255);
    filters[i].Filter(mappedValue);
    lv_chart_set_value_by_id(chart, ser1, i, filters[i].Current());

  } 

  audioLevel = getOverAllVolume();

  // char str[256];
  // itoa(FreqVal[0],str,10); //Base 10
  // lv_label_set_text(label1, str);
}

static void draw_event_cb(lv_event_t * e)
{
  lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

  if(dsc->part == LV_PART_ITEMS) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_chart_series_t * ser = lv_chart_get_series_next(obj, NULL);
    uint32_t cnt = lv_chart_get_point_count(obj);
       
    int mix = 0;
    if(ser->y_points[dsc->id] < 150){
      mix = map(ser->y_points[dsc->id] , 0, 150, 0, 255);
      dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_ORANGE), lv_palette_main(LV_PALETTE_GREEN), mix);
    }
    else{
      mix = map(ser->y_points[dsc->id] - 150 , 0, 105, 0, 255);
      dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_RED), lv_palette_main(LV_PALETTE_ORANGE), mix);
    }
  }

  if(!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL)) return;

  if(dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text) {
    const char * month[] = {"63", "160", "400", "1K", "2.5K", "6.3K", "16K"};
    lv_snprintf(dsc->text, dsc->text_length, "%s", month[dsc->value]);
  }
}

void setupVuGraph(lv_obj_t * tab)
{
  /*Create a chart*/
  chart = lv_chart_create(tab);
  lv_obj_set_size(chart, 300, 200);
  lv_obj_center(chart);
  lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 255);
  lv_chart_set_point_count(chart, 7);
  lv_obj_add_event_cb(chart, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    /*Add ticks and label to every axis*/
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 10, 5, 7, 1, true, 40);

  /*Add two data series*/
  ser1 = lv_chart_add_series(chart, lv_palette_lighten(LV_PALETTE_GREEN, 2), LV_CHART_AXIS_PRIMARY_Y);

  /*Set the next points on 'ser1'*/
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
  lv_chart_set_next_value(chart, ser1, 0);
    
  lv_chart_refresh(chart); /*Required after direct set*/

  // label1 = lv_label_create(tab);
  // lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);     /*Break the long lines*/
  // lv_label_set_text(label1, "0");
  // lv_obj_set_width(label1, 150);  /*Set smaller width to make the lines wrap*/
  // lv_obj_set_style_text_align(label1, LV_TEXT_ALIGN_CENTER, 0);
  // lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);
}

void vuHandler(lv_event_t * e)
{
  lv_event_code_t code = lv_event_get_code(e);

  if(code == LV_EVENT_VALUE_CHANGED) {
    vu = !vu;
    updateWebUi();
  }
}

void setVuState(bool newState){
  vu = newState;
  if(vu)
  {
    lv_obj_add_state(vuButton, LV_STATE_CHECKED);
  }
  else
  {
    lv_obj_clear_state(vuButton, LV_STATE_CHECKED);
  }
}