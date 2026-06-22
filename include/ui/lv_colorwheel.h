/**
 * @file lv_colorwheel.h
 *
 * LVGL 9-compatible color wheel widget (public API)
 */

#ifndef LV_COLORWHEEL_H
#define LV_COLORWHEEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
//#include "lv_draw/lv_draw.h"

#if LV_USE_COLORWHEEL

/**********************
 *      TYPEDEFS
 **********************/

/* Color wheel mode: which HSV component is mapped to the angle */
typedef enum {
    LV_COLORWHEEL_MODE_HUE = 0,
    LV_COLORWHEEL_MODE_SATURATION,
    LV_COLORWHEEL_MODE_VALUE,
} lv_colorwheel_mode_t;

/* The class descriptor, used by LVGL’s object system */
extern const lv_obj_class_t lv_colorwheel_class;

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

/**
 * Create a color wheel object.
 * @param parent       pointer to parent object
 * @param knob_recolor true: knob is recolored to current color
 * @return             pointer to created object
 */
lv_obj_t * lv_colorwheel_create(lv_obj_t * parent, bool knob_recolor);

/*=====================
 * Setter functions
 *====================*/

/**
 * Set the current HSV value.
 * @param obj color wheel object
 * @param hsv new HSV value
 * @return    true if changed, false otherwise
 */
bool lv_colorwheel_set_hsv(lv_obj_t * obj, lv_color_hsv_t hsv);

/**
 * Set the current RGB value.
 * @param obj   color wheel object
 * @param color new RGB color
 * @return      true if changed, false otherwise
 */
bool lv_colorwheel_set_rgb(lv_obj_t * obj, lv_color_t color);

/**
 * Set the current color mode (hue/sat/value).
 * @param obj  color wheel object
 * @param mode color mode
 */
void lv_colorwheel_set_mode(lv_obj_t * obj, lv_colorwheel_mode_t mode);

/**
 * Enable/disable mode change on long-press in center.
 * @param obj   color wheel object
 * @param fixed true: mode is fixed, no change on long press
 */
void lv_colorwheel_set_mode_fixed(lv_obj_t * obj, bool fixed);

/*=====================
 * Getter functions
 *====================*/

/**
 * Get current HSV value.
 * @param obj color wheel object
 * @return    HSV value
 */
lv_color_hsv_t lv_colorwheel_get_hsv(lv_obj_t * obj);

/**
 * Get current RGB value.
 * @param obj color wheel object
 * @return    RGB color
 */
lv_color_t lv_colorwheel_get_rgb(lv_obj_t * obj);

/**
 * Get current mode (hue/sat/value).
 * @param obj color wheel object
 * @return    mode
 */
lv_colorwheel_mode_t lv_colorwheel_get_color_mode(lv_obj_t * obj);

/**
 * Get whether mode is fixed (no long-press toggle).
 * @param obj color wheel object
 * @return    true if mode cannot be changed by long press
 */
bool lv_colorwheel_get_color_mode_fixed(lv_obj_t * obj);

#endif /* LV_USE_COLORWHEEL */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_COLORWHEEL_H */
