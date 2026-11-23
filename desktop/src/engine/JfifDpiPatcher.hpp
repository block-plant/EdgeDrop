#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <utility>

namespace EdgeDrop {

class JfifDpiPatcher {
public:
    // --- JPEG JFIF Helpers ---
    // Injects or overwrites JFIF APP0 18-byte marker with exact target DPI (units = 1 = DPI).
    static bool patchJpegDpi(std::vector<uint8_t>& jpegData, int targetDpi);
    static bool patchJpegFileDpi(const std::string& filePath, int targetDpi);

    // Reads (xDpi, yDpi) from JFIF APP0. Returns {-1, -1} if no JFIF tag found.
    static std::pair<int, int> readJpegDpi(const std::vector<uint8_t>& jpegData);

    // Appends a harmless JFIF COM (0xFFFE) comment marker with padding to achieve target minBytes.
    static bool padJpegToMinBytes(std::vector<uint8_t>& jpegData, size_t targetMinBytes);

    // --- PNG pHYs Helpers ---
    // Injects or replaces pHYs chunk with exact pixels per meter (round(dpi / 0.0254)).
    static bool patchPngDpi(std::vector<uint8_t>& pngData, int targetDpi);
    static bool patchPngFileDpi(const std::string& filePath, int targetDpi);

    // Reads (xDpi, yDpi) from PNG pHYs chunk. Returns {-1, -1} if no pHYs chunk found.
    static std::pair<int, int> readPngDpi(const std::vector<uint8_t>& pngData);

    // Calculate IEEE 802.3 CRC32 (used by PNG)
    static uint32_t calculateCrc32(const uint8_t* data, size_t length);
};

} // namespace EdgeDrop
