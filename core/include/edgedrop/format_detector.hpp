#pragma once

#include "types.hpp"
#include <filesystem>
#include <string>

namespace EdgeDrop {

class FormatDetector {
public:
    /// Detect format from magic bytes (first 32 bytes) + extension fallback.
    static DetectedFormat detect(const std::filesystem::path& filePath);

    /// Human-readable badge: e.g. "JPG · 3.1 MB", "PDF · 12 pages", "MP4 · 01:42"
    static std::string formatLabel(DetectedFormat fmt, const std::filesystem::path& filePath);

    /// Output file extension for detected format (same as input, e.g. ".jpg")
    static std::string outputExtension(DetectedFormat fmt);

    /// Short format name for UI badge: "JPG", "PDF", "DOCX", "MP4", etc.
    static std::string formatName(DetectedFormat fmt);

    /// Whether this format is handled by the system
    static bool isSupported(DetectedFormat fmt);
};

} // namespace EdgeDrop
