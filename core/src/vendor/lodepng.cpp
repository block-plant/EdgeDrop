#include "lodepng.h"
#include <zlib.h>
#include <cstring>
#include <algorithm>

namespace lodepng {

static void writeBe32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void appendChunk(std::vector<uint8_t>& out,
                        const char type[4],
                        const uint8_t* data,
                        size_t length) {
    writeBe32(out, static_cast<uint32_t>(length));
    size_t typeOffset = out.size();
    out.insert(out.end(), type, type + 4);
    if (data && length > 0) {
        out.insert(out.end(), data, data + length);
    }
    uint32_t c = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));
    c = static_cast<uint32_t>(crc32(c, out.data() + typeOffset, static_cast<uInt>(length + 4)));
    writeBe32(out, c);
}

static bool deflateCompress(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& out) {
    z_stream strm{};
    if (deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }

    out.resize(srcLen + 1024);
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    strm.avail_in = static_cast<uInt>(srcLen);
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());

    int ret = deflate(&strm, Z_FINISH);
    while (ret == Z_OK) {
        size_t oldSize = out.size();
        out.resize(oldSize * 2);
        strm.next_out = reinterpret_cast<Bytef*>(out.data() + oldSize);
        strm.avail_out = static_cast<uInt>(out.size() - oldSize);
        ret = deflate(&strm, Z_FINISH);
    }

    if (ret != Z_STREAM_END) {
        deflateEnd(&strm);
        return false;
    }

    out.resize(strm.total_out);
    deflateEnd(&strm);
    return true;
}

unsigned encode_indexed(std::vector<uint8_t>& out,
                        const uint8_t* indices,
                        unsigned width, unsigned height,
                        const std::vector<uint8_t>& palette_rgba) {
    if (!indices || width == 0 || height == 0 || palette_rgba.empty()) {
        return 1;
    }

    out.clear();
    // 1. PNG Signature
    const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    out.insert(out.end(), sig, sig + 8);

    // 2. IHDR Chunk (color_type 3 = indexed, bit_depth 8)
    std::vector<uint8_t> ihdr;
    writeBe32(ihdr, width);
    writeBe32(ihdr, height);
    ihdr.push_back(8); // 8-bit depth
    ihdr.push_back(3); // color_type = indexed palette
    ihdr.push_back(0); // compression method (deflate)
    ihdr.push_back(0); // filter method
    ihdr.push_back(0); // interlace method (none)
    appendChunk(out, "IHDR", ihdr.data(), ihdr.size());

    // 3. PLTE Chunk (RGB triples)
    size_t numColors = palette_rgba.size() / 4;
    std::vector<uint8_t> plte(numColors * 3);
    bool hasTransparency = false;
    std::vector<uint8_t> trns;
    trns.reserve(numColors);

    for (size_t i = 0; i < numColors; ++i) {
        plte[i * 3 + 0] = palette_rgba[i * 4 + 0]; // R
        plte[i * 3 + 1] = palette_rgba[i * 4 + 1]; // G
        plte[i * 3 + 2] = palette_rgba[i * 4 + 2]; // B
        uint8_t a = palette_rgba[i * 4 + 3];
        trns.push_back(a);
        if (a < 255) {
            hasTransparency = true;
        }
    }
    appendChunk(out, "PLTE", plte.data(), plte.size());

    // 4. tRNS Chunk (Alpha values for palette entries)
    if (hasTransparency) {
        // Strip trailing 255s from tRNS as per PNG spec
        while (!trns.empty() && trns.back() == 255) {
            trns.pop_back();
        }
        if (!trns.empty()) {
            appendChunk(out, "tRNS", trns.data(), trns.size());
        }
    }

    // 5. Raw Scanlines with Filter 0 (None)
    std::vector<uint8_t> rawScanlines((width + 1) * height);
    for (unsigned y = 0; y < height; ++y) {
        size_t rowOffset = y * (width + 1);
        rawScanlines[rowOffset] = 0; // Filter byte: 0 = None
        std::memcpy(&rawScanlines[rowOffset + 1], indices + (y * width), width);
    }

    // 6. IDAT Chunk
    std::vector<uint8_t> compressedIdat;
    if (!deflateCompress(rawScanlines.data(), rawScanlines.size(), compressedIdat)) {
        return 2;
    }
    appendChunk(out, "IDAT", compressedIdat.data(), compressedIdat.size());

    // 7. IEND Chunk
    appendChunk(out, "IEND", nullptr, 0);

    return 0;
}

unsigned encode_rgba(std::vector<uint8_t>& out,
                     const uint8_t* rgba,
                     unsigned width, unsigned height) {
    if (!rgba || width == 0 || height == 0) {
        return 1;
    }

    out.clear();
    const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    out.insert(out.end(), sig, sig + 8);

    std::vector<uint8_t> ihdr;
    writeBe32(ihdr, width);
    writeBe32(ihdr, height);
    ihdr.push_back(8); // 8-bit depth
    ihdr.push_back(6); // color_type 6 = RGBA
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    appendChunk(out, "IHDR", ihdr.data(), ihdr.size());

    // Raw scanlines (4 bytes per pixel + 1 filter byte)
    size_t rowStride = width * 4;
    std::vector<uint8_t> rawScanlines((rowStride + 1) * height);
    for (unsigned y = 0; y < height; ++y) {
        size_t rowOffset = y * (rowStride + 1);
        rawScanlines[rowOffset] = 0; // None
        std::memcpy(&rawScanlines[rowOffset + 1], rgba + (y * rowStride), rowStride);
    }

    std::vector<uint8_t> compressedIdat;
    if (!deflateCompress(rawScanlines.data(), rawScanlines.size(), compressedIdat)) {
        return 2;
    }
    appendChunk(out, "IDAT", compressedIdat.data(), compressedIdat.size());
    appendChunk(out, "IEND", nullptr, 0);

    return 0;
}

} // namespace lodepng
