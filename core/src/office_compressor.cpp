#include "edgedrop/office_compressor.hpp"
#include "edgedrop/image_compressor.hpp"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"

#include <opencv2/imgcodecs.hpp>
#include <zlib.h>

#include <fstream>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

namespace EdgeDrop {

bool OfficeCompressor::canHandle(DetectedFormat fmt) const {
    return fmt == DetectedFormat::DOCX
        || fmt == DetectedFormat::PPTX
        || fmt == DetectedFormat::XLSX;
}

// ── Minimal ZIP Structures ────────────────────────────────────────────────────

#pragma pack(push, 1)
struct ZipLocalHeader {
    uint32_t signature;      // 0x04034b50
    uint16_t version;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLen;
    uint16_t extraLen;
};

struct ZipCentralDir {
    uint32_t signature;      // 0x02014b50
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compression;
    uint16_t modTime;
    uint16_t modDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLen;
    uint16_t extraLen;
    uint16_t commentLen;
    uint16_t diskStart;
    uint16_t intAttr;
    uint32_t extAttr;
    uint32_t localHeaderOffset;
};

struct ZipEndRecord {
    uint32_t signature;      // 0x06054b50
    uint16_t diskNum;
    uint16_t centralDirDisk;
    uint16_t numEntriesOnDisk;
    uint16_t numEntries;
    uint32_t centralDirSize;
    uint32_t centralDirOffset;
    uint16_t commentLen;
};
#pragma pack(pop)

struct ZipEntry {
    std::string name;
    std::vector<uint8_t> data;
    uint16_t modTime = 0;
    uint16_t modDate = 0;
};

static bool zlibInflate(const uint8_t* src, size_t srcLen,
                        std::vector<uint8_t>& out, size_t uncompressedHint) {
    constexpr size_t kMaxSingleEntrySize = 150ULL * 1024 * 1024; // 150 MB
    if (uncompressedHint > kMaxSingleEntrySize) return false;

    out.resize(std::min(kMaxSingleEntrySize, std::max(uncompressedHint, srcLen * 3)));
    z_stream strm{};
    if (inflateInit2(&strm, -15) != Z_OK) return false;

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    strm.avail_in = static_cast<uInt>(srcLen);
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());

    int ret = inflate(&strm, Z_SYNC_FLUSH);
    while (ret == Z_OK && strm.avail_out == 0) {
        size_t old = out.size();
        if (old >= kMaxSingleEntrySize) {
            inflateEnd(&strm);
            return false;
        }
        size_t nextSize = std::min(kMaxSingleEntrySize, old * 2);
        out.resize(nextSize);
        strm.next_out = reinterpret_cast<Bytef*>(out.data() + old);
        strm.avail_out = static_cast<uInt>(nextSize - old);
        ret = inflate(&strm, Z_SYNC_FLUSH);
    }
    inflateEnd(&strm);
    if (ret != Z_STREAM_END && ret != Z_OK) return false;
    if (strm.total_out > kMaxSingleEntrySize) return false;
    out.resize(strm.total_out);
    return true;
}

static bool zlibDeflate(const uint8_t* src, size_t srcLen,
                        std::vector<uint8_t>& out) {
    out.resize(srcLen + 1024);
    z_stream strm{};
    if (deflateInit2(&strm, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return false;

    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    strm.avail_in = static_cast<uInt>(srcLen);
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(out.size());

    int ret = deflate(&strm, Z_FINISH);
    while (ret == Z_OK) {
        size_t old = out.size();
        out.resize(old * 2);
        strm.next_out = reinterpret_cast<Bytef*>(out.data() + old);
        strm.avail_out = static_cast<uInt>(out.size() - old);
        ret = deflate(&strm, Z_FINISH);
    }
    deflateEnd(&strm);
    if (ret != Z_STREAM_END) return false;
    out.resize(strm.total_out);
    return true;
}

static std::vector<ZipEntry> readZipEntries(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return {};

    f.seekg(0, std::ios::end);
    size_t fsize = static_cast<size_t>(f.tellg());
    if (fsize < sizeof(ZipEndRecord)) return {};

    std::vector<uint8_t> buf(fsize);
    f.seekg(0, std::ios::beg);
    f.read(reinterpret_cast<char*>(buf.data()), fsize);

    // Locate End of Central Directory Record
    int eocdPos = -1;
    for (int i = static_cast<int>(fsize - sizeof(ZipEndRecord)); i >= 0; --i) {
        if (*reinterpret_cast<const uint32_t*>(buf.data() + i) == 0x06054b50) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) return {};

    const auto* eocd = reinterpret_cast<const ZipEndRecord*>(buf.data() + eocdPos);

    // Streaming Guard: Max entries cap (4,096)
    constexpr size_t kMaxEntries = 4096;
    constexpr size_t kMaxSingleEntrySize = 150ULL * 1024 * 1024; // 150 MB
    constexpr size_t kMaxTotalArchiveSize = 500ULL * 1024 * 1024; // 500 MB

    if (eocd->numEntries > kMaxEntries) return {};

    std::vector<ZipEntry> entries;
    entries.reserve(eocd->numEntries);

    size_t totalUncompressed = 0;
    size_t cdOffset = eocd->centralDirOffset;
    for (uint16_t i = 0; i < eocd->numEntries; ++i) {
        if (cdOffset + sizeof(ZipCentralDir) > fsize) break;
        const auto* cd = reinterpret_cast<const ZipCentralDir*>(buf.data() + cdOffset);
        if (cd->signature != 0x02014b50) break;

        std::string name(reinterpret_cast<const char*>(buf.data() + cdOffset + sizeof(ZipCentralDir)),
                         cd->fileNameLen);

        // 1. Zip Slip Prevention: reject .., leading /, \, drive prefixes, null bytes, control chars
        if (name.empty() ||
            name.find("..") != std::string::npos ||
            name[0] == '/' || name[0] == '\\' ||
            name.find(':') != std::string::npos) {
            return {};
        }
        for (char c : name) {
            auto uc = static_cast<unsigned char>(c);
            if (uc < 0x20 || uc == 0x7F) {
                return {};
            }
        }

        // 2. Zip Bomb Prevention: single entry size, total size, and ratio check
        if (cd->uncompressedSize > kMaxSingleEntrySize) {
            return {};
        }
        totalUncompressed += cd->uncompressedSize;
        if (totalUncompressed > kMaxTotalArchiveSize) {
            return {};
        }
        if (cd->uncompressedSize > 1024 * 1024) {
            if (cd->compressedSize == 0 || (static_cast<uint64_t>(cd->uncompressedSize) / cd->compressedSize) > 100) {
                return {};
            }
        }

        size_t localOff = cd->localHeaderOffset;
        if (localOff + sizeof(ZipLocalHeader) <= fsize) {
            const auto* lh = reinterpret_cast<const ZipLocalHeader*>(buf.data() + localOff);
            if (lh->signature == 0x04034b50) {
                const uint8_t* compressedData = buf.data() + localOff + sizeof(ZipLocalHeader) +
                                                lh->fileNameLen + lh->extraLen;
                ZipEntry entry;
                entry.name = name;
                entry.modTime = cd->modTime;
                entry.modDate = cd->modDate;

                if (cd->compression == 0) {
                    entry.data.assign(compressedData, compressedData + cd->uncompressedSize);
                    entries.push_back(std::move(entry));
                } else if (cd->compression == 8) {
                    std::vector<uint8_t> uncompressed;
                    if (zlibInflate(compressedData, cd->compressedSize, uncompressed, cd->uncompressedSize)) {
                        entry.data = std::move(uncompressed);
                        entries.push_back(std::move(entry));
                    }
                }
            }
        }
        cdOffset += sizeof(ZipCentralDir) + cd->fileNameLen + cd->extraLen + cd->commentLen;
    }
    return entries;
}

static bool writeZipArchive(const std::filesystem::path& outPath, const std::vector<ZipEntry>& entries) {
    std::vector<uint8_t> out;
    struct CdEntry {
        std::string name;
        uint32_t crc32;
        uint32_t compSize;
        uint32_t uncompSize;
        uint32_t localOffset;
        uint16_t modTime;
        uint16_t modDate;
    };
    std::vector<CdEntry> cdEntries;

    for (const auto& entry : entries) {
        uint32_t localOffset = static_cast<uint32_t>(out.size());
        std::vector<uint8_t> deflated;
        bool useDeflate = zlibDeflate(entry.data.data(), entry.data.size(), deflated) &&
                          deflated.size() < entry.data.size();

        uint16_t compMethod = useDeflate ? 8 : 0;
        const uint8_t* dataToStore = useDeflate ? deflated.data() : entry.data.data();
        size_t storedSize = useDeflate ? deflated.size() : entry.data.size();
        uint32_t crc = static_cast<uint32_t>(crc32(0L, entry.data.data(), static_cast<uInt>(entry.data.size())));

        ZipLocalHeader lh{};
        lh.signature = 0x04034b50;
        lh.version = 20;
        lh.flags = 0;
        lh.compression = compMethod;
        lh.modTime = entry.modTime;
        lh.modDate = entry.modDate;
        lh.crc32 = crc;
        lh.compressedSize = static_cast<uint32_t>(storedSize);
        lh.uncompressedSize = static_cast<uint32_t>(entry.data.size());
        lh.fileNameLen = static_cast<uint16_t>(entry.name.size());
        lh.extraLen = 0;

        const auto* lhBytes = reinterpret_cast<const uint8_t*>(&lh);
        out.insert(out.end(), lhBytes, lhBytes + sizeof(lh));
        out.insert(out.end(), entry.name.begin(), entry.name.end());
        out.insert(out.end(), dataToStore, dataToStore + storedSize);

        CdEntry cd{};
        cd.name = entry.name;
        cd.crc32 = crc;
        cd.compSize = static_cast<uint32_t>(storedSize);
        cd.uncompSize = static_cast<uint32_t>(entry.data.size());
        cd.localOffset = localOffset;
        cd.modTime = entry.modTime;
        cd.modDate = entry.modDate;
        cdEntries.push_back(cd);
    }

    uint32_t cdOffset = static_cast<uint32_t>(out.size());
    for (const auto& cd : cdEntries) {
        ZipCentralDir c{};
        c.signature = 0x02014b50;
        c.versionMadeBy = 20;
        c.versionNeeded = 20;
        c.flags = 0;
        c.compression = (cd.compSize < cd.uncompSize) ? 8 : 0;
        c.modTime = cd.modTime;
        c.modDate = cd.modDate;
        c.crc32 = cd.crc32;
        c.compressedSize = cd.compSize;
        c.uncompressedSize = cd.uncompSize;
        c.fileNameLen = static_cast<uint16_t>(cd.name.size());
        c.extraLen = 0;
        c.commentLen = 0;
        c.diskStart = 0;
        c.intAttr = 0;
        c.extAttr = 0;
        c.localHeaderOffset = cd.localOffset;

        const auto* cBytes = reinterpret_cast<const uint8_t*>(&c);
        out.insert(out.end(), cBytes, cBytes + sizeof(c));
        out.insert(out.end(), cd.name.begin(), cd.name.end());
    }

    uint32_t cdSize = static_cast<uint32_t>(out.size() - cdOffset);
    ZipEndRecord eocd{};
    eocd.signature = 0x06054b50;
    eocd.diskNum = 0;
    eocd.centralDirDisk = 0;
    eocd.numEntriesOnDisk = static_cast<uint16_t>(cdEntries.size());
    eocd.numEntries = static_cast<uint16_t>(cdEntries.size());
    eocd.centralDirSize = cdSize;
    eocd.centralDirOffset = cdOffset;
    eocd.commentLen = 0;

    const auto* eBytes = reinterpret_cast<const uint8_t*>(&eocd);
    out.insert(out.end(), eBytes, eBytes + sizeof(eocd));

    // Atomic write
    auto parent = outPath.parent_path();
    std::filesystem::create_directories(parent);
    std::string tmp = ".edgedrop.tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto tmpPath = parent / tmp;

    {
        std::ofstream f(tmpPath, std::ios::binary);
        if (!f.is_open()) return false;
        f.write(reinterpret_cast<const char*>(out.data()), out.size());
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, outPath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    return true;
}

// ── Main Compression Execution ────────────────────────────────────────────────

JobResult OfficeCompressor::compress(const JobRequest& req,
                                     ProgressCallback progress,
                                     CancelToken cancel) {
    auto t0 = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = req.inputFilePath;

    if (!std::filesystem::exists(req.inputFilePath)) {
        result.success = false;
        result.errorMessage = "File not found: " + req.inputFilePath.string();
        return result;
    }

    result.inputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(req.inputFilePath));
    if (result.inputSizeBytes == 0) {
        result.success = false;
        result.errorMessage = "Input file is empty (0 bytes).";
        return result;
    }

    auto outDir = req.outputDirectory.empty()
        ? (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads")
        : req.outputDirectory;
    std::filesystem::create_directories(outDir);

    std::string ext = FormatDetector::outputExtension(req.detectedFormat);
    if (ext.empty()) ext = req.inputFilePath.extension().string();
    std::filesystem::path outPath = SecurityGuards::generateCompressedOutputPath(req.inputFilePath, outDir, ext);

    // ── Pass-Through Guard ────────────────────────────────────────────────────
    if (req.targetBytes > 0 && result.inputSizeBytes <= req.targetBytes) {
        std::error_code ec;
        std::filesystem::copy_file(req.inputFilePath, outPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.success = true;
            result.outputFilePath = outPath;
            result.outputSizeBytes = result.inputSizeBytes;
            result.outputFormat = FormatDetector::formatName(req.detectedFormat);
            auto t1 = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (progress) progress(1.0);
            return result;
        }
    }

    if (progress) progress(0.1);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    auto entries = readZipEntries(req.inputFilePath);
    if (entries.empty()) {
        result.success = false;
        result.errorMessage = "Malformed or potentially unsafe archive (Zip Slip / Zip Bomb rejected).";
        return result;
    }

    // Measure non-media XML size (B_xml) and find media entries
    size_t xmlBytes = 0;
    std::vector<size_t> mediaIndices;
    size_t totalMediaBytes = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& name = entries[i].name;
        bool isMedia = (name.find("/media/") != std::string::npos || name.find("media/") == 0);
        if (isMedia) {
            mediaIndices.push_back(i);
            totalMediaBytes += entries[i].data.size();
        } else {
            xmlBytes += entries[i].data.size();
        }
    }

    if (progress) progress(0.3);

    // Calculate media byte budget
    size_t mediaBudget = (req.targetBytes > 0 && static_cast<size_t>(req.targetBytes) > xmlBytes)
        ? static_cast<size_t>(req.targetBytes - xmlBytes)
        : (req.targetBytes > 0 ? static_cast<size_t>(req.targetBytes * 0.2) : 0);

    for (size_t idx : mediaIndices) {
        if (cancel && cancel->load()) {
            result.success = false;
            result.errorMessage = "Cancelled.";
            return result;
        }

        auto& entry = entries[idx];
        std::string lowerName = entry.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        bool isPng = (lowerName.size() >= 4 && lowerName.substr(lowerName.size() - 4) == ".png");
        bool isJpg = (lowerName.size() >= 4 && (lowerName.substr(lowerName.size() - 4) == ".jpg" ||
                                                (lowerName.size() >= 5 && lowerName.substr(lowerName.size() - 5) == ".jpeg")));

        if (!isPng && !isJpg) continue;

        cv::Mat img = cv::imdecode(entry.data, cv::IMREAD_UNCHANGED);
        if (img.empty()) continue;

        size_t targetPerImage = 0;
        if (mediaBudget > 0 && totalMediaBytes > 0) {
            double proportion = static_cast<double>(entry.data.size()) / static_cast<double>(totalMediaBytes);
            targetPerImage = static_cast<size_t>(mediaBudget * proportion);
        }

        std::vector<uint8_t> compressed;
        if (isPng) {
            compressed = ImageCompressor::pngCompress(img, targetPerImage);
        } else {
            compressed = ImageCompressor::jpegBinarySearch(img, targetPerImage, false);
        }

        // Only replace if strictly smaller
        if (!compressed.empty() && compressed.size() < entry.data.size()) {
            entry.data = std::move(compressed);
        }
    }

    if (progress) progress(0.8);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    // Secure Repack and write archive via SecureTempFile
    SecureTempFile tempFile(outDir, ext);
    if (!tempFile.isValid()) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    if (!writeZipArchive(tempFile.path(), entries)) {
        result.success = false;
        result.errorMessage = "Failed to write output Office document.";
        return result;
    }

    // Verify written ZIP integrity
    auto verifyEntries = readZipEntries(tempFile.path());
    if (verifyEntries.size() != entries.size()) {
        result.success = false;
        result.errorMessage = "Office document archive verification failed.";
        return result;
    }
    verifyEntries.clear();
    verifyEntries.shrink_to_fit();

    if (!tempFile.commit(outPath, "zip")) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    // Memory Hygiene: release large entries buffer
    entries.clear();
    entries.shrink_to_fit();

    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(outPath));
    result.outputFormat = FormatDetector::formatName(req.detectedFormat);

    if (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes) {
        result.hitFloor = true;
        result.floorNote = "Reached maximum media compression limit; XML structure preserved.";
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
