// PdfCompressor.cpp — libqpdf structural PDF compression (never rasterizes pages)
#include "PdfCompressor.hpp"
#include "ImageCompressor.hpp"
#include "FormatDetector.hpp"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>

#include <chrono>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

namespace EdgeDrop {

bool PdfCompressor::canHandle(DetectedFormat fmt) const {
    return fmt == DetectedFormat::PDF;
}

// ── Stage 1: lossless stream recompression via QPDFWriter ─────────────────────
static bool losslessCompress(const QString& inPath, const QString& outPath) {
    try {
        QPDF pdf;
        pdf.processFile(inPath.toStdString().c_str());

        QPDFWriter writer(pdf, outPath.toStdString().c_str());
        writer.setCompressStreams(true);
        writer.setRecompressFlate(true);
        writer.setPreserveUnreferencedObjects(false);
        writer.write();
        return true;
    } catch (...) {
        return false;
    }
}

// ── Stage 2: re-encode embedded JPEG images at a reduced quality ──────────────
static bool reencodeEmbeddedImages(const QString& inPath, const QString& outPath,
                                   int jpegQuality, int maxLongEdgePx) {
    try {
        QPDF pdf;
        pdf.processFile(inPath.toStdString().c_str());

        for (auto& obj : pdf.getAllObjects()) {
            if (!obj.isStream()) continue;
            auto dict = obj.getDict();
            if (!dict.isDictionary()) continue;
            if (!dict.hasKey("/Subtype")) continue;
            auto subtype = dict.getKey("/Subtype");
            if (!subtype.isName() || subtype.getName() != "/Image") continue;

            // Skip image masks and SMasks
            if (dict.hasKey("/ImageMask")) {
                auto imgMask = dict.getKey("/ImageMask");
                if (imgMask.isBool() && imgMask.getBoolValue()) continue;
            }

            if (!dict.hasKey("/Filter")) continue;
            auto filter = dict.getKey("/Filter");
            bool isDct = filter.isName() && filter.getName() == "/DCTDecode";
            if (!isDct) continue;

            // Get raw JPEG stream
            std::shared_ptr<Buffer> buf = obj.getRawStreamData();
            if (!buf || buf->getSize() < 1024) continue;

            std::vector<uint8_t> jpegIn(buf->getBuffer(), buf->getBuffer() + buf->getSize());

            // Optionally downscale large embedded images
            cv::Mat img = cv::imdecode(jpegIn, cv::IMREAD_COLOR);
            if (img.empty()) continue;

            if (maxLongEdgePx > 0) {
                int longEdge = std::max(img.cols, img.rows);
                if (longEdge > maxLongEdgePx) {
                    double scale = static_cast<double>(maxLongEdgePx) / longEdge;
                    cv::resize(img, img, cv::Size(), scale, scale, cv::INTER_AREA);
                }
            }

            std::vector<uint8_t> jpegOut;
            std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, jpegQuality, cv::IMWRITE_JPEG_OPTIMIZE, 1};
            cv::imencode(".jpg", img, jpegOut, params);
            if (jpegOut.empty()) continue;

            // Replace stream data
            obj.replaceStreamData(
                std::string(reinterpret_cast<const char*>(jpegOut.data()), jpegOut.size()),
                QPDFObjectHandle::newName("/DCTDecode"),
                QPDFObjectHandle::newNull()
            );

            // Update /Width and /Height if changed
            dict.replaceKey("/Width",  QPDFObjectHandle::newInteger(img.cols));
            dict.replaceKey("/Height", QPDFObjectHandle::newInteger(img.rows));
        }

        QPDFWriter writer(pdf, outPath.toStdString().c_str());
        writer.setCompressStreams(true);
        writer.write();
        return true;
    } catch (...) {
        return false;
    }
}

JobResult PdfCompressor::compress(const JobRequest& req,
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

    QString outDir = req.outputDirectory.isEmpty()
        ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
    QDir().mkpath(outDir);

    QString stem = inInfo.completeBaseName();
    QString outPath;
    int ctr = 0;
    do {
        QString sfx = ctr == 0 ? "" : QString(" (%1)").arg(ctr);
        outPath = QDir(outDir).filePath(stem + "_compressed" + sfx + ".pdf");
        ++ctr;
    } while (QFile::exists(outPath));

    // ── Check for password-protected PDF ──────────────────────────────────────
    try {
        QPDF test;
        test.processFile(req.inputFilePath.toStdString().c_str());
    } catch (const QPDFExc&) {
        result.success = false;
        result.errorMessage = "This PDF is password-protected and cannot be compressed.";
        return result;
    } catch (...) {
        result.success = false;
        result.errorMessage = "Cannot open PDF: " + req.inputFilePath;
        return result;
    }

    // ── Stage 1: lossless ────────────────────────────────────────────────────
    QString stage1Path = QDir::tempPath() + "/edgedrop_pdf_s1_" +
                         QString::number(QDateTime::currentMSecsSinceEpoch()) + ".pdf";

    if (!losslessCompress(req.inputFilePath, stage1Path)) {
        // Fall back to copying
        QFile::copy(req.inputFilePath, stage1Path);
    }

    if (progress) progress(0.4);
    if (cancel && cancel->load()) {
        QFile::remove(stage1Path);
        result.success = false; result.errorMessage = "Cancelled."; return result;
    }

    qint64 s1Size = QFileInfo(stage1Path).size();

    // If no target, or already within target after lossless, done
    if (req.targetBytes == 0 || s1Size <= req.targetBytes) {
        bool ok = QFile::copy(stage1Path, outPath);
        QFile::remove(stage1Path);
        if (!ok) {
            result.success = false;
            result.errorMessage = "Cannot write output: " + outPath;
            return result;
        }
        auto t1 = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        result.success = true;
        result.outputFilePath = outPath;
        result.outputSizeBytes = QFileInfo(outPath).size();
        result.outputFormat = "PDF";
        if (progress) progress(1.0);
        return result;
    }

    // ── Stage 2: binary search on embedded JPEG quality ───────────────────────
    size_t targetBytes = static_cast<size_t>(req.targetBytes);
    int bestQ = -1;
    QString bestPath = stage1Path;

    QString stage2Base = QDir::tempPath() + "/edgedrop_pdf_s2_" +
                         QString::number(QDateTime::currentMSecsSinceEpoch());

    int low = 25, high = 85;
    for (int iter = 0; iter < 7 && low <= high; ++iter) {
        int mid = low + (high - low) / 2;
        QString tryPath = stage2Base + "_q" + QString::number(mid) + ".pdf";
        if (reencodeEmbeddedImages(stage1Path, tryPath, mid, 1600)) {
            qint64 sz = QFileInfo(tryPath).size();
            if (static_cast<size_t>(sz) <= targetBytes) {
                if (bestQ == -1 || mid > bestQ) {
                    if (bestQ != -1) QFile::remove(bestPath);
                    bestQ = mid;
                    bestPath = tryPath;
                } else {
                    QFile::remove(tryPath);
                }
                low = mid + 1;
            } else {
                QFile::remove(tryPath);
                high = mid - 1;
            }
        } else {
            high = mid - 1;
        }
        if (progress) progress(0.4 + 0.5 * (iter / 7.0));
        if (cancel && cancel->load()) break;
    }

    if (progress) progress(0.95);

    // Copy best result to outPath
    bool copied = QFile::copy(bestPath, outPath);

    // Cleanup temp files
    if (bestPath != stage1Path) QFile::remove(bestPath);
    QFile::remove(stage1Path);

    if (!copied) {
        result.success = false;
        result.errorMessage = "Cannot write output: " + outPath;
        return result;
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = QFileInfo(outPath).size();
    result.outputFormat = "PDF";

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
