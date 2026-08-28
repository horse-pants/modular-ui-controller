#pragma once

#include <lvgl.h>
#include <stdint.h>

/**
 * @brief A full-screen RGB565 copy of whatever LVGL last drew, held in PSRAM.
 *
 * LVGL runs in PARTIAL render mode here (a 320x10 scratch buffer), so there is
 * no framebuffer anywhere to read a screenshot out of, and the ST7796's read-back
 * would fight the render task for the parallel bus. The mirror sidesteps both:
 * every dirty rect the flush callback pushes to the panel is also copied here, so
 * a complete and current frame is always sitting in PSRAM. Reading it touches
 * neither LVGL nor the panel, which is what lets the screenshot route serve from
 * the async web task.
 *
 * Cost is one memcpy per flush (~6 KB) plus the mirror itself (w*h*2 = 300 KB).
 */
namespace ScreenMirror {

/// Allocate the mirror. False if PSRAM is too fragmented — screenshots are then
/// unavailable and everything else carries on unaffected.
bool begin(int32_t width, int32_t height);

bool isReady();
int32_t width();
int32_t height();

/// Copy one flushed area in. Called from the LVGL flush callback (render task).
/// A no-op while frozen, and while the mirror isn't allocated.
void blit(const lv_area_t* area, const uint8_t* pixels);

/// Hold the mirror still so a reader sees one coherent frame instead of one torn
/// across two refreshes. Only the copy stops updating — the live screen is
/// untouched. freeze() is not reentrant; the screenshot route is the only caller.
void freeze();
void thaw();
bool isFrozen();

/// Row accessor for the encoder. Only meaningful while frozen. Null if the row is
/// out of range or the mirror isn't allocated.
const uint16_t* row(int32_t y);

}  // namespace ScreenMirror
