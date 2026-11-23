// OfficeCompressor.cpp — DOCX/PPTX/XLSX ZIP-based media re-compression
// Uses miniz (bundled single-header) for ZIP read/write.
#include "OfficeCompressor.hpp"
#include "ImageCompressor.hpp"
#include "FormatDetector.hpp"

#include <opencv2/imgcodecs.hpp>

#include <QFileInfo>
#include <QDir>
#include <QFile>

#include <chrono>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

// ── miniz — bundled minimal ZIP implementation ────────────────────────────────
// We use a minimal internal ZIP implementation here (no external dependency).
// For production use, link against miniz or libzip.

// Use Qt's built-in ZIP reading via ZLIB, or fall back to no-op if unavailable.
// Since we cannot access the network to download miniz, we implement a minimal
// ZIP re-compressor using Qt's QIODevice + zlib directly.

#include <zlib.h>  // macOS ships zlib with Xcode CommandLineTools

namespace EdgeDrop {

bool OfficeCompressor::canHandle(DetectedFormat fmt) const {
    return fmt == DetectedFormat::DOCX
        || fmt == DetectedFormat::PPTX
        || fmt == DetectedFormat::XLSX;
}

// ── Minimal ZIP structures ────────────────────────────────────────────────────

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
    std::vector<uint8_t> data;  // uncompressed
    uint16_t modTime = 0;
    uint16_t modDate = 0;
};

// ── Decompress a DEFLATE stream via zlib ─────────────────────────────────────
static bool zlibInflate(const uint8_t* src, size_t srcLen,
                        std::vector<uint8_t>& out, size_t uncompressedHint) {
    out.resize(std::max(uncompressedHint, srcLen * 3));
    z_stream strm{};
    if (inflateInit2(&strm, -15) != Z_OK) return false;

    strm.next_in  = const_cast<uint8_t*>(src);
    strm.avail_in = static_cast<uInt>(srcLen);

    size_t written = 0;
    int ret = Z_OK;
    while (ret == Z_OK || ret == Z_BUF_ERROR) {
        strm.next_out  = out.data() + written;
        strm.avail_out = static_cast<uInt>(out.size() - written);
        ret = inflate(&strm, Z_SYNC_FLUSH);
        written = out.size() - strm.avail_out;
        if (strm.avail_out == 0) {
            out.resize(out.size() * 2);
        }
        if (ret == Z_STREAM_END) break;
        if (ret != Z_OK && ret != Z_BUF_ERROR) { inflateEnd(&strm); return false; }
    }
    inflateEnd(&strm);
    out.resize(written);
    return true;
}

// ── Compress data to DEFLATE ──────────────────────────────────────────────────
static std::vector<uint8_t> zlibDeflate(const uint8_t* src, size_t srcLen, int level = 9) {
    std::vector<uint8_t> out(srcLen + srcLen / 100 + 128);
    z_stream strm{};
    if (deflateInit2(&strm, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) return {};

    strm.next_in  = const_cast<uint8_t*>(src);
    strm.avail_in = static_cast<uInt>(srcLen);
    strm.next_out  = out.data();
    strm.avail_out = static_cast<uInt>(out.size());

    int ret = deflate(&strm, Z_FINISH);
    deflateEnd(&strm);
    if (ret != Z_STREAM_END) return {};
    out.resize(strm.total_out);
    return out;
}

// ── Read all entries from a ZIP file into memory ──────────────────────────────
static bool readZip(const QByteArray& zipData, std::vector<ZipEntry>& entries) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(zipData.constData());
    size_t totalSize = static_cast<size_t>(zipData.size());

    // Find End of Central Directory record (search backwards)
    if (totalSize < sizeof(ZipEndRecord)) return false;

    int endPos = -1;
    for (int i = static_cast<int>(totalSize) - static_cast<int>(sizeof(ZipEndRecord)); i >= 0; --i) {
        const auto* er = reinterpret_cast<const ZipEndRecord*>(base + i);
        if (er->signature == 0x06054b50) {
            endPos = i;
            break;
        }
    }
    if (endPos < 0) return false;

    const auto* endRec = reinterpret_cast<const ZipEndRecord*>(base + endPos);
    size_t cdOffset = endRec->centralDirOffset;

    for (uint16_t e = 0; e < endRec->numEntries; ++e) {
        if (cdOffset + sizeof(ZipCentralDir) > totalSize) break;
        const auto* cd = reinterpret_cast<const ZipCentralDir*>(base + cdOffset);
        if (cd->signature != 0x02014b50) break;

        ZipEntry entry;
        entry.name = std::string(reinterpret_cast<const char*>(base + cdOffset + sizeof(ZipCentralDir)),
                                 cd->fileNameLen);
        entry.modTime = cd->modTime;
        entry.modDate = cd->modDate;

        // Find local header
        size_t lhOffset = cd->localHeaderOffset;
        if (lhOffset + sizeof(ZipLocalHeader) > totalSize) {
            cdOffset += sizeof(ZipCentralDir) + cd->fileNameLen + cd->extraLen + cd->commentLen;
            continue;
        }
        const auto* lh = reinterpret_cast<const ZipLocalHeader*>(base + lhOffset);
        size_t dataOffset = lhOffset + sizeof(ZipLocalHeader) + lh->fileNameLen + lh->extraLen;

        if (dataOffset + cd->compressedSize > totalSize) {
            cdOffset += sizeof(ZipCentralDir) + cd->fileNameLen + cd->extraLen + cd->commentLen;
            continue;
        }

        const uint8_t* compData = base + dataOffset;

        if (cd->compression == 0) {
            // Stored
            entry.data.assign(compData, compData + cd->uncompressedSize);
        } else if (cd->compression == 8) {
            // Deflate
            if (!zlibInflate(compData, cd->compressedSize, entry.data, cd->uncompressedSize)) {
                entry.data.assign(compData, compData + cd->compressedSize); // keep raw on failure
            }
        }

        entries.push_back(std::move(entry));
        cdOffset += sizeof(ZipCentralDir) + cd->fileNameLen + cd->extraLen + cd->commentLen;
    }

    return !entries.empty();
}

// ── Write entries back to a ZIP byte array ────────────────────────────────────
static QByteArray writeZip(const std::vector<ZipEntry>& entries) {
    QByteArray out;

    struct CdEntry {
        std::string name;
        uint32_t crc = 0;
        uint32_t compressedSize = 0;
        uint32_t uncompressedSize = 0;
        uint16_t modTime = 0;
        uint16_t modDate = 0;
        uint32_t localOffset = 0;
        uint16_t compression = 8;
    };
    std::vector<CdEntry> cdEntries;

    for (const auto& entry : entries) {
        CdEntry cd;
        cd.name = entry.name;
        cd.modTime = entry.modTime;
        cd.modDate = entry.modDate;
        cd.uncompressedSize = static_cast<uint32_t>(entry.data.size());
        cd.localOffset = static_cast<uint32_t>(out.size());

        // CRC32
        cd.crc = static_cast<uint32_t>(
            crc32(0L, entry.data.data(), static_cast<uInt>(entry.data.size())));

        // Compress
        std::vector<uint8_t> comp = zlibDeflate(entry.data.data(), entry.data.size(), 9);
        if (comp.empty() || comp.size() >= entry.data.size()) {
            // Store uncompressed if deflate fails or is larger
            comp = entry.data;
            cd.compression = 0;
        }
        cd.compressedSize = static_cast<uint32_t>(comp.size());

        // Local header
        ZipLocalHeader lh{};
        lh.signature = 0x04034b50;
        lh.version = 20;
        lh.flags = 0;
        lh.compression = cd.compression;
        lh.modTime = cd.modTime;
        lh.modDate = cd.modDate;
        lh.crc32 = cd.crc;
        lh.compressedSize = cd.compressedSize;
        lh.uncompressedSize = cd.uncompressedSize;
        lh.fileNameLen = static_cast<uint16_t>(cd.name.size());
        lh.extraLen = 0;

        out.append(reinterpret_cast<const char*>(&lh), sizeof(lh));
        out.append(cd.name.c_str(), static_cast<qsizetype>(cd.name.size()));
        out.append(reinterpret_cast<const char*>(comp.data()), static_cast<qsizetype>(comp.size()));

        cdEntries.push_back(cd);
    }

    uint32_t cdStart = static_cast<uint32_t>(out.size());

    for (const auto& cd : cdEntries) {
        ZipCentralDir cdrec{};
        cdrec.signature = 0x02014b50;
        cdrec.versionMadeBy = 20;
        cdrec.versionNeeded = 20;
        cdrec.flags = 0;
        cdrec.compression = cd.compression;
        cdrec.modTime = cd.modTime;
        cdrec.modDate = cd.modDate;
        cdrec.crc32 = cd.crc;
        cdrec.compressedSize = cd.compressedSize;
        cdrec.uncompressedSize = cd.uncompressedSize;
        cdrec.fileNameLen = static_cast<uint16_t>(cd.name.size());
        cdrec.extraLen = 0;
        cdrec.commentLen = 0;
        cdrec.diskStart = 0;
        cdrec.intAttr = 0;
        cdrec.extAttr = 0;
        cdrec.localHeaderOffset = cd.localOffset;

        out.append(reinterpret_cast<const char*>(&cdrec), sizeof(cdrec));
        out.append(cd.name.c_str(), static_cast<qsizetype>(cd.name.size()));
    }

    uint32_t cdSize = static_cast<uint32_t>(out.size()) - cdStart;

    ZipEndRecord endRec{};
    endRec.signature = 0x06054b50;
    endRec.diskNum = 0;
    endRec.centralDirDisk = 0;
    endRec.numEntriesOnDisk = static_cast<uint16_t>(cdEntries.size());
    endRec.numEntries = static_cast<uint16_t>(cdEntries.size());
    endRec.centralDirSize = cdSize;
    endRec.centralDirOffset = cdStart;
    endRec.commentLen = 0;

    out.append(reinterpret_cast<const char*>(&endRec), sizeof(endRec));
    return out;
}

// ── Determine if a ZIP entry name is a re-compressible media file ─────────────
static bool isMediaEntry(const std::string& name) {
    auto lc = name;
    std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
    return (lc.find("/media/") != std::string::npos || lc.find("\\media\\") != std::string::npos)
        && (lc.rfind(".jpg")  != std::string::npos ||
            lc.rfind(".jpeg") != std::string::npos ||
            lc.rfind(".png")  != std::string::npos ||
            lc.rfind(".bmp")  != std::string::npos);
}

// ─── Main compress ────────────────────────────────────────────────────────────

JobResult OfficeCompressor::compress(const JobRequest& req,
                                     ProgressCallback progress,
                                     CancelToken cancel) {
    auto t0 = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = req.inputFilePath;

    QFileInfo inInfo(req.inputFilePath);
    if (!inInfo.exists()) {
        result.success = false;
        result.errorMessage = "File not found: " + req.inputFilePath;
        return result;
    }
    result.inputSizeBytes = inInfo.size();

    if (progress) progress(0.05);

    // Read ZIP
    QFile f(req.inputFilePath);
    if (!f.open(QIODevice::ReadOnly)) {
        result.success = false;
        result.errorMessage = "Cannot open: " + req.inputFilePath;
        return result;
    }
    QByteArray zipData = f.readAll();
    f.close();

    std::vector<ZipEntry> entries;
    if (!readZip(zipData, entries)) {
        result.success = false;
        result.errorMessage = "Cannot parse ZIP: " + req.inputFilePath;
        return result;
    }

    if (progress) progress(0.2);

    // Compute proportional scale factor for media images
    double scaleFactor = 1.0;
    if (req.targetBytes > 0 && result.inputSizeBytes > req.targetBytes) {
        scaleFactor = static_cast<double>(req.targetBytes) / static_cast<double>(result.inputSizeBytes);
        scaleFactor = std::clamp(scaleFactor, 0.3, 0.98);
    }

    int processedMedia = 0;
    int totalMedia = static_cast<int>(
        std::count_if(entries.begin(), entries.end(),
                      [](const ZipEntry& e) { return isMediaEntry(e.name); }));

    for (auto& entry : entries) {
        if (cancel && cancel->load()) break;

        if (!isMediaEntry(entry.name) || entry.data.empty()) continue;

        // Determine format from extension
        auto lc = entry.name;
        std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
        bool isJpeg = lc.rfind(".jpg") != std::string::npos || lc.rfind(".jpeg") != std::string::npos;
        bool isPng  = lc.rfind(".png") != std::string::npos;

        cv::Mat img = cv::imdecode(entry.data, isJpeg ? cv::IMREAD_COLOR : cv::IMREAD_UNCHANGED);
        if (img.empty()) continue;

        // Target size for this media entry
        size_t entryTarget = static_cast<size_t>(entry.data.size() * scaleFactor);
        if (entryTarget < 10240) entryTarget = 10240; // 10 KB floor

        std::vector<uint8_t> recompressed;
        if (isPng) {
            recompressed = ImageCompressor::pngCompress(img, entryTarget);
        } else {
            recompressed = ImageCompressor::jpegBinarySearch(img, entryTarget, false, false);
        }

        if (!recompressed.empty() && recompressed.size() < entry.data.size()) {
            entry.data = std::move(recompressed);
        }

        ++processedMedia;
        if (progress && totalMedia > 0) {
            progress(0.2 + 0.7 * (static_cast<double>(processedMedia) / totalMedia));
        }
    }

    // Write new ZIP
    QByteArray newZip = writeZip(entries);

    QString outDir = req.outputDirectory.isEmpty()
        ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
    QDir().mkpath(outDir);

    QString ext = FormatDetector::outputExtension(req.detectedFormat);
    QString stem = inInfo.completeBaseName();
    QString outPath;
    int ctr = 0;
    do {
        QString sfx = ctr == 0 ? "" : QString(" (%1)").arg(ctr);
        outPath = QDir(outDir).filePath(stem + "_compressed" + sfx + ext);
        ++ctr;
    } while (QFile::exists(outPath));

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        result.success = false;
        result.errorMessage = "Cannot write: " + outPath;
        return result;
    }
    outFile.write(newZip);
    outFile.close();

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<qint64>(newZip.size());
    result.outputFormat = FormatDetector::formatName(req.detectedFormat);

    result.hitFloor = (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes);
    if (result.hitFloor) {
        result.floorNote = QString("Compressed as far as safely possible: %1 (target was %2)")
            .arg(result.formattedOutputSize())
            .arg(JobResult::formatBytes(req.targetBytes));
    }

    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
