#pragma once

#include <stdint.h>
#include "audio/AudioFrame.h"

/**
 * @brief One screensaver visual, rendered procedurally a strip at a time.
 *
 * Scenes never allocate a framebuffer and never touch PSRAM. ScenePlayer owns a
 * single ~10 KB internal-RAM strip and hands it to the scene repeatedly, once
 * per horizontal band of the screen, then pushes each strip straight to the
 * panel. That is what makes this affordable: an earlier version of the
 * screensaver painted a full 320x480 canvas in PSRAM and had LVGL read it back
 * to blit, which is two QSPI round-trips of 300 KB every frame.
 *
 * The per-frame budget at 30 fps is ~33 ms, of which pushing a full screen over
 * the 8-bit parallel bus costs ~7.7 ms. A scene therefore has roughly 25 ms to
 * produce 153,600 pixels — about 39 CPU cycles each. Comfortable for table
 * lookups and integer maths; not for trig or floating point per pixel.
 *
 * Implementations get seven MSGEQ7 bands and an overall level, and nothing else.
 * There is no waveform and no fine-grained spectrum, so a scene that wants
 * detail has to invent it (interpolation, history) rather than expect it.
 */
class IScene {
public:
    virtual ~IScene() = default;

    /// Short label, for a future scene picker.
    virtual const char* name() const = 0;

    /// Build lookup tables and reset state. False if it can't run.
    virtual bool start(int32_t width, int32_t height) = 0;

    /// Release anything large. Called when the screensaver is dismissed.
    virtual void stop() {}

    /// Advance one frame of animation. Called ONCE per frame, before the strips
    /// — sampling audio per strip would tear the picture across the screen.
    virtual void advance(const AudioFrame& frame) = 0;

    /// Render rows [y, y + height) into @p dst, which is `width * height`
    /// RGB565 pixels in native byte order, row-major, tightly packed.
    virtual void renderStrip(uint16_t* dst, int32_t y, int32_t height) = 0;

    /**
     * Milliseconds between frames. Override to run slower than the default 30 fps.
     *
     * Nothing syncs the strip pushes to the panel's own scan, so while a frame is
     * being written the top of the screen shows the new one and the bottom still
     * shows the old. That is invisible when consecutive frames are similar, and
     * obvious when the whole image moves — a scene in the second category tears
     * LESS at a lower frame rate, because it then spends most of its time showing
     * one coherent image instead of continuously sweeping.
     */
    virtual uint32_t frameIntervalMs() const { return 33; }
};
