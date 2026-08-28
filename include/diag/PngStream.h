#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Streams the ScreenMirror out as a real, deflate-compressed PNG.
 *
 * The compressor is the ESP32-S3 ROM's miniz — `tdefl_compress` and `mz_crc32`
 * are exported from ROM (see `esp32s3.rom.ld`), so a proper PNG costs no flash
 * and pulls in no library. The S3 has no hardware JPEG encoder and the bundled
 * `esp_jpeg` is decode-only, so this is the one good option that needs nothing
 * new in `platformio.ini`.
 *
 * Pull-driven by design: `read()` hands back the next slice of the file, which is
 * exactly what AsyncWebServer's chunked response asks for. Each call compresses
 * only enough rows to fill the caller's buffer, so the async task is never blocked
 * for long and the whole image is never held in RAM — rows are read straight out
 * of the mirror, converted RGB565 → RGB888, Up-filtered and fed to the compressor
 * one at a time.
 *
 * Output is framed as a run of IDAT chunks (PNG concatenates them on read),
 * because a single chunk would need its compressed length known up front.
 */
class PngStream {
public:
    PngStream() = default;
    ~PngStream();

    PngStream(const PngStream&) = delete;
    PngStream& operator=(const PngStream&) = delete;

    /// Allocate the compressor + buffers in PSRAM. False if the mirror isn't
    /// available or PSRAM is exhausted.
    bool begin();

    /// Fill up to @p maxLen bytes of the PNG. Returns 0 once the file is complete
    /// (or on failure — check failed()).
    size_t read(uint8_t* dst, size_t maxLen);

    bool failed() const { return _failed; }

private:
    enum class State : uint8_t { Header, Rows, Finish, Trailer, Done };

    // Deflate bytes staged between reads, framed into an IDAT on the way out.
    // Sized past the largest single block the ROM compressor can hand to the put
    // callback, because the callback has no way to refuse bytes.
    static constexpr size_t STAGE_CAP = 128u * 1024u;

    // Rows compressed per read(). This is the latency bound: deflate emits only
    // when its internal buffers fill, and a flat UI compresses so well that
    // without a cap one call would swallow all 480 rows and sit on the async task
    // for hundreds of ms. Each batch ends in a sync flush so there is always
    // something to hand back.
    static constexpr int32_t ROWS_PER_BATCH = 40;

    static int putBuf(const void* buf, int len, void* user);

    void emitHeader();
    void pumpRows();
    void finishDeflate();
    void emitTrailer();
    void frameIdat();

    // The staging buffer doubles as the read buffer: deflate output lands at
    // `_buf + 8`, leaving room to write the IDAT length + tag in front of it and
    // the CRC behind, so a chunk is framed without a second copy.
    uint8_t* _buf = nullptr;
    uint8_t* _line = nullptr;   // filter byte + one filtered RGB888 row
    uint8_t* _prev = nullptr;   // previous row, unfiltered, for the Up filter
    void*    _comp = nullptr;   // tdefl_compressor — ~300 KB, hence PSRAM

    size_t _staged = 0;         // deflate bytes sitting at _buf + 8
    size_t _ready = 0;          // framed bytes at _buf + 0, ready to hand out
    size_t _sent = 0;           // how much of _ready has gone out
    size_t _rowBytes = 0;       // _w * 3

    int32_t _y = 0;
    int32_t _w = 0;
    int32_t _h = 0;

    State _state = State::Header;
    bool  _failed = false;
};
