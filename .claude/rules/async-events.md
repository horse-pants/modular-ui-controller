# Async & Events Rules

## Universal rule

- **Never block in callbacks.** Set flags; process in the main loop.

This applies to:
- AsyncWebServer handlers
- WebSocket message callbacks
- LVGL event handlers
- Audio analyser callbacks

## AsyncWebServer / AsyncTCP

- **Registration order matters** — register specific routes BEFORE wildcards.
- **`server->on()` does prefix matching.** `/api/foo` matches `/api/foobar` — order specific routes before generic ones, or use an explicit URL parser inside a single wildcard handler.
- **`AsyncCallbackJsonWebHandler` catches ALL methods** — register GET before POST if both exist on the same path.
- Async restart pattern: set `g_restartRequested = true` from the handler and let `loop()` schedule the actual `ESP.restart()` after a delay. Never call `ESP.restart()` inline from a web callback.

## WebSocket cleanup

- `WebUIManager::update()` does WebSocket housekeeping. Don't iterate the client list from a callback; queue work and let the main loop drain it.
- `Logger` is attached to the WebSocket via `Logger.attachWebSocket(g_webUIManager->getWebSocket())` — log lines are pushed to connected clients. Heavy logging from a callback can backpressure the socket.
