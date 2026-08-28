#include "diag/PngStream.h"

#include "diag/ScreenMirror.h"

#include <Logger.h>
#include <esp_heap_caps.h>
#include <miniz.h>      // esp_rom/include — tdefl_* and mz_crc32 live in ROM
#include <string.h>

namespace {

// Deflate effort, as a max-probe count in the low 12 bits of the tdefl flags.
// The default is 128; a flat dark UI is already trivially compressible, so a
// third of that gives essentially the same file for a fraction of the CPU — and
// this runs on the async web task, where CPU time is latency for everyone else.
constexpr int DEFLATE_PROBES = 32;

// PNG filter type 2 (Up): each byte is the difference from the byte above it.
// Large flat regions and vertical gradients collapse to runs of zero, which is
// most of this UI.
constexpr uint8_t FILTER_UP = 2;

void be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// [length][tag][data][crc32 of tag+data]. Returns the bytes written.
size_t writeChunk(uint8_t* dst, const char tag[4], const uint8_t* data, size_t len) {
    be32(dst, (uint32_t)len);
    memcpy(dst + 4, tag, 4);
    if (len && data) memcpy(dst + 8, data, len);
    const uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, dst + 4, 4 + len);
    be32(dst + 8 + len, crc);
    return 8 + len + 4;
}

}  // namespace

PngStream::~PngStream() {
    if (_buf)  { heap_caps_free(_buf);  _buf = nullptr; }
    if (_line) { heap_caps_free(_line); _line = nullptr; }
    if (_prev) { heap_caps_free(_prev); _prev = nullptr; }
    if (_comp) { heap_caps_free(_comp); _comp = nullptr; }
}

bool PngStream::begin() {
    if (!ScreenMirror::isReady()) return false;

    _w = ScreenMirror::width();
    _h = ScreenMirror::height();
    if (_w <= 0 || _h <= 0) return false;
    _rowBytes = (size_t)_w * 3;

    // 8 for the IDAT length + tag in front, 4 for the CRC behind, and slack so the
    // header and trailer chunks can be built in the same buffer.
    _buf  = (uint8_t*)heap_caps_malloc(STAGE_CAP + 64, MALLOC_CAP_SPIRAM);
    _comp = heap_caps_malloc(sizeof(tdefl_compressor), MALLOC_CAP_SPIRAM);
    // These two are touched once per byte of the image, so they earn their ~2 KB
    // of internal RAM — PSRAM would put the slow bus in the inner loop.
    _line = (uint8_t*)heap_caps_malloc(_rowBytes + 1, MALLOC_CAP_INTERNAL);
    _prev = (uint8_t*)heap_caps_malloc(_rowBytes, MALLOC_CAP_INTERNAL);

    if (!_buf || !_line || !_prev || !_comp) {
        Logger.warning("PngStream: out of PSRAM (largest free block %u KB)",
                       (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
        return false;
    }

    // Row 0 filters against an all-zero "previous row", which makes Up a no-op
    // there — exactly what the spec expects.
    memset(_prev, 0, _rowBytes);
    _line[0] = FILTER_UP;

    const int flags = DEFLATE_PROBES | TDEFL_WRITE_ZLIB_HEADER;
    if (tdefl_init((tdefl_compressor*)_comp, putBuf, this, flags) != TDEFL_STATUS_OKAY) {
        Logger.warning("PngStream: tdefl_init failed");
        return false;
    }
    return true;
}

int PngStream::putBuf(const void* buf, int len, void* user) {
    PngStream* self = static_cast<PngStream*>(user);
    if (!self || len <= 0) return MZ_TRUE;

    // The compressor can't be told to wait, so overflowing here is fatal to the
    // stream rather than something to retry. STAGE_CAP is sized so it can't
    // happen in practice; this is the guard, not the plan.
    if (self->_staged + (size_t)len > STAGE_CAP) {
        self->_failed = true;
        return MZ_FALSE;
    }
    memcpy(self->_buf + 8 + self->_staged, buf, (size_t)len);
    self->_staged += (size_t)len;
    return MZ_TRUE;
}

size_t PngStream::read(uint8_t* dst, size_t maxLen) {
    if (!dst || maxLen == 0 || _failed || !_buf) return 0;

    for (;;) {
        if (_sent < _ready) {
            const size_t n = (_ready - _sent) < maxLen ? (_ready - _sent) : maxLen;
            memcpy(dst, _buf + _sent, n);
            _sent += n;
            return n;
        }

        _ready = 0;
        _sent = 0;

        switch (_state) {
            case State::Header:  emitHeader();    break;
            case State::Rows:    pumpRows();      break;
            case State::Finish:  finishDeflate(); break;
            case State::Trailer: emitTrailer();   break;
            case State::Done:    return 0;
        }

        if (_failed) return 0;
    }
}

void PngStream::emitHeader() {
    static const uint8_t SIG[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

    uint8_t ihdr[13];
    be32(ihdr + 0, (uint32_t)_w);
    be32(ihdr + 4, (uint32_t)_h);
    ihdr[8]  = 8;   // 8 bits per channel
    ihdr[9]  = 2;   // colour type 2: truecolour RGB
    ihdr[10] = 0;   // deflate
    ihdr[11] = 0;   // adaptive filtering
    ihdr[12] = 0;   // no interlace

    memcpy(_buf, SIG, sizeof(SIG));
    _ready = sizeof(SIG) + writeChunk(_buf + sizeof(SIG), "IHDR", ihdr, sizeof(ihdr));
    _state = State::Rows;
}

void PngStream::pumpRows() {
    const int32_t end = (_y + ROWS_PER_BATCH < _h) ? (_y + ROWS_PER_BATCH) : _h;

    while (_y < end) {
        const uint16_t* src = ScreenMirror::row(_y);
        if (!src) { _failed = true; return; }

        uint8_t* line = _line + 1;   // past the filter byte
        for (int32_t x = 0; x < _w; ++x) {
            const uint16_t v = src[x];
            // 5/6/5 -> 8/8/8 by replicating the high bits into the low ones, so
            // full-scale stays full-scale (0x1F -> 0xFF, not 0xF8).
            uint8_t r = (uint8_t)((v >> 8) & 0xF8); r = (uint8_t)(r | (r >> 5));
            uint8_t g = (uint8_t)((v >> 3) & 0xFC); g = (uint8_t)(g | (g >> 6));
            uint8_t b = (uint8_t)((v << 3) & 0xF8); b = (uint8_t)(b | (b >> 5));

            const size_t i = (size_t)x * 3;
            line[i]     = (uint8_t)(r - _prev[i]);     _prev[i]     = r;
            line[i + 1] = (uint8_t)(g - _prev[i + 1]); _prev[i + 1] = g;
            line[i + 2] = (uint8_t)(b - _prev[i + 2]); _prev[i + 2] = b;
        }

        if (tdefl_compress_buffer((tdefl_compressor*)_comp, _line, _rowBytes + 1,
                                  TDEFL_NO_FLUSH) != TDEFL_STATUS_OKAY) {
            _failed = true;
            return;
        }
        ++_y;
    }

    // End the batch on a sync flush so this call always has bytes to return.
    // Without it deflate would keep buffering and the loop in read() would just
    // call back in, compressing the whole image in one go.
    if (tdefl_compress_buffer((tdefl_compressor*)_comp, nullptr, 0,
                              TDEFL_SYNC_FLUSH) != TDEFL_STATUS_OKAY) {
        _failed = true;
        return;
    }
    frameIdat();
    if (_y >= _h) _state = State::Finish;
}

void PngStream::finishDeflate() {
    if (tdefl_compress_buffer((tdefl_compressor*)_comp, nullptr, 0, TDEFL_FINISH) !=
        TDEFL_STATUS_DONE) {
        _failed = true;
        return;
    }
    frameIdat();               // whatever the flush produced; a no-op if nothing did
    _state = State::Trailer;
}

void PngStream::emitTrailer() {
    _ready = writeChunk(_buf, "IEND", nullptr, 0);
    _state = State::Done;
}

void PngStream::frameIdat() {
    if (_staged == 0) return;
    // Deflate output is already sitting at _buf + 8, so the chunk is framed by
    // filling in the header in front of it and the CRC behind it.
    be32(_buf, (uint32_t)_staged);
    memcpy(_buf + 4, "IDAT", 4);
    const uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, _buf + 4, 4 + _staged);
    be32(_buf + 8 + _staged, crc);

    _ready = 8 + _staged + 4;
    _sent = 0;
    _staged = 0;
}
