# Replace `lv_colorwheel` with the Deck Doohickey colour wheel

`include/ui/lv_colorwheel.h` + `src/ui/lv_colorwheel.c` here are a hand-port of LVGL **v8**'s
`lv_colorwheel`, which upstream dropped in v9 and never replaced. `ColourWheel.{h,cpp}` wraps it.

A cleaner replacement now exists in **Deck Doohickey**:

- `D:\PlatformIO\Projects\DeckDoohickey\include\ui\ColorWheel.h`
- `D:\PlatformIO\Projects\DeckDoohickey\src\ui\ColorWheel.cpp`

## Why it's worth swapping

| | vendored v8 port (here) | DeckDoohickey `ColorWheel` |
|---|---|---|
| Size | ~790 lines (`.c` + `.h`) | ~250 lines |
| LVGL internals | includes four **private** headers | public API only |
| Include paths | reaches into `../../.pio/libdeps/...` — breaks whenever the env name or LVGL version changes | none |
| Config | needs `LV_USE_COLORWHEEL` in `lv_conf.h` | nothing |
| Appearance | arc segments, visible banding | continuous per-pixel gradient, feathered rim |
| Upgrades | re-port on every LVGL release | unaffected |

## How it works

Paints an HSV disc **once** into an `lv_canvas` (angle = hue, radius = saturation, value pinned at
full) and never repaints it — only a knob moves. Every later frame is a plain blit. Touch converts
the point to hue/saturation with `atan2f`/`sqrtf` and fires a live callback, so a drag previews
continuously.

Two details worth carrying over rather than rediscovering:

1. **Cache flush.** The pixels are written by the CPU but the draw unit can reach PSRAM over DMA
   (true on ESP32-P4). Without `lv_draw_buf_flush_cache()` after painting you get stale lines.
2. **RGB565 has no alpha**, so the square canvas is painted out to a `backdrop` colour matching the
   card behind it. Pass the actual card colour or the corners will show.

## Porting notes

The API is close enough to be nearly a drop-in for `ColourWheel`:

| `ColourWheel` (here) | `ColorWheel` (DeckDoohickey) |
|---|---|
| `initialize(parent, size, knobRecolor)` | `Create(parent, size, backdrop)` — returns `nullptr` on alloc failure, so handle that |
| `setColor(r, g, b, applyToLeds)` | `SetRGB(r, g, b)` — never fires the callback |
| `setCallback(cb)` | `OnChange(cb)` |
| `getColorRGB(r, g, b)` | via the `OnChange` callback |
| `setColor(hexString)` / `getColorHex()` | not provided — parse/format at the call site |
| `getHSV` / `setHSV` | not provided — add if the LED code needs it |

It costs `size * size * 2` bytes of PSRAM, held for the object's lifetime (~58 KB at 170 px). The
v8 widget drew straight into the display buffer, so this is the one real trade — worth it here
because the wheel then never re-renders.

Dropping the vendored files also lets `LV_USE_COLORWHEEL` come out of `lv_conf.h`.
