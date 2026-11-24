#include "edgedrop/format_detector.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>

namespace EdgeDrop {

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

DetectedFormat FormatDetector::detect(const std::filesystem::path& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return DetectedFormat::Unknown;
    }
    auto size = std::filesystem::file_size(filePath);
    if (size == 0) {
        return DetectedFormat::Unknown;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return DetectedFormat::Unknown;
    }

    uint8_t magic[32] = {0};
    file.read(reinterpret_cast<char*>(magic), sizeof(magic));
    std::streamsize bytesRead = file.gcount();

    // ── Magic Bytes Check ─────────────────────────────────────────────────────

    // JPEG: FF D8 FF
    if (bytesRead >= 3 && magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF) {
        return DetectedFormat::JPEG;
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (bytesRead >= 8 &&
        magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47 &&
        magic[4] == 0x0D && magic[5] == 0x0A && magic[6] == 0x1A && magic[7] == 0x0A) {
        return DetectedFormat::PNG;
    }

    // WebP: RIFF .... WEBP
    if (bytesRead >= 12 &&
        magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F' &&
        magic[8] == 'W' && magic[9] == 'E' && magic[10] == 'B' && magic[11] == 'P') {
        return DetectedFormat::WebP;
    }

    // PDF: %PDF (25 50 44 46)
    if (bytesRead >= 4 &&
        magic[0] == 0x25 && magic[1] == 0x50 && magic[2] == 0x44 && magic[3] == 0x46) {
        return DetectedFormat::PDF;
    }

    // GIF: GIF87a or GIF89a
    if (bytesRead >= 6 && magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F' &&
        magic[3] == '8' && (magic[4] == '7' || magic[4] == '9') && magic[5] == 'a') {
        return DetectedFormat::GIF;
    }

    // BMP: BM (42 4D)
    if (bytesRead >= 2 && magic[0] == 0x42 && magic[1] == 0x4D) {
        return DetectedFormat::BMP;
    }

    // TIFF: II*. or MM.*
    if (bytesRead >= 4 &&
        ((magic[0] == 0x49 && magic[1] == 0x49 && magic[2] == 0x2A && magic[3] == 0x00) ||
         (magic[0] == 0x4D && magic[1] == 0x4D && magic[2] == 0x00 && magic[3] == 0x2A))) {
        return DetectedFormat::TIFF;
    }

    // HEIC / HEIF / MP4 / MOV / M4A: check ftyp box (bytes 4..7 == "ftyp")
    if (bytesRead >= 12 && magic[4] == 'f' && magic[5] == 't' && magic[6] == 'y' && magic[7] == 'p') {
        std::string brand(reinterpret_cast<char*>(magic + 8), 4);
        if (brand == "heic" || brand == "heix" || brand == "mif1" || brand == "msf1" || brand == "hevc") {
            return DetectedFormat::HEIC;
        }
        if (brand == "qt  ") {
            return DetectedFormat::MOV;
        }
        if (brand == "M4A " || brand == "M4B " || brand == "m4a " || brand == "m4b ") {
            return DetectedFormat::M4A;
        }
        std::string ext = toLower(filePath.extension().string());
        if (ext == ".m4a") return DetectedFormat::M4A;
        return DetectedFormat::MP4;
    }

    // AAC: ADTS frame sync (FF F1, FF F9, or FF F8)
    if (bytesRead >= 2 && magic[0] == 0xFF && (magic[1] & 0xF6) == 0xF0) {
        return DetectedFormat::AAC;
    }

    // MKV / WEBM (EBML ID: 1A 45 DF A3)
    if (bytesRead >= 4 &&
        magic[0] == 0x1A && magic[1] == 0x45 && magic[2] == 0xDF && magic[3] == 0xA3) {
        std::string ext = toLower(filePath.extension().string());
        if (ext == ".webm") return DetectedFormat::WEBM;
        return DetectedFormat::MKV;
    }

    // OGG: OggS
    if (bytesRead >= 4 &&
        magic[0] == 'O' && magic[1] == 'g' && magic[2] == 'g' && magic[3] == 'S') {
        return DetectedFormat::OGG;
    }

    // WAV: RIFF .... WAVE
    if (bytesRead >= 12 &&
        magic[0] == 'R' && magic[1] == 'I' && magic[2] == 'F' && magic[3] == 'F' &&
        magic[8] == 'W' && magic[9] == 'A' && magic[10] == 'V' && magic[11] == 'E') {
        return DetectedFormat::WAV;
    }

    // MP3: ID3 or sync frame
    if ((bytesRead >= 3 && magic[0] == 'I' && magic[1] == 'D' && magic[2] == '3') ||
        (bytesRead >= 2 && magic[0] == 0xFF && (magic[1] == 0xFB || magic[1] == 0xF3 || magic[1] == 0xF2))) {
        return DetectedFormat::MP3;
    }

    // ZIP (50 4B 03 04) -> DOCX, PPTX, XLSX check
    if (bytesRead >= 4 && magic[0] == 0x50 && magic[1] == 0x4B && magic[2] == 0x03 && magic[3] == 0x04) {
        // Inspect up to 4KB of the ZIP header to check internal folder paths
        file.seekg(0, std::ios::beg);
        char zipBuf[4096];
        file.read(zipBuf, sizeof(zipBuf));
        std::streamsize zRead = file.gcount();
        std::string content(zipBuf, static_cast<size_t>(zRead));

        if (content.find("word/") != std::string::npos) return DetectedFormat::DOCX;
        if (content.find("ppt/") != std::string::npos)  return DetectedFormat::PPTX;
        if (content.find("xl/") != std::string::npos)   return DetectedFormat::XLSX;

        // Fall back to file extension
        std::string ext = toLower(filePath.extension().string());
        if (ext == ".docx") return DetectedFormat::DOCX;
        if (ext == ".pptx") return DetectedFormat::PPTX;
        if (ext == ".xlsx") return DetectedFormat::XLSX;
    }

    // ── Extension Fallback ────────────────────────────────────────────────────
    std::string ext = toLower(filePath.extension().string());
    if (ext == ".jpg" || ext == ".jpeg") return DetectedFormat::JPEG;
    if (ext == ".png")                   return DetectedFormat::PNG;
    if (ext == ".webp")                  return DetectedFormat::WebP;
    if (ext == ".heic" || ext == ".heif")return DetectedFormat::HEIC;
    if (ext == ".tiff" || ext == ".tif") return DetectedFormat::TIFF;
    if (ext == ".bmp")                   return DetectedFormat::BMP;
    if (ext == ".pdf")                   return DetectedFormat::PDF;
    if (ext == ".docx")                  return DetectedFormat::DOCX;
    if (ext == ".pptx")                  return DetectedFormat::PPTX;
    if (ext == ".xlsx")                  return DetectedFormat::XLSX;
    if (ext == ".mp4")                   return DetectedFormat::MP4;
    if (ext == ".mov")                   return DetectedFormat::MOV;
    if (ext == ".mkv")                   return DetectedFormat::MKV;
    if (ext == ".webm")                  return DetectedFormat::WEBM;
    if (ext == ".mp3")                   return DetectedFormat::MP3;
    if (ext == ".m4a")                   return DetectedFormat::M4A;
    if (ext == ".aac")                   return DetectedFormat::AAC;
    if (ext == ".ogg")                   return DetectedFormat::OGG;
    if (ext == ".wav")                   return DetectedFormat::WAV;
    if (ext == ".gif")                   return DetectedFormat::GIF;

    return DetectedFormat::Unknown;
}

std::string FormatDetector::formatName(DetectedFormat fmt) {
    switch (fmt) {
        case DetectedFormat::JPEG: return "JPG";
        case DetectedFormat::PNG:  return "PNG";
        case DetectedFormat::WebP: return "WebP";
        case DetectedFormat::HEIC: return "HEIC";
        case DetectedFormat::TIFF: return "TIFF";
        case DetectedFormat::BMP:  return "BMP";
        case DetectedFormat::PDF:  return "PDF";
        case DetectedFormat::DOCX: return "DOCX";
        case DetectedFormat::PPTX: return "PPTX";
        case DetectedFormat::XLSX: return "XLSX";
        case DetectedFormat::MP4:  return "MP4";
        case DetectedFormat::MOV:  return "MOV";
        case DetectedFormat::MKV:  return "MKV";
        case DetectedFormat::WEBM: return "WEBM";
        case DetectedFormat::MP3:  return "MP3";
        case DetectedFormat::M4A:  return "M4A";
        case DetectedFormat::AAC:  return "AAC";
        case DetectedFormat::OGG:  return "OGG";
        case DetectedFormat::WAV:  return "WAV";
        case DetectedFormat::GIF:  return "GIF";
        default:                   return "FILE";
    }
}

std::string FormatDetector::outputExtension(DetectedFormat fmt) {
    switch (fmt) {
        case DetectedFormat::JPEG: return ".jpg";
        case DetectedFormat::PNG:  return ".png";
        case DetectedFormat::WebP: return ".webp";
        case DetectedFormat::HEIC: return ".heic";
        case DetectedFormat::TIFF: return ".tiff";
        case DetectedFormat::BMP:  return ".bmp";
        case DetectedFormat::PDF:  return ".pdf";
        case DetectedFormat::DOCX: return ".docx";
        case DetectedFormat::PPTX: return ".pptx";
        case DetectedFormat::XLSX: return ".xlsx";
        case DetectedFormat::MP4:  return ".mp4";
        case DetectedFormat::MOV:  return ".mov";
        case DetectedFormat::MKV:  return ".mkv";
        case DetectedFormat::WEBM: return ".webm";
        case DetectedFormat::MP3:  return ".mp3";
        case DetectedFormat::M4A:  return ".m4a";
        case DetectedFormat::AAC:  return ".aac";
        case DetectedFormat::OGG:  return ".ogg";
        case DetectedFormat::WAV:  return ".wav";
        case DetectedFormat::GIF:  return ".gif";
        default:                   return "";
    }
}

std::string FormatDetector::formatLabel(DetectedFormat fmt, const std::filesystem::path& filePath) {
    std::string name = formatName(fmt);
    if (!std::filesystem::exists(filePath)) {
        return name;
    }
    int64_t bytes = static_cast<int64_t>(std::filesystem::file_size(filePath));
    return name + " · " + JobResult::formatBytes(bytes);
}

bool FormatDetector::isSupported(DetectedFormat fmt) {
    return fmt != DetectedFormat::Unknown;
}

} // namespace EdgeDrop
