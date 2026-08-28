# LVGL v9 Rules

## API version

- **Search the codebase before using LVGL functions** — v8 APIs simply don't exist in v9. Don't trust pre-v9 examples or stale memory.

## Config lives in `include/lv_conf.h`

`platformio.ini` adds `-I include` so LVGL picks up the project-level `lv_conf.h` directly. When toggling `LV_USE_*` / `LV_FONT_*` settings, edit `include/lv_conf.h` — that's the single source of truth.

## State-based styling

- **Define styles ONCE, toggle with `lv_obj_add_state()` / `lv_obj_clear_state()`**
- **NEVER call `lv_obj_set_style_*` repeatedly in loops or update callbacks** — this causes the UI to freeze. The audio-reactive VU update path is the hot spot to watch.
- For dynamic visuals (selected/active/disabled), pre-build the style and switch state.

## Tabview uses buttons, NOT a btnmatrix

In v9, the tab bar is a row of regular buttons — `LV_PART_ITEMS` styling that worked under v8's btnmatrix does nothing. Iterate the tab bar children and style each button individually:

```cpp
// ❌ WRONG (v8 style — silently no-ops):
lv_obj_set_style_text_color(tab_btns, color, LV_PART_ITEMS);

// ✅ CORRECT (v9):
lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview_);
for (uint32_t i = 0; i < lv_obj_get_child_count(tab_bar); i++) {
    lv_obj_t* btn = lv_obj_get_child(tab_bar, i);
    lv_obj_set_style_text_color(btn, color, 0);                  // inactive
    lv_obj_set_style_text_color(btn, color, LV_STATE_CHECKED);   // active
}
```

## Dropdown lists

- The dropdown list widget **doesn't exist until the dropdown is opened.** Style it from the `LV_EVENT_READY` / open handler — not at construction time.
- Use `lv_obj_set_scroll_dir(list, LV_DIR_VER)` to constrain dropdown scroll to vertical only.

## Colour wheel

LVGL 9 ships no colour picker (v8's `lv_colorwheel` was dropped and never replaced), so
`ColourWheel` paints an HSV disc pixel-by-pixel into an `lv_canvas` — angle = hue, radius =
saturation, value pinned at full. Things that are easy to break:

- **Paint once, never repaint.** The buffer is retained for the object's lifetime; only the knob
  moves, so every later frame is a plain blit. Don't add anything that re-paints per frame.
- **Flush the cache after painting.** The pixels are written by the CPU but the draw unit can reach
  PSRAM over DMA — without `lv_draw_buf_flush_cache()` you get stale lines.
- **RGB565 has no alpha**, so the square canvas is painted out to a `backdrop` colour. Pass the
  actual colour of the card behind it (`UI_COLOR_SURFACE`) or the corners show.
- The knob **overhangs the square** by half its width at full saturation. That needs
  `LV_OBJ_FLAG_OVERFLOW_VISIBLE` on the wheel's wrapper *and* ~20px of container padding around it.
- **A press must not report a colour.** A press is also how a tab swipe begins; the colour goes out
  on `PRESSING` (drag) or `RELEASED` (tap), and `lv_indev_get_scroll_obj()` is what tells the wheel
  a drag became a swipe — nothing else guards it.
- **The PSRAM alloc can fail** on a fragmented heap. `initialize()` returns false; callers degrade
  rather than aborting the whole tab.

## Active-state visuals

- **Thick bright borders mark "active"**, not shadows or glow effects. The hardware doesn't render box shadows / blur effectively, so reach for a border-width + border-color change instead.
- Effects dropdown convention: cyan border when an animation is active; dimmed when colour/white modes are active.

## Event handlers

- **Never block.** No HTTP, no I2C, no long ops inline in an LVGL event.
- Queue work to the main loop and return.
