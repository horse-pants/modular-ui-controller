# General Code Quality Rules

## File size

- **Hard limit: 500 lines per file**
- **Target: 200–400 lines** (ideal range)
- If approaching 400 lines, proactively split
- Count implementation lines, not comments/whitespace

## Single Responsibility Principle

- One class = one purpose. If you can describe a class with "and", split it.
- Extract when a method group serves a distinct sub-purpose.
- **God objects are forbidden.** UIManager already coordinates many widgets — keep each LVGL component (ColourWheel, VuGraph, EffectsList, WhiteButton, VuButton, BrightnessSlider) as its own class with a narrow API.

## DRY — Don't Repeat Yourself

- **If code appears twice, extract it.** Never copy-paste with minor modifications.
- Common extractions in this project:
  - LVGL widget creation patterns → a helper namespace or base class
  - Web route handlers → a shared response/error helper
  - Style application → state-based styles defined once

## Encapsulation

- **Private by default** — only expose what's necessary
- Use `protected` for base-class hooks
- Prefer composition over inheritance
- Small focused interfaces, not monolithic ones

## File organization

```
include/
  Feature.h           # Main class declaration
  FeatureHelper.h     # Extracted helper declarations (if any)
src/
  Feature.cpp         # Main implementation
  ui/Feature.cpp      # UI components live under src/ui/
```

Headers live in a flat `include/` (no nested folders); implementations either at `src/` top-level or under `src/ui/` for LVGL components.

## Naming

- Classes / methods: `PascalCase`
- Variables: `camelCase`
- Private members: `_prefixedCamelCase`
- Constants: `SCREAMING_SNAKE_CASE` (e.g. `UI_COLOR_PRIMARY`) or `PascalCase` in namespaces
- Files: `PascalCase.h`, `PascalCase.cpp`
- Globals: `g_camelCase` (e.g. `g_uiManager`)
