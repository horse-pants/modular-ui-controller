#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief The built Svelte bundle, linked into the firmware image.
 *
 * `build_web.py` runs vite, gzips the output and writes the bytes into
 * `src/generated/WebAssets.cpp` as `const` arrays. Those land in flash `.rodata`,
 * which is memory-mapped on the ESP32-S3 — so the data costs no RAM and
 * `AsyncProgmemResponse` streams it straight out with no copy.
 *
 * This replaced serving from LittleFS. The point is one flash instead of two:
 * `pio run -t upload` now carries the UI with it, and there is no `uploadfs`
 * step to forget. It also answers faster, since there's no filesystem in the
 * path.
 *
 * The generated file is git-ignored and rebuilt whenever `web_src/` changes.
 */
namespace WebAssets {

struct Asset {
    const char*    path;         ///< request path, e.g. "/app.js"
    const uint8_t* data;         ///< points into flash .rodata
    uint32_t       len;
    const char*    contentType;
    bool           gzip;         ///< send with Content-Encoding: gzip
};

/// Number of assets in the bundle.
size_t count();

/// Asset by index, or nullptr when out of range. For registering routes.
const Asset* at(size_t index);

/// Exact-path lookup, or nullptr.
const Asset* find(const char* path);

}  // namespace WebAssets
