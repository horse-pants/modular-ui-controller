#pragma once

/**
 * @brief Central board pin map for the WT32-SC01 Plus.
 *
 * Single source of truth for every GPIO the firmware drives — referenced by the
 * Display module, LEDManager, and the audio task. The board breaks out very few
 * free GPIOs and this wiring is SETTLED: do not change a value without a hardware
 * reason. (The PlatformIO `board = esp32-s3-devkitc-1` is just a generic S3
 * target; the real hardware is the SC01 Plus.)
 */
namespace pins {

// --- Display: ST7796 320x480 over an 8-bit 8080 parallel bus (LovyanGFX) ---
constexpr int LCD_WR   = 47;
constexpr int LCD_RD   = -1;
constexpr int LCD_RS   = 0;   // data/command select
constexpr int LCD_D0   = 9;
constexpr int LCD_D1   = 46;
constexpr int LCD_D2   = 3;
constexpr int LCD_D3   = 8;
constexpr int LCD_D4   = 18;
constexpr int LCD_D5   = 17;
constexpr int LCD_D6   = 16;
constexpr int LCD_D7   = 15;
constexpr int LCD_CS   = -1;
constexpr int LCD_RST  = 4;
constexpr int LCD_BUSY = -1;
constexpr int LCD_BL   = 45;  // backlight (PWM)

// --- Touch: FT5x06 capacitive (I2C) ---
constexpr int TOUCH_SDA = 6;
constexpr int TOUCH_SCL = 5;

// --- LED strip data (WS2812, RMT + DMA) ---
constexpr int LED_DATA = 10;

// --- Audio: MSGEQ7 spectrum analyser ---
constexpr int MSGEQ7_STROBE = 13;
constexpr int MSGEQ7_RESET  = 21;
constexpr int MSGEQ7_ANALOG = 12;  // GPIO12 = ADC2 (shared with WiFi — paused during OTA)

}  // namespace pins
