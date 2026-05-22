# Theming & Constants

## Single source of truth: `include/modular-ui.h`

**All colours, spacing, and sizing constants live in `include/modular-ui.h`.** Use the `UI_COLOR_*` and `UI_*` macros — never inline hex.

```cpp
// ✅ Correct
lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_PRIMARY), 0);

// ❌ Wrong — magic hex
lv_obj_set_style_text_color(label, lv_color_hex(0x00D9FF), 0);
```

## Categories defined

- **Primary / accent palette** — `UI_COLOR_PRIMARY`, `UI_COLOR_PRIMARY_DARK`, `UI_COLOR_PRIMARY_DARKER`, `UI_COLOR_PRIMARY_DIM`, `UI_COLOR_PRIMARY_GLOW`, `UI_COLOR_ACCENT`, `UI_COLOR_ACCENT_DARK`.
- **Backgrounds & surfaces** — `UI_COLOR_BACKGROUND`, `UI_COLOR_SURFACE`, `UI_COLOR_SURFACE_LIGHT`, `UI_COLOR_SURFACE_DARK`.
- **Borders** — `UI_COLOR_BORDER`, `UI_COLOR_BORDER_LIGHT`, `UI_COLOR_BORDER_GLOW`.
- **Text** — `UI_COLOR_TEXT`, `UI_COLOR_TEXT_MUTED`, `UI_COLOR_TEXT_DIM`.
- **VU gradient** — `UI_COLOR_VU_GREEN`, `UI_COLOR_VU_YELLOW`, `UI_COLOR_VU_ORANGE`, `UI_COLOR_VU_RED`.
- **Slider track** — `UI_COLOR_TRACK`, `UI_COLOR_TRACK_LIGHT`.
- **Layout** — spacing/sizing constants in the same header.

## Library overrides

`modular-ui.h` does `#undef UI_COLOR_*` before the project's `#define`s. This is intentional — `ESP32WifiSetup` ships its own defaults and we want the project's palette to win. Don't remove the `#undef` block.

## Adding a new colour

1. Add the constant to `modular-ui.h` in the matching category.
2. Use it via `lv_color_hex(UI_COLOR_X)` — never via a literal.
3. If the colour is web-only, add it as a CSS variable on `:root` in the web UI rather than threading it through C++.
