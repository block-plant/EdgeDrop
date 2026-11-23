#include "JfifDpiPatcher.hpp"
#include <fstream>
#include <cstring>
#include <cmath>

namespace EdgeDrop {

// Precomputed CRC32 table for PNG
static uint32_t crcTable[256];
static bool crcTableComputed = false;

static void makeCrcTable() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) {
            if (c & 1) {
                c = 0xedb88320L ^ (c >> 1);
            } else {
                c = c >> 1;
            }
        }
        crcTable[n] = c;
    }
    crcTableComputed = true;
}

uint32_t JfifDpiPatcher::calculateCrc32(const uint8_t* data, size_t length) {
    if (!crcTableComputed) {
        makeCrcTable();
    }
    uint32_t c = 0xffffffffL;
    for (size_t n = 0; n < length; n++) {
        c = crcTable[(c ^ data[n]) & 0xff] ^ (c >> 8);
    }
    return c ^ 0xffffffffL;
}

bool JfifDpiPatcher::patchJpegDpi(std::vector<uint8_t>& jpegData, int targetDpi) {
    if (jpegData.size() < 4 || targetDpi <= 0) return false;

    // Check SOI marker 0xFF 0xD8
    if (jpegData[0] != 0xFF || jpegData[1] != 0xD8) {
        return false;
    }

    uint16_t dpiVal = static_cast<uint16_t>(targetDpi);
    uint8_t xHi = static_cast<uint8_t>((dpiVal >> 8) & 0xFF);
    uint8_t xLo = static_cast<uint8_t>(dpiVal & 0xFF);

    // Look for existing APP0 JFIF marker at offset 2
    if (jpegData.size() >= 20 && jpegData[2] == 0xFF && jpegData[3] == 0xE0) {
        uint16_t app0Len = (static_cast<uint16_t>(jpegData[4]) << 8) | jpegData[5];
        if (app0Len >= 16 &&
            jpegData[6] == 'J' && jpegData[7] == 'F' && jpegData[8] == 'I' &&
            jpegData[9] == 'F' && jpegData[10] == 0x00) {
            // Overwrite existing APP0 JFIF density
            jpegData[13] = 1; // Units: 1 = DPI
            jpegData[14] = xHi;
            jpegData[15] = xLo;
            jpegData[16] = xHi;
            jpegData[17] = xLo;
            return true;
        }
    }

    // Build standard 18-byte JFIF APP0 block:
    // FF E0 (marker) + 00 10 (length = 16) + 'JFIF\0' (5) + 01 02 (version) + 01 (DPI) + xHi xLo + yHi yLo + 00 00 (no thumbnail)
    std::vector<uint8_t> jfifApp0 = {
        0xFF, 0xE0,
        0x00, 0x10,
        'J', 'F', 'I', 'F', 0x00,
        0x01, 0x02,
        0x01,
        xHi, xLo,
        xHi, xLo,
        0x00, 0x00
    };

    // Insert right after SOI (offset 2)
    jpegData.insert(jpegData.begin() + 2, jfifApp0.begin(), jfifApp0.end());
    return true;
}

std::pair<int, int> JfifDpiPatcher::readJpegDpi(const std::vector<uint8_t>& jpegData) {
    if (jpegData.size() < 20) return {-1, -1};
    if (jpegData[0] != 0xFF || jpegData[1] != 0xD8) return {-1, -1};

    size_t offset = 2;
    while (offset + 4 <= jpegData.size()) {
        if (jpegData[offset] != 0xFF) break;
        uint8_t marker = jpegData[offset + 1];
        if (marker == 0xD9 || marker == 0xDA) break; // SOS or EOI

        uint16_t len = (static_cast<uint16_t>(jpegData[offset + 2]) << 8) | jpegData[offset + 3];
        if (len < 2 || offset + 2 + len > jpegData.size()) break;

        if (marker == 0xE0 && len >= 16) {
            if (jpegData[offset + 4] == 'J' && jpegData[offset + 5] == 'F' &&
                jpegData[offset + 6] == 'I' && jpegData[offset + 7] == 'F' &&
                jpegData[offset + 8] == 0x00) {
                uint8_t units = jpegData[offset + 11];
                int xDensity = (jpegData[offset + 12] << 8) | jpegData[offset + 13];
                int yDensity = (jpegData[offset + 14] << 8) | jpegData[offset + 15];

                if (units == 1) { // DPI
                    return {xDensity, yDensity};
                } else if (units == 2) { // DPCM -> convert to DPI
                    int xDpi = static_cast<int>(std::round(xDensity * 2.54));
                    int yDpi = static_cast<int>(std::round(yDensity * 2.54));
                    return {xDpi, yDpi};
                } else {
                    return {xDensity, yDensity};
                }
            }
        }
        offset += 2 + len;
    }
    return {-1, -1};
}

bool JfifDpiPatcher::padJpegToMinBytes(std::vector<uint8_t>& jpegData, size_t targetMinBytes) {
    if (jpegData.size() >= targetMinBytes) return true;
    if (jpegData.size() < 4) return false;

    // Check EOI (0xFF 0xD9) at the end
    size_t insertPos = jpegData.size();
    if (jpegData[jpegData.size() - 2] == 0xFF && jpegData[jpegData.size() - 1] == 0xD9) {
        insertPos = jpegData.size() - 2;
    }

    size_t needed = targetMinBytes - jpegData.size();
    // COM marker requires 4 bytes header: FF FE + uint16 len
    if (needed < 5) needed = 5;

    // Maximum comment length per JPEG COM marker is 65535 (including 2 length bytes)
    while (jpegData.size() < targetMinBytes) {
        size_t currentDeficit = targetMinBytes - jpegData.size();
        size_t chunkPayload = std::min(currentDeficit, static_cast<size_t>(65530));
        if (chunkPayload < 1) chunkPayload = 1;

        uint16_t markerLen = static_cast<uint16_t>(chunkPayload + 2);
        std::vector<uint8_t> comBlock;
        comBlock.reserve(chunkPayload + 4);
        comBlock.push_back(0xFF);
        comBlock.push_back(0xFE);
        comBlock.push_back(static_cast<uint8_t>((markerLen >> 8) & 0xFF));
        comBlock.push_back(static_cast<uint8_t>(markerLen & 0xFF));
        comBlock.resize(chunkPayload + 4, 0x20); // space padding

        jpegData.insert(jpegData.begin() + insertPos, comBlock.begin(), comBlock.end());
        insertPos += comBlock.size();
    }
    return true;
}

bool JfifDpiPatcher::patchPngDpi(std::vector<uint8_t>& pngData, int targetDpi) {
    // PNG signature: 8 bytes 89 50 4E 47 0D 0A 1A 0A
    static const uint8_t pngSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (pngData.size() < 8 || std::memcmp(pngData.data(), pngSig, 8) != 0 || targetDpi <= 0) {
        return false;
    }

    // Convert DPI to pixels per meter
    uint32_t ppm = static_cast<uint32_t>(std::round(static_cast<double>(targetDpi) / 0.0254));

    // Construct pHYs chunk
    // Length: 9
    // Type: "pHYs"
    // Data: 4 bytes xPpm, 4 bytes yPpm, 1 byte unit (1 = meter)
    std::vector<uint8_t> physChunk(21);
    physChunk[0] = 0x00; physChunk[1] = 0x00; physChunk[2] = 0x00; physChunk[3] = 0x09; // length = 9
    physChunk[4] = 'p'; physChunk[5] = 'H'; physChunk[6] = 'Y'; physChunk[7] = 's';
    physChunk[8] = (ppm >> 24) & 0xFF;
    physChunk[9] = (ppm >> 16) & 0xFF;
    physChunk[10] = (ppm >> 8) & 0xFF;
    physChunk[11] = ppm & 0xFF;
    physChunk[12] = physChunk[8];
    physChunk[13] = physChunk[9];
    physChunk[14] = physChunk[10];
    physChunk[15] = physChunk[11];
    physChunk[16] = 1; // meter unit

    uint32_t crc = calculateCrc32(physChunk.data() + 4, 13);
    physChunk[17] = (crc >> 24) & 0xFF;
    physChunk[18] = (crc >> 16) & 0xFF;
    physChunk[19] = (crc >> 8) & 0xFF;
    physChunk[20] = crc & 0xFF;

    // Scan chunks: remove existing pHYs if present, then insert after IHDR
    size_t offset = 8;
    size_t insertOffset = 8;

    while (offset + 12 <= pngData.size()) {
        uint32_t chunkLen = (static_cast<uint32_t>(pngData[offset]) << 24) |
                            (static_cast<uint32_t>(pngData[offset + 1]) << 16) |
                            (static_cast<uint32_t>(pngData[offset + 2]) << 8) |
                            static_cast<uint32_t>(pngData[offset + 3]);
        char chunkType[5] = {
            static_cast<char>(pngData[offset + 4]),
            static_cast<char>(pngData[offset + 5]),
            static_cast<char>(pngData[offset + 6]),
            static_cast<char>(pngData[offset + 7]),
            0
        };

        size_t totalChunkSize = 4 + 4 + chunkLen + 4;
        if (offset + totalChunkSize > pngData.size()) break;

        if (std::strcmp(chunkType, "IHDR") == 0) {
            insertOffset = offset + totalChunkSize;
        } else if (std::strcmp(chunkType, "pHYs") == 0) {
            // Remove existing pHYs
            pngData.erase(pngData.begin() + offset, pngData.begin() + offset + totalChunkSize);
            continue;
        }

        offset += totalChunkSize;
    }

    // Insert pHYs right after IHDR
    pngData.insert(pngData.begin() + insertOffset, physChunk.begin(), physChunk.end());
    return true;
}

std::pair<int, int> JfifDpiPatcher::readPngDpi(const std::vector<uint8_t>& pngData) {
    static const uint8_t pngSig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (pngData.size() < 8 || std::memcmp(pngData.data(), pngSig, 8) != 0) {
        return {-1, -1};
    }

    size_t offset = 8;
    while (offset + 12 <= pngData.size()) {
        uint32_t chunkLen = (static_cast<uint32_t>(pngData[offset]) << 24) |
                            (static_cast<uint32_t>(pngData[offset + 1]) << 16) |
                            (static_cast<uint32_t>(pngData[offset + 2]) << 8) |
                            static_cast<uint32_t>(pngData[offset + 3]);
        char chunkType[5] = {
            static_cast<char>(pngData[offset + 4]),
            static_cast<char>(pngData[offset + 5]),
            static_cast<char>(pngData[offset + 6]),
            static_cast<char>(pngData[offset + 7]),
            0
        };

        size_t totalChunkSize = 4 + 4 + chunkLen + 4;
        if (offset + totalChunkSize > pngData.size()) break;

        if (std::strcmp(chunkType, "pHYs") == 0 && chunkLen >= 9) {
            uint32_t xPpm = (static_cast<uint32_t>(pngData[offset + 8]) << 24) |
                            (static_cast<uint32_t>(pngData[offset + 9]) << 16) |
                            (static_cast<uint32_t>(pngData[offset + 10]) << 8) |
                            static_cast<uint32_t>(pngData[offset + 11]);
            uint32_t yPpm = (static_cast<uint32_t>(pngData[offset + 12]) << 24) |
                            (static_cast<uint32_t>(pngData[offset + 13]) << 16) |
                            (static_cast<uint32_t>(pngData[offset + 14]) << 8) |
                            static_cast<uint32_t>(pngData[offset + 15]);
            uint8_t unit = pngData[offset + 16];

            if (unit == 1) { // meters
                int xDpi = static_cast<int>(std::round(xPpm * 0.0254));
                int yDpi = static_cast<int>(std::round(yPpm * 0.0254));
                return {xDpi, yDpi};
            }
        }

        offset += totalChunkSize;
    }
    return {-1, -1};
}

bool JfifDpiPatcher::patchJpegFileDpi(const std::string& filePath, int targetDpi) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (!patchJpegDpi(buffer, targetDpi)) return false;

    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

bool JfifDpiPatcher::patchPngFileDpi(const std::string& filePath, int targetDpi) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return false;
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (!patchPngDpi(buffer, targetDpi)) return false;

    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    return true;
}

} // namespace EdgeDrop
