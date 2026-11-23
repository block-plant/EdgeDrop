#pragma once

#include <string>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <atomic>
#include <sstream>
#include <iomanip>

namespace EdgeDrop {

// ── Detected Format ──────────────────────────────────────────────────────────

enum class DetectedFormat {
    // Raster images
    JPEG, PNG, WebP, HEIC, TIFF, BMP,
    // Structural
    PDF,
    // Office OOXML (ZIP-based)
    DOCX, PPTX, XLSX,
    // Video
    MP4, MOV, MKV, WEBM,
    // Audio
    MP3, M4A, AAC, OGG, WAV,
    // Animated
    GIF,
    Unknown
};

// ── Legacy Compatibility Enums ────────────────────────────────────────────────

enum class FileCategory { Photo, Signature, Document };
enum class TargetFormat { JPG, PNG, WebP, PDF };

inline std::string categoryToString(FileCategory cat) {
    switch (cat) {
        case FileCategory::Photo:     return "Photo";
        case FileCategory::Signature: return "Signature";
        case FileCategory::Document:  return "Document";
    }
    return "Photo";
}

inline FileCategory stringToCategory(std::string_view str) {
    if (str == "Signature" || str == "signature") return FileCategory::Signature;
    if (str == "Document" || str == "document" || str == "PDF" || str == "pdf") return FileCategory::Document;
    return FileCategory::Photo;
}

inline std::string formatToString(TargetFormat fmt) {
    switch (fmt) {
        case TargetFormat::JPG:  return "JPG";
        case TargetFormat::PNG:  return "PNG";
        case TargetFormat::WebP: return "WebP";
        case TargetFormat::PDF:  return "PDF";
    }
    return "JPG";
}

inline TargetFormat stringToFormat(std::string_view str) {
    if (str == "PNG" || str == "png") return TargetFormat::PNG;
    if (str == "WebP" || str == "webp") return TargetFormat::WebP;
    if (str == "PDF" || str == "pdf") return TargetFormat::PDF;
    return TargetFormat::JPG;
}

// ── Callbacks & Tokens ───────────────────────────────────────────────────────

using ProgressCallback = std::function<void(double)>; // 0.0 – 1.0
using CancelToken      = std::atomic<bool>*;

// ── Job Types ────────────────────────────────────────────────────────────────

struct JobRequest {
    std::filesystem::path inputFilePath;
    std::filesystem::path outputDirectory;
    DetectedFormat detectedFormat = DetectedFormat::Unknown;
    int64_t targetBytes = 0;   // 0 = no ceiling (default-quality pass-through)

    // Legacy fields for backward compatibility with presets / processors
    FileCategory category = FileCategory::Photo;
    TargetFormat targetFormat = TargetFormat::JPG;
    int targetWidth = 0;       // 0 = preserve original
    int targetHeight = 0;      // 0 = preserve original
    int maxLongEdge = 0;       // for document/PDF
    int dpi = 300;             // 150, 200, 300, etc.
    int minKb = 0;             // 0 = no floor
    int maxKb = 0;             // 0 = no ceiling
    int targetKb = 0;          // optimal target size
    bool whitenBackground = false;
    bool autoCrop = false;     // Face for photo, ink bbox for signature
    double faceRatio = 0.70;   // % of frame height occupied by face
    bool greyscale = false;
    std::string presetId;
    std::string presetName;
    std::string customFileName;
};

struct JobResult {
    bool success = false;
    bool hitFloor = false;         // true when targetBytes could not be reached
    std::string floorNote;         // honest explanation when hitFloor is true
    std::filesystem::path inputFilePath;
    std::filesystem::path outputFilePath;
    int64_t inputSizeBytes = 0;
    int64_t outputSizeBytes = 0;
    int outputWidth = 0;
    int outputHeight = 0;
    int outputDpi = 0;
    std::string outputFormat;
    std::string errorMessage;
    int64_t durationMs = 0;

    [[nodiscard]] double savingsPercentage() const {
        if (inputSizeBytes <= 0 || outputSizeBytes <= 0) return 0.0;
        return (1.0 - static_cast<double>(outputSizeBytes) / static_cast<double>(inputSizeBytes)) * 100.0;
    }

    [[nodiscard]] std::string formattedInputSize() const {
        return formatBytes(inputSizeBytes);
    }

    [[nodiscard]] std::string formattedOutputSize() const {
        return formatBytes(outputSizeBytes);
    }

    [[nodiscard]] std::string specSummary() const {
        if (outputWidth > 0 && outputHeight > 0 && outputDpi > 0) {
            std::ostringstream ss;
            ss << outputFormat << " · " << outputWidth << "×" << outputHeight << " px · " << outputDpi << " DPI";
            return ss.str();
        }
        return outputFormat;
    }

    static std::string formatBytes(int64_t bytes) {
        if (bytes <= 0) return "0 KB";
        double kb = static_cast<double>(bytes) / 1024.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        if (kb < 1024.0) {
            ss << kb << " KB";
            return ss.str();
        }
        double mb = kb / 1024.0;
        ss << std::setprecision(2) << mb << " MB";
        return ss.str();
    }
};

} // namespace EdgeDrop
