#include "ui/Display.h"
#include "pins.h"

#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Arduino.h>

namespace {

// LovyanGFX device for the WT32-SC01 Plus (ST7796 8-bit parallel + FT5x06 touch).
// Pin map is SETTLED — do not change without a hardware reason.
class MyLGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7796 _panel_instance;
    lgfx::Bus_Parallel8 _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_FT5x06 _touch_instance;

public:
    MyLGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.freq_write = 40000000;
            cfg.pin_wr = pins::LCD_WR;
            cfg.pin_rd = pins::LCD_RD;
            cfg.pin_rs = pins::LCD_RS;
            cfg.pin_d0 = pins::LCD_D0;
            cfg.pin_d1 = pins::LCD_D1;
            cfg.pin_d2 = pins::LCD_D2;
            cfg.pin_d3 = pins::LCD_D3;
            cfg.pin_d4 = pins::LCD_D4;
            cfg.pin_d5 = pins::LCD_D5;
            cfg.pin_d6 = pins::LCD_D6;
            cfg.pin_d7 = pins::LCD_D7;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = pins::LCD_CS;
            cfg.pin_rst = pins::LCD_RST;
            cfg.pin_busy = pins::LCD_BUSY;
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = pins::LCD_BL;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        {
            auto cfg = _touch_instance.config();
            cfg.i2c_port = 1;
            cfg.i2c_addr = 0x38;
            cfg.pin_sda = pins::TOUCH_SDA;
            cfg.pin_scl = pins::TOUCH_SCL;
            cfg.freq = 400000;
            cfg.x_min = 0;
            cfg.x_max = 320;
            cfg.y_min = 0;
            cfg.y_max = 480;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }
};

MyLGFX lcd;

// LVGL display configuration
const uint16_t screenWidth = 320;
const uint16_t screenHeight = 480;
lv_color_t buf[screenWidth * 10];

// Last screen-touch timestamp, used to trigger the idle screensaver. Written
// in touchpadRead (render task, inside lv_timer_handler) and read by UIManager
// on the same task — no synchronisation needed.
uint32_t s_lastTouchMs = 0;

// Display flush callback
void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.pushPixels((uint16_t*)px_map, w * h, true);
    lcd.endWrite();

    lv_display_flush_ready(disp);
}

// Touch read callback
void touchpadRead(lv_indev_t* indev_driver, lv_indev_data_t* data) {
    uint16_t touchX, touchY;
    bool touched = lcd.getTouch(&touchX, &touchY);

    if (!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchX;
        data->point.y = touchY;
        s_lastTouchMs = millis();  // any screen touch resets the idle timer
    }
}

}  // namespace

void Display::initPanel() {
    lcd.init();
    lcd.setRotation(2);
}

void Display::setupLvglDisplay() {
    lv_display_t* disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, displayFlush);
}

void Display::setupLvglTouch() {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpadRead);
}

uint32_t Display::lastTouchMs() {
    return s_lastTouchMs;
}
