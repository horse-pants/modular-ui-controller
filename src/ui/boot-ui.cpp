#include "modular-ui.h"

lv_obj_t * ta;

void setupBootUI(void)
{
    ta = lv_textarea_create(lv_scr_act());
    lv_textarea_set_one_line(ta, false);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 10);
}

void addBootText(const char *text)
{
  lv_textarea_add_text(ta, text);
}