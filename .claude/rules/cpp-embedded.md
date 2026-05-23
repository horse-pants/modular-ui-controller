# C++ / Embedded Rules

## Memory management

- Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) when ownership is non-trivial
- Raw pointers for non-owning references only
- No `new`/`delete` in app code unless wrapped (global managers in `main.cpp` are the legacy exception)
- Avoid dynamic allocation in hot paths (LVGL update loop, LED update loop, audio analyser tick)

## Type safety

- `const` correctness — mark everything possible as const
- Prefer `enum class` over plain `enum`
- Prefer fixed-width integer types (`uint8_t`, `int32_t`) for hardware-facing data

## RAII

- Constructors acquire resources; destructors release them
- **Always unregister callbacks in destructors.** Any `[this]` lambda registered with LVGL / WebSocket / AsyncWebServer must be removed in the destructor or the next event after teardown crashes.

## Strings

- `String` (Arduino) for general use across the codebase
- Pass by `const String&` to minimise copies
- Use `c_str()` only when needed for C APIs (LVGL labels, printf-family)

## Logging

- **Always `Logger`, never `Serial.println`** — `Logger` is attached to the WebSocket so log output streams to the web UI
- Levels: `Logger.debug()`, `Logger.info()`, `Logger.warning()`, `Logger.error()`
- `Logger.begin(200, true, true)` is called in `setup()` before anything else logs

## Boot ordering

`setup()` initialises in a specific order — UI screen first, then `WifiBootManager`, then (only if not in setup mode) the full LVGL UI + `LEDManager`, then `WebUIManager`. Don't touch LEDs or full-UI widgets before `WifiBootManager::initialize()` has decided whether we're in captive-portal mode.
