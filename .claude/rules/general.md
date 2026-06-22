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

Group files by **subsystem** into matching subfolders under both `include/` and
`src/`. The two trees mirror each other, so a header at `include/<area>/Foo.h` has its
implementation at `src/<area>/Foo.cpp`.

```
include/
  ui/          # LVGL component classes (UIManager, ColourWheel, VuGraph, …) + ui.h
  led/         # LED driver, manager, helpers, animation engine + catalog
    animations/  # IAnimation, RenderContext, one <Name>Animation.h per animation
  audio/       # analyzer, AudioBus/AudioFrame, audio task, Filter
  modular-ui.h, lv_conf.h, UiCommand.h   # cross-cutting config / shared POD (root)
  WebUIManager.h, WifiBootManager.h      # single-file subsystems (root)
src/
  ui/  led/  led/animations/  audio/     # mirror include/
  main.cpp, WebUIManager.cpp, WifiBootManager.cpp   # root
```

Rules:
- **Mirror the trees.** A new component goes in the same `<area>/` under both `include/`
  and `src/`.
- **Include with the area-qualified path** from outside the folder:
  `#include "led/LEDManager.h"`, `#include "ui/VuGraph.h"`. Same-folder siblings may use a
  bare name (`#include "IAnimation.h"` from within `led/animations/`).
- **Root-level files** (`modular-ui.h`, `lv_conf.h`, `UiCommand.h`, the single-file
  `WebUIManager`/`WifiBootManager` subsystems, `main.cpp`) are included bare.
- A subsystem with many related files (like `led/animations/`, 15+ files) gets its own
  nested subfolder — that's the whole point: no flat dumping-ground directories.

## Naming

- Classes / methods: `PascalCase`
- Variables: `camelCase`
- Private members: `_prefixedCamelCase`
- Constants: `SCREAMING_SNAKE_CASE` (e.g. `UI_COLOR_PRIMARY`) or `PascalCase` in namespaces
- Files: `PascalCase.h`, `PascalCase.cpp`
- Globals: `g_camelCase` (e.g. `g_uiManager`)
