#include "led/LedDriver.h"
#include <Logger.h>

bool LedDriver::begin(int dataPin, int numPixels) {
    if (strip_) {
        return true;
    }

    numPixels_ = numPixels;

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num         = dataPin;
    strip_config.max_leds               = numPixels;
    strip_config.led_model              = LED_MODEL_WS2812;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.flags.invert_out       = false;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src           = RMT_CLK_SRC_DEFAULT;
    rmt_config.resolution_hz     = 10 * 1000 * 1000;  // 10 MHz, 0.1 µs/tick — WS2812 timing
    rmt_config.mem_block_symbols = 1024;              // Large block, DMA-fed; required when with_dma=true
    rmt_config.flags.with_dma    = true;              // The whole point — DMA-driven RMT5

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
    if (err != ESP_OK) {
        Logger.error("led_strip_new_rmt_device failed: %d", err);
        strip_ = nullptr;
        return false;
    }

    led_strip_clear(strip_);
    led_strip_refresh(strip_);
    return true;
}

void LedDriver::end() {
    if (strip_) {
        led_strip_del(strip_);
        strip_ = nullptr;
    }
}

void LedDriver::show(const CRGB* buffer) {
    if (!strip_ || !buffer) {
        return;
    }
    for (int i = 0; i < numPixels_; ++i) {
        led_strip_set_pixel(strip_, i,
                            scale8(buffer[i].r, brightness_),
                            scale8(buffer[i].g, brightness_),
                            scale8(buffer[i].b, brightness_));
    }
    led_strip_refresh(strip_);
}

void LedDriver::clear() {
    if (!strip_) {
        return;
    }
    led_strip_clear(strip_);
    led_strip_refresh(strip_);
}
