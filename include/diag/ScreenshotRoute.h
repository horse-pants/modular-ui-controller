#pragma once

#include <ESPAsyncWebServer.h>

/**
 * @brief `GET /screenshot.png` — the current screen, as a PNG, for help docs.
 *
 * Serves straight from ScreenMirror, so it neither touches LVGL nor competes for
 * the panel bus. Encoding is streamed a batch of rows at a time (see PngStream),
 * which keeps every call back into the async task short.
 */
namespace ScreenshotRoute {

/// Register the route. Must be called before the static-file wildcard.
void registerRoutes(AsyncWebServer* server);

}  // namespace ScreenshotRoute
