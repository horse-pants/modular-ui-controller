/**
 * @file lv_colorwheel.c
 *
 * LVGL 9-compatible color wheel widget (ported from v8).
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_colorwheel.h"

#if LV_USE_COLORWHEEL

/* Private LVGL headers – same pattern as lv_arc.c, lv_slider.c, etc. */
#include "../../.pio/libdeps/esp32-s3-devkitc-1/lvgl/src/core/lv_obj_class_private.h"
#include "../../.pio/libdeps/esp32-s3-devkitc-1/lvgl/src/core/lv_obj_private.h"
#include "../../.pio/libdeps/esp32-s3-devkitc-1/lvgl/src/core/lv_obj_event_private.h"
#include "../../.pio/libdeps/esp32-s3-devkitc-1/lvgl/src/misc/lv_area_private.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS (&lv_colorwheel_class)

#define LV_CPICKER_DEF_QF 3

/**
 * OUTER_MASK_WIDTH helps when masking the outer ring. Because of integer math,
 * the ring radius jitters by up to ~2px, so this extra width ensures it’s hidden.
 */
#define OUTER_MASK_WIDTH 3

/**********************
 *      TYPEDEFS
 **********************/

/* Internal widget struct – lives only in this file */
typedef struct {
    lv_obj_t obj;              /* Must be first */
    lv_color_hsv_t hsv;
    struct {
        lv_point_t pos;
        uint8_t recolor : 1;
    } knob;
    uint32_t last_click_time;
    uint32_t last_change_time;
    lv_point_t last_press_point;
    lv_colorwheel_mode_t mode : 2;
    uint8_t mode_fixed : 1;
} lv_colorwheel_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_colorwheel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_colorwheel_event(const lv_obj_class_t * class_p, lv_event_t * e);

static void draw_disc_grad(lv_obj_t * obj, lv_layer_t * layer);
static void draw_knob(lv_obj_t * obj, lv_layer_t * layer);
static void invalidate_knob(lv_obj_t * obj);
static lv_area_t get_knob_area(lv_obj_t * obj);

static void next_color_mode(lv_obj_t * obj);
static lv_result_t double_click_reset(lv_obj_t * obj);
static void refr_knob_pos(lv_obj_t * obj);
static lv_color_t angle_to_mode_color_fast(lv_obj_t * obj, uint16_t angle);
static uint16_t get_angle(lv_obj_t * obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_colorwheel_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = lv_colorwheel_constructor,
    .event_cb = lv_colorwheel_event,
    .instance_size = sizeof(lv_colorwheel_t),
    .width_def = LV_DPI_DEF * 2,
    .height_def = LV_DPI_DEF * 2,
};

static bool create_knob_recolor;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_colorwheel_create(lv_obj_t * parent, bool knob_recolor)
{
    LV_LOG_INFO("begin");
    create_knob_recolor = knob_recolor;

    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

bool lv_colorwheel_set_hsv(lv_obj_t * obj, lv_color_hsv_t hsv)
{
    if(hsv.h > 360) hsv.h %= 360;
    if(hsv.s > 100) hsv.s = 100;
    if(hsv.v > 100) hsv.v = 100;

    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    if(colorwheel->hsv.h == hsv.h &&
       colorwheel->hsv.s == hsv.s &&
       colorwheel->hsv.v == hsv.v) {
        return false;
    }

    colorwheel->hsv = hsv;

    refr_knob_pos(obj);
    lv_obj_invalidate(obj);

    return true;
}

bool lv_colorwheel_set_rgb(lv_obj_t * obj, lv_color_t color)
{
    lv_color32_t c32 = lv_color_to_32(color, LV_OPA_COVER);
    return lv_colorwheel_set_hsv(obj,
                                 lv_color_rgb_to_hsv(c32.red, c32.green, c32.blue));
}

void lv_colorwheel_set_mode(lv_obj_t * obj, lv_colorwheel_mode_t mode)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    colorwheel->mode = mode;
    refr_knob_pos(obj);
    lv_obj_invalidate(obj);
}

void lv_colorwheel_set_mode_fixed(lv_obj_t * obj, bool fixed)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    colorwheel->mode_fixed = fixed ? 1 : 0;
}

/*=====================
 * Getter functions
 *====================*/

lv_color_hsv_t lv_colorwheel_get_hsv(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    return colorwheel->hsv;
}

lv_color_t lv_colorwheel_get_rgb(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    return lv_color_hsv_to_rgb(colorwheel->hsv.h,
                               colorwheel->hsv.s,
                               colorwheel->hsv.v);
}

lv_colorwheel_mode_t lv_colorwheel_get_color_mode(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    return colorwheel->mode;
}

bool lv_colorwheel_get_color_mode_fixed(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    return colorwheel->mode_fixed ? true : false;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_colorwheel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;
    colorwheel->hsv.h = 0;
    colorwheel->hsv.s = 100;
    colorwheel->hsv.v = 100;
    colorwheel->mode = LV_COLORWHEEL_MODE_HUE;
    colorwheel->mode_fixed = 0;
    colorwheel->last_click_time = 0;
    colorwheel->last_change_time = 0;
    colorwheel->knob.recolor = create_knob_recolor;

    lv_obj_add_flag(obj, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_set_style_arc_width(obj, 26, LV_PART_MAIN);   // 20px ring thickness

    lv_obj_add_flag(obj, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    refr_knob_pos(obj);
}

static void draw_disc_grad(lv_obj_t * obj, lv_layer_t * layer)
{
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_coord_t w  = lv_obj_get_width(obj);
    lv_coord_t h  = lv_obj_get_height(obj);
    lv_coord_t cx = obj_coords.x1 + w / 2;
    lv_coord_t cy = obj_coords.y1 + h / 2;
    lv_coord_t r  = w / 2;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    lv_obj_init_draw_line_dsc(obj, LV_PART_MAIN, &line_dsc);

    // Make sure the ring is not transparent
    line_dsc.opa = LV_OPA_COVER;

    // Segment thickness along the ring
    line_dsc.width = (r * 628 / (256 / LV_CPICKER_DEF_QF)) / 100;
    line_dsc.width += 4;

    uint16_t i;
    uint32_t a = 0;

    // Ring thickness from style (now non-zero thanks to constructor)
    lv_coord_t cir_w = lv_obj_get_style_arc_width(obj, LV_PART_MAIN);
    lv_coord_t cir_w_extra = 0;

    for(i = 0; i <= 256; i += LV_CPICKER_DEF_QF, a += 360 * LV_CPICKER_DEF_QF) {
        // Pick color for this angle based on current mode
        line_dsc.color = angle_to_mode_color_fast(obj, i);
        uint16_t angle_trigo = (uint16_t)(a >> 8);

        // Outer point on the ring
        line_dsc.p1.x = cx + ((r + cir_w_extra) * lv_trigo_sin(angle_trigo) >> LV_TRIGO_SHIFT);
        line_dsc.p1.y = cy + ((r + cir_w_extra) * lv_trigo_cos(angle_trigo) >> LV_TRIGO_SHIFT);

        // Inner point = radius minus ring thickness
        line_dsc.p2.x = cx + ((r - cir_w - cir_w_extra) * lv_trigo_sin(angle_trigo) >> LV_TRIGO_SHIFT);
        line_dsc.p2.y = cy + ((r - cir_w - cir_w_extra) * lv_trigo_cos(angle_trigo) >> LV_TRIGO_SHIFT);

        lv_draw_line(layer, &line_dsc);
    }
}

static void draw_knob(lv_obj_t * obj, lv_layer_t * layer)
{
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    lv_draw_rect_dsc_t cir_dsc;
    lv_draw_rect_dsc_init(&cir_dsc);
    lv_obj_init_draw_rect_dsc(obj, LV_PART_KNOB, &cir_dsc);

    cir_dsc.radius = LV_RADIUS_CIRCLE;

    // Ensure knob is visible
    cir_dsc.bg_opa     = LV_OPA_COVER;
    cir_dsc.border_opa = LV_OPA_COVER;
    cir_dsc.border_width = 2;
    cir_dsc.border_color = lv_color_hex(0xFFFFFF);

    if(colorwheel->knob.recolor) {
        cir_dsc.bg_color = lv_colorwheel_get_rgb(obj);
    }

    lv_area_t knob_area = get_knob_area(obj);
    lv_draw_rect(layer, &cir_dsc, &knob_area);
}


static void invalidate_knob(lv_obj_t * obj)
{
    lv_area_t knob_area = get_knob_area(obj);
    lv_obj_invalidate_area(obj, &knob_area);
}

static lv_area_t get_knob_area(lv_obj_t * obj)
{
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    /* Get knob's radius */
    uint16_t r = lv_obj_get_style_arc_width(obj, LV_PART_MAIN)/2;

    lv_coord_t left   = lv_obj_get_style_pad_left(obj,   LV_PART_KNOB);
    lv_coord_t right  = lv_obj_get_style_pad_right(obj,  LV_PART_KNOB);
    lv_coord_t top    = lv_obj_get_style_pad_top(obj,    LV_PART_KNOB);
    lv_coord_t bottom = lv_obj_get_style_pad_bottom(obj, LV_PART_KNOB);

    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    lv_area_t knob_area;
    knob_area.x1 = obj_coords.x1 + colorwheel->knob.pos.x - r - left;
    knob_area.y1 = obj_coords.y1 + colorwheel->knob.pos.y - r - right;
    knob_area.x2 = obj_coords.x1 + colorwheel->knob.pos.x + r + top;
    knob_area.y2 = obj_coords.y1 + colorwheel->knob.pos.y + r + bottom;

    return knob_area;
}

static void lv_colorwheel_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    /* Call the ancestor's event handler */
    lv_result_t res = lv_obj_event_base(MY_CLASS, e);
    if(res != LV_RESULT_OK) return;

    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;

    if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_coord_t left   = lv_obj_get_style_pad_left(obj,   LV_PART_KNOB);
        lv_coord_t right  = lv_obj_get_style_pad_right(obj,  LV_PART_KNOB);
        lv_coord_t top    = lv_obj_get_style_pad_top(obj,    LV_PART_KNOB);
        lv_coord_t bottom = lv_obj_get_style_pad_bottom(obj, LV_PART_KNOB);

        lv_coord_t knob_pad = LV_MAX4(left, right, top, bottom) + 2;
        lv_coord_t * s = lv_event_get_param(e);
        *s = LV_MAX(*s, knob_pad);
    }
    else if(code == LV_EVENT_SIZE_CHANGED) {
        const lv_area_t * old = lv_event_get_old_size(e);
        if(old) {
            if(lv_obj_get_width(obj)  != lv_area_get_width(old) ||
               lv_obj_get_height(obj) != lv_area_get_height(old)) {
                refr_knob_pos(obj);
            }
        }
        else {
            refr_knob_pos(obj);
        }
    }
    else if(code == LV_EVENT_STYLE_CHANGED) {
        refr_knob_pos(obj);
    }
    else if(code == LV_EVENT_KEY) {
        uint32_t c = lv_event_get_key(e);

        if(c == LV_KEY_RIGHT || c == LV_KEY_UP) {
            lv_color_hsv_t hsv_cur = colorwheel->hsv;

            switch(colorwheel->mode) {
                case LV_COLORWHEEL_MODE_HUE:
                    hsv_cur.h = (colorwheel->hsv.h + 1) % 360;
                    break;
                case LV_COLORWHEEL_MODE_SATURATION:
                    hsv_cur.s = (colorwheel->hsv.s + 1) % 100;
                    break;
                case LV_COLORWHEEL_MODE_VALUE:
                    hsv_cur.v = (colorwheel->hsv.v + 1) % 100;
                    break;
            }

            if(lv_colorwheel_set_hsv(obj, hsv_cur)) {
                res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
                if(res != LV_RESULT_OK) return;
            }
        }
        else if(c == LV_KEY_LEFT || c == LV_KEY_DOWN) {
            lv_color_hsv_t hsv_cur = colorwheel->hsv;

            switch(colorwheel->mode) {
                case LV_COLORWHEEL_MODE_HUE:
                    hsv_cur.h = colorwheel->hsv.h > 0 ? (colorwheel->hsv.h - 1) : 360;
                    break;
                case LV_COLORWHEEL_MODE_SATURATION:
                    hsv_cur.s = colorwheel->hsv.s > 0 ? (colorwheel->hsv.s - 1) : 100;
                    break;
                case LV_COLORWHEEL_MODE_VALUE:
                    hsv_cur.v = colorwheel->hsv.v > 0 ? (colorwheel->hsv.v - 1) : 100;
                    break;
            }

            if(lv_colorwheel_set_hsv(obj, hsv_cur)) {
                res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
                if(res != LV_RESULT_OK) return;
            }
        }
    }
    else if(code == LV_EVENT_PRESSED) {
        colorwheel->last_change_time = lv_tick_get();
        lv_indev_get_point(lv_indev_active(), &colorwheel->last_press_point);
        res = double_click_reset(obj);
        if(res != LV_RESULT_OK) return;
    }
    else if(code == LV_EVENT_PRESSING) {
        lv_indev_t * indev = lv_indev_active();
        if(indev == NULL) return;

        lv_indev_type_t indev_type = lv_indev_get_type(indev);
        lv_point_t p;

        if(indev_type == LV_INDEV_TYPE_ENCODER || indev_type == LV_INDEV_TYPE_KEYPAD) {
            p.x = obj->coords.x1 + lv_obj_get_width(obj) / 2;
            p.y = obj->coords.y1 + lv_obj_get_height(obj) / 2;
        }
        else {
            lv_indev_get_point(indev, &p);
        }

        lv_coord_t drag_limit = 10; /* Default scroll limit for LVGL 9 */
        if((LV_ABS(p.x - colorwheel->last_press_point.x) > drag_limit) ||
           (LV_ABS(p.y - colorwheel->last_press_point.y) > drag_limit)) {
            colorwheel->last_change_time = lv_tick_get();
            colorwheel->last_press_point = p;
        }

        p.x -= obj->coords.x1;
        p.y -= obj->coords.y1;

        uint16_t w = lv_obj_get_width(obj);

        int16_t angle = 0;
        lv_coord_t cir_w = lv_obj_get_style_arc_width(obj, LV_PART_MAIN);

        lv_coord_t r_in = w / 2;
        p.x -= r_in;
        p.y -= r_in;
        bool on_ring = true;
        r_in -= cir_w;
        if(r_in > LV_DPI_DEF / 2) {
            lv_coord_t inner = cir_w / 2;
            r_in -= inner;

            if(r_in < LV_DPI_DEF / 2) r_in = LV_DPI_DEF / 2;
        }

        if(p.x * p.x + p.y * p.y < r_in * r_in) {
            on_ring = false;
        }

        /* Long-press in center: change mode */
        uint32_t diff = lv_tick_elaps(colorwheel->last_change_time);
        if(!on_ring && diff > 400 && !colorwheel->mode_fixed) {
            next_color_mode(obj);
            lv_indev_wait_release(lv_indev_active());
            return;
        }

        if(!on_ring) return;

        angle = lv_atan2(p.x, p.y) % 360;

        lv_color_hsv_t hsv_cur = colorwheel->hsv;

        switch(colorwheel->mode) {
            case LV_COLORWHEEL_MODE_HUE:
                hsv_cur.h = angle;
                break;
            case LV_COLORWHEEL_MODE_SATURATION:
                hsv_cur.s = (angle * 100) / 360;
                break;
            case LV_COLORWHEEL_MODE_VALUE:
                hsv_cur.v = (angle * 100) / 360;
                break;
        }

        if(lv_colorwheel_set_hsv(obj, hsv_cur)) {
            res = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
            if(res != LV_RESULT_OK) return;
        }
    }
    else if(code == LV_EVENT_HIT_TEST) {
        /* Advanced hit test: only allow clicks inside the circle */
        lv_hit_test_info_t * info = lv_event_get_hit_test_info(e);
        if(info == NULL) return;

        const lv_point_t * p = info->point;
        lv_area_t obj_coords;
        lv_obj_get_coords(obj, &obj_coords);

        lv_coord_t w = lv_obj_get_width(obj);
        lv_coord_t h = lv_obj_get_height(obj);
        lv_coord_t cx = obj_coords.x1 + w / 2;
        lv_coord_t cy = obj_coords.y1 + h / 2;
        lv_coord_t r  = LV_MIN(w, h) / 2;

        lv_coord_t dx = p->x - cx;
        lv_coord_t dy = p->y - cy;

        if(dx * dx + dy * dy > r * r) {
            info->res = false; /* outside the circle: not clickable */
        }
        /* otherwise leave res as is (usually true) */
    }
    else if(code == LV_EVENT_DRAW_MAIN) {
        lv_layer_t * layer = lv_event_get_layer(e);
        lv_obj_t   * obj   = lv_event_get_target_obj(e);

        draw_disc_grad(obj, layer);
        draw_knob(obj, layer);
    }
    else if(code == LV_EVENT_COVER_CHECK) {
        /* Color wheel doesn't fully cover anything special; do nothing */
    }
}

static void next_color_mode(lv_obj_t * obj)
{
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;
    colorwheel->mode = (colorwheel->mode + 1) % 3;
    refr_knob_pos(obj);
    lv_obj_invalidate(obj);
}

static void refr_knob_pos(lv_obj_t * obj)
{
    invalidate_knob(obj);

    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;
    lv_coord_t w = lv_obj_get_width(obj);

    lv_coord_t scale_w = lv_obj_get_style_arc_width(obj, LV_PART_MAIN);
    lv_coord_t r = (w - scale_w) / 2;
    uint16_t angle = get_angle(obj);
    colorwheel->knob.pos.x = (((int32_t)r * lv_trigo_sin(angle)) >> LV_TRIGO_SHIFT);
    colorwheel->knob.pos.y = (((int32_t)r * lv_trigo_cos(angle)) >> LV_TRIGO_SHIFT);
    colorwheel->knob.pos.x += w / 2;
    colorwheel->knob.pos.y += w / 2;

    invalidate_knob(obj);
}

static lv_result_t double_click_reset(lv_obj_t * obj)
{
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;
    lv_indev_t * indev = lv_indev_active();

    /* Double-click timeout ~ long-press time (400 ms) */
    if(lv_tick_elaps(colorwheel->last_click_time) < 400) {
        lv_color_hsv_t hsv_cur = colorwheel->hsv;

        switch(colorwheel->mode) {
            case LV_COLORWHEEL_MODE_HUE:
                hsv_cur.h = 0;
                break;
            case LV_COLORWHEEL_MODE_SATURATION:
                hsv_cur.s = 100;
                break;
            case LV_COLORWHEEL_MODE_VALUE:
                hsv_cur.v = 100;
                break;
        }

        lv_indev_wait_release(indev);

        if(lv_colorwheel_set_hsv(obj, hsv_cur)) {
            lv_result_t r = lv_obj_send_event(obj, LV_EVENT_VALUE_CHANGED, NULL);
            if(r != LV_RESULT_OK) return r;
        }
    }
    colorwheel->last_click_time = lv_tick_get();

    return LV_RESULT_OK;
}

/* === fast HSV->RGB helper, unchanged from your code === */

#define SWAPPTR(A, B) do { uint8_t * t = A; A = B; B = t; } while(0)
#define HSV_PTR_SWAP(sextant,r,g,b) \
    if((sextant) & 2) { SWAPPTR((r), (b)); } \
    if((sextant) & 4) { SWAPPTR((g), (b)); } \
    if(!((sextant) & 6)) { \
        if(!((sextant) & 1)) { SWAPPTR((r), (g)); } \
    } else { \
        if((sextant) & 1) { SWAPPTR((r), (g)); } \
    }

/**
 * Fast approximate HSV->RGB (for drawing the wheel).
 * Based on https://www.vagrearg.org/content/hsvrgb
 */
static void fast_hsv2rgb(uint16_t h, uint8_t s, uint8_t v,
                         uint8_t * r, uint8_t * g, uint8_t * b)
{
    if(!s) {
        *r = *g = *b = v;
        return;
    }

    uint8_t sextant = h >> 8;
    HSV_PTR_SWAP(sextant, r, g, b);

    *g = v;

    uint8_t bb = ~s;
    uint16_t ww = v * bb;
    *b = ww >> 8;

    uint8_t h_frac = h & 0xff;

    if(!(sextant & 1)) {
        ww = !h_frac ? ((uint16_t)s << 8) : (s * (uint8_t)(-h_frac));
    }
    else {
        ww = s * h_frac;
    }
    bb = ww >> 8;
    bb = ~bb;
    ww = v * bb;
    *r = ww >> 8;
}

static lv_color_t angle_to_mode_color_fast(lv_obj_t * obj, uint16_t angle)
{
    lv_colorwheel_t * ext = (lv_colorwheel_t *)obj;
    uint8_t r = 0, g = 0, b = 0;
    static uint16_t h = 0;
    static uint8_t s = 0, v = 0, m = 255;
    static uint16_t angle_saved = 0xffff;

    if(angle_saved != angle) m = 255;
    angle_saved = angle;

    switch(ext->mode) {
        default:
        case LV_COLORWHEEL_MODE_HUE:
            if(m != ext->mode) {
                s = (uint8_t)(((uint16_t)ext->hsv.s * 51) / 20);
                v = (uint8_t)(((uint16_t)ext->hsv.v * 51) / 20);
                m = ext->mode;
            }
            fast_hsv2rgb(angle * 6, s, v, &r, &g, &b);
            break;
        case LV_COLORWHEEL_MODE_SATURATION:
            if(m != ext->mode) {
                h = (uint16_t)(((uint32_t)ext->hsv.h * 6 * 256) / 360);
                v = (uint8_t)(((uint16_t)ext->hsv.v * 51) / 20);
                m = ext->mode;
            }
            fast_hsv2rgb(h, angle, v, &r, &g, &b);
            break;
        case LV_COLORWHEEL_MODE_VALUE:
            if(m != ext->mode) {
                h = (uint16_t)(((uint32_t)ext->hsv.h * 6 * 256) / 360);
                s = (uint8_t)(((uint16_t)ext->hsv.s * 51) / 20);
                m = ext->mode;
            }
            fast_hsv2rgb(h, s, angle, &r, &g, &b);
            break;
    }
    return lv_color_make(r, g, b);
}

static uint16_t get_angle(lv_obj_t * obj)
{
    lv_colorwheel_t * colorwheel = (lv_colorwheel_t *)obj;
    uint16_t angle;
    switch(colorwheel->mode) {
        default:
        case LV_COLORWHEEL_MODE_HUE:
            angle = colorwheel->hsv.h;
            break;
        case LV_COLORWHEEL_MODE_SATURATION:
            angle = (colorwheel->hsv.s * 360) / 100;
            break;
        case LV_COLORWHEEL_MODE_VALUE:
            angle = (colorwheel->hsv.v * 360) / 100;
            break;
    }
    return angle;
}

#endif /* LV_USE_COLORWHEEL */
