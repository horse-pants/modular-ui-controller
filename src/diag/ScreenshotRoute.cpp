#include "diag/ScreenshotRoute.h"

#include "diag/PngStream.h"
#include "diag/ScreenMirror.h"

#include <Logger.h>
#include <memory>

namespace {

// Owns everything one screenshot needs, so the mirror is released no matter how
// the response ends. AsyncWebServer destroys the response — and with it the
// lambda holding this — on completion AND on a client disconnect, so the thaw in
// the destructor covers the aborted-download case too. Leaving the mirror frozen
// would silently stop the screenshot endpoint working ever again.
struct ScreenshotJob {
    PngStream png;
    ~ScreenshotJob() { ScreenMirror::thaw(); }
};

}  // namespace

void ScreenshotRoute::registerRoutes(AsyncWebServer* server) {
    if (!server) return;

    server->on("/screenshot.png", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!ScreenMirror::isReady()) {
            request->send(503, "text/plain",
                          "Screen mirror unavailable (PSRAM allocation failed at boot)");
            return;
        }
        // One at a time: the mirror is a single frozen frame, and a second reader
        // would thaw it out from under the first when its job object died.
        if (ScreenMirror::isFrozen()) {
            request->send(409, "text/plain", "A screenshot is already being served");
            return;
        }

        // Hold the mirror still so the PNG is one coherent frame rather than one
        // torn across two refreshes. The live screen keeps updating regardless.
        ScreenMirror::freeze();

        auto job = std::make_shared<ScreenshotJob>();
        if (!job->png.begin()) {
            // job dies here and thaws on the way out.
            request->send(503, "text/plain", "Out of PSRAM for the PNG encoder");
            return;
        }

        AsyncWebServerResponse* response = request->beginChunkedResponse(
            "image/png",
            [job](uint8_t* buffer, size_t maxLen, size_t /*index*/) -> size_t {
                const size_t n = job->png.read(buffer, maxLen);
                if (n == 0 && job->png.failed()) {
                    Logger.warning("Screenshot: PNG encode failed mid-stream");
                }
                return n;
            });

        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    Logger.debug("ScreenshotRoute: /screenshot.png registered");
}
