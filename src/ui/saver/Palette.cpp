#include "ui/saver/Palette.h"

namespace {

/// Ramp stops. The floor is a very dark blue rather than black so silence still
/// reads as "on" rather than "broken", and the two mid stops are the UI's own
/// primary and accent so the screensaver belongs to the same device.
struct Stop { uint8_t at, r, g, b; };

constexpr Stop kStops[] = {
    {   0,   4,   6,  18 },
    {  90,  10,  70, 136 },
    { 174,  20, 217, 255 },   // UI_COLOR_PRIMARY
    { 224, 255,  45, 137 },   // near UI_COLOR_ACCENT
    { 255, 255, 245, 247 },
};
constexpr int kStopCount = (int)(sizeof(kStops) / sizeof(kStops[0]));

inline uint16_t rgb565(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

}  // namespace

void SaverPalette::buildDefault(uint16_t* lut256) {
    if (!lut256) return;

    for (int i = 0; i < 256; ++i) {
        int seg = 0;
        while (seg < kStopCount - 2 && i > kStops[seg + 1].at) ++seg;

        const Stop& a = kStops[seg];
        const Stop& b = kStops[seg + 1];
        const int span = (b.at - a.at) ? (b.at - a.at) : 1;
        const int t = ((i - a.at) * 255) / span;

        lut256[i] = rgb565(a.r + ((b.r - a.r) * t) / 255,
                           a.g + ((b.g - a.g) * t) / 255,
                           a.b + ((b.b - a.b) * t) / 255);
    }
}
