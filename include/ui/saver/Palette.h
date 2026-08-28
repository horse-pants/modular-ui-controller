#pragma once

#include <stdint.h>

/**
 * @brief The screensaver's shared colour ramp and RGB565 helpers.
 *
 * Every scene maps "how loud is this" onto the same ramp — deep blue floor,
 * through the UI's cyan, into its magenta, topping out near white — so the
 * scenes read as one family rather than three unrelated toys.
 *
 * Dithering matters more here than it looks. The panel is RGB565, so a 256-entry
 * ramp collapses onto ~32 blue levels; a smooth gradient across a 480 px screen
 * then shows as visible steps. A 4x4 ordered (Bayer) offset applied before the
 * lookup breaks those steps up for about two cycles a pixel.
 */
namespace SaverPalette {

/// Fill a 256-entry RGB565 lookup table with the shared ramp.
void buildDefault(uint16_t* lut256);

/**
 * Dither offset for a pixel, to be added to a 0..255 level BEFORE the palette
 * lookup. Small and zero-mean, so it trades a little noise for the banding.
 */
inline int32_t ditherAt(int32_t x, int32_t y) {
    static const int8_t kBayer4[16] = {
        -6, 2, -4, 4,
         5, -3, 7, -1,
        -3, 5, -5, 3,
         6, 0, 8, -2,
    };
    return kBayer4[((y & 3) << 2) | (x & 3)];
}

/// Clamp a dithered level back into the palette's range.
inline uint8_t clampLevel(int32_t v) {
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

}  // namespace SaverPalette
