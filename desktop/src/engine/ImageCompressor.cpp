// ImageCompressor.cpp — v2 same-type image compression backend
#include "ImageCompressor.hpp"
#include "FormatDetector.hpp"
#include "JfifDpiPatcher.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QByteArray>

#include <fstream>
#include <cmath>
#include <algorithm>
#include <chrono>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>
#include <CoreServices/CoreServices.h>
#endif

namespace EdgeDrop {

// ─── canHandle ────────────────────────────────────────────────────────────────

bool ImageCompressor::canHandle(DetectedFormat fmt) const {
    switch (fmt) {
        case DetectedFormat::JPEG:
        case DetectedFormat::PNG:
        case DetectedFormat::WebP:
        case DetectedFormat::HEIC:
        case DetectedFormat::TIFF:
        case DetectedFormat::BMP:
            return true;
        default:
            return false;
    }
}

// ─── Load & decode ────────────────────────────────────────────────────────────

cv::Mat ImageCompressor::loadDecoded(const QString& filePath, bool flattenAlpha) {
    // Read via QFile into buffer (handles Unicode/spaces in paths)
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray ba = f.readAll();
    f.close();

    std::vector<uint8_t> buf(ba.begin(), ba.end());
    cv::Mat img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    if (img.empty()) return {};

    // Flatten 4-channel BGRA onto white for JPEG output
    if (flattenAlpha && img.channels() == 4) {
        cv::Mat bgr(img.rows, img.cols, CV_8UC3, cv::Scalar(255, 255, 255));
        for (int y = 0; y < img.rows; ++y) {
            const cv::Vec4b* src = img.ptr<cv::Vec4b>(y);
            cv::Vec3b*       dst = bgr.ptr<cv::Vec3b>(y);
            for (int x = 0; x < img.cols; ++x) {
                float a = src[x][3] / 255.0f;
                dst[x][0] = static_cast<uint8_t>(src[x][0] * a + 255.f * (1.f - a));
                dst[x][1] = static_cast<uint8_t>(src[x][1] * a + 255.f * (1.f - a));
                dst[x][2] = static_cast<uint8_t>(src[x][2] * a + 255.f * (1.f - a));
            }
        }
        return bgr;
    }

    // Convert 1-channel gray to BGR for uniform processing
    if (img.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }

    return img;
}

// ─── k-means color quantisation ───────────────────────────────────────────────

cv::Mat ImageCompressor::quantizeColors(const cv::Mat& src, int maxColors) {
    if (src.empty() || maxColors <= 0 || maxColors >= 256) return src;

    cv::Mat data;
    src.reshape(1, src.rows * src.cols).convertTo(data, CV_32F);

    cv::Mat labels, centers;
    cv::kmeans(data, maxColors, labels,
               cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0),
               3, cv::KMEANS_PP_CENTERS, centers);

    centers.convertTo(centers, CV_8U);
    cv::Mat quantized(src.size(), src.type());
    cv::Mat flat = quantized.reshape(1, src.rows * src.cols);

    for (int i = 0; i < src.rows * src.cols; ++i) {
        int ci = labels.at<int>(i);
        for (int c = 0; c < src.channels(); ++c) {
            flat.at<uint8_t>(i, c) = centers.at<uint8_t>(ci, c);
        }
    }
    return quantized;
}

// ─── JPEG / WebP binary search ────────────────────────────────────────────────

std::vector<uint8_t> ImageCompressor::jpegBinarySearch(cv::Mat& img,
                                                        size_t targetMaxBytes,
                                                        bool isWebP,
                                                        bool lockDimensions) {
    if (img.empty()) return {};

    const std::string ext = isWebP ? ".webp" : ".jpg";
    const int paramId = isWebP ? cv::IMWRITE_WEBP_QUALITY : cv::IMWRITE_JPEG_QUALITY;

    // Apply 3% safety margin
    size_t safeMax = (targetMaxBytes > 0) ? static_cast<size_t>(targetMaxBytes * 0.97) : 0;

    cv::Mat work = img;

    auto tryEncode = [&](int q) -> std::vector<uint8_t> {
        std::vector<uint8_t> buf;
        std::vector<int> params = {paramId, q};
        if (!isWebP) {
            params.push_back(cv::IMWRITE_JPEG_OPTIMIZE);
            params.push_back(1);
        }
        cv::imencode(ext, work, buf, params);
        return buf;
    };

    if (safeMax == 0) {
        auto buf = tryEncode(90);
        img = work;
        return buf;
    }

    // Binary search q ∈ [15, 95]
    int low = 15, high = 95;
    int bestQ = -1;
    std::vector<uint8_t> bestBuf;

    for (int iter = 0; iter < 8 && low <= high; ++iter) {
        int mid = low + (high - low) / 2;
        auto cur = tryEncode(mid);
        if (cur.size() <= safeMax) {
            bestQ = mid;
            bestBuf = std::move(cur);
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    if (bestQ != -1) {
        img = work;
        return bestBuf;
    }

    // Even q=15 too big — try dimension scaling
    if (!lockDimensions) {
        for (int step = 0; step < 10; ++step) {
            cv::resize(work, work, cv::Size(), 0.90, 0.90, cv::INTER_AREA);
            low = 15; high = 90;
            for (int iter = 0; iter < 7 && low <= high; ++iter) {
                int mid = low + (high - low) / 2;
                auto cur = tryEncode(mid);
                if (cur.size() <= safeMax) {
                    bestQ = mid;
                    bestBuf = std::move(cur);
                    low = mid + 1;
                } else {
                    high = mid - 1;
                }
            }
            if (bestQ != -1) break;
        }
    }

    img = work;
    if (!bestBuf.empty()) return bestBuf;

    // Floor guard: return lowest-quality encode
    return tryEncode(15);
}

// ─── PNG compression ──────────────────────────────────────────────────────────

std::vector<uint8_t> ImageCompressor::pngCompress(cv::Mat& img, size_t targetMaxBytes) {
    if (img.empty()) return {};

    std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 9};
    std::vector<uint8_t> buf;
    cv::imencode(".png", img, buf, params);

    size_t safeMax = (targetMaxBytes > 0) ? static_cast<size_t>(targetMaxBytes * 0.97) : 0;
    if (safeMax == 0 || buf.size() <= safeMax) return buf;

    // Gentle scaling first
    cv::Mat work = img;
    for (int s = 0; s < 8 && buf.size() > safeMax; ++s) {
        cv::resize(work, work, cv::Size(), 0.90, 0.90, cv::INTER_AREA);
        cv::imencode(".png", work, buf, params);
    }

    if (buf.size() <= safeMax) {
        img = work;
        return buf;
    }

    // Palette quantisation
    for (int colors : {256, 128, 64, 32}) {
        cv::Mat q = quantizeColors(work, colors);
        std::vector<uint8_t> qbuf;
        cv::imencode(".png", q, qbuf, params);
        if (qbuf.size() <= safeMax || colors == 32) {
            img = q;
            return qbuf;
        }
    }
    return buf;
}

// ─── HEIC compression via ImageIO (macOS only) ───────────────────────────────

#ifdef __APPLE__
static JobResult compressHeic(const JobRequest& req, ProgressCallback progress) {
    if (progress) progress(0.05);

    QFileInfo inInfo(req.inputFilePath);
    if (!inInfo.exists()) {
        JobResult r; r.success = false;
        r.errorMessage = "File not found: " + req.inputFilePath;
        return r;
    }

    // Load input via ImageIO
    CFStringRef pathStr = req.inputFilePath.toCFString();
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, pathStr, kCFURLPOSIXPathStyle, false);
    CFRelease(pathStr);

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) {
        JobResult r; r.success = false;
        r.errorMessage = "Cannot open HEIC: " + req.inputFilePath;
        return r;
    }

    CGImageRef cgImage = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!cgImage) {
        JobResult r; r.success = false;
        r.errorMessage = "Cannot decode HEIC image";
        return r;
    }

    if (progress) progress(0.3);

    // Determine output path
    QString outDir = req.outputDirectory.isEmpty()
        ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
    QDir().mkpath(outDir);

    QString stem = inInfo.completeBaseName();
    QString outPath;
    int counter = 0;
    do {
        QString suffix = counter == 0 ? "" : QString(" (%1)").arg(counter);
        outPath = QDir(outDir).filePath(stem + "_compressed" + suffix + ".heic");
        ++counter;
    } while (QFile::exists(outPath));

    // Binary search quality
    size_t safeMax = req.targetBytes > 0 ? static_cast<size_t>(req.targetBytes * 0.97) : 0;

    auto encodeToFile = [&](double q) -> qint64 {
        CFStringRef outPathStr = outPath.toCFString();
        CFURLRef outUrl = CFURLCreateWithFileSystemPath(nullptr, outPathStr, kCFURLPOSIXPathStyle, false);
        CFRelease(outPathStr);

        CFStringRef type = CFSTR("public.heic");
        CGImageDestinationRef dst = CGImageDestinationCreateWithURL(outUrl, type, 1, nullptr);
        CFRelease(outUrl);
        if (!dst) return -1;

        CFNumberRef qNum = CFNumberCreate(nullptr, kCFNumberDoubleType, &q);
        CFStringRef key = kCGImageDestinationLossyCompressionQuality;
        CFDictionaryRef opts = CFDictionaryCreate(nullptr,
            reinterpret_cast<const void**>(&key),
            reinterpret_cast<const void**>(&qNum), 1,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFRelease(qNum);

        CGImageDestinationAddImage(dst, cgImage, opts);
        CFRelease(opts);
        bool ok = CGImageDestinationFinalize(dst);
        CFRelease(dst);
        if (!ok) return -1;

        return QFileInfo(outPath).size();
    };

    if (safeMax == 0) {
        encodeToFile(0.75);
    } else {
        double lo = 0.05, hi = 0.95;
        double bestFound = -1.0;
        for (int iter = 0; iter < 8; ++iter) {
            double mid = (lo + hi) / 2.0;
            qint64 sz = encodeToFile(mid);
            if (sz > 0 && static_cast<size_t>(sz) <= safeMax) {
                bestFound = mid;
                lo = mid + 0.05;
            } else {
                hi = mid - 0.05;
            }
            if (progress) progress(0.3 + 0.6 * (iter / 8.0));
        }
        if (bestFound >= 0) {
            encodeToFile(bestFound);
        } else {
            // Floor: encode at lowest
            encodeToFile(0.05);
        }
    }

    CGImageRelease(cgImage);

    if (progress) progress(1.0);

    QFileInfo outInfo(outPath);
    JobResult result;
    result.success = true;
    result.inputFilePath = req.inputFilePath;
    result.outputFilePath = outPath;
    result.inputSizeBytes = inInfo.size();
    result.outputSizeBytes = outInfo.size();
    result.outputFormat = "HEIC";
    result.hitFloor = (req.targetBytes > 0 && outInfo.size() > req.targetBytes);
    if (result.hitFloor) {
        result.floorNote = QString("Compressed as far as safely possible: %1 (target was %2)")
            .arg(JobResult::formatBytes(outInfo.size()))
            .arg(JobResult::formatBytes(req.targetBytes));
    }
    return result;
}
#endif // __APPLE__

// ─── Main compress entry point ────────────────────────────────────────────────

JobResult ImageCompressor::compress(const JobRequest& req,
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

    if (progress) progress(0.0);

    // ── HEIC: macOS native path ───────────────────────────────────────────────
#ifdef __APPLE__
    if (req.detectedFormat == DetectedFormat::HEIC) {
        result = compressHeic(req, progress);
        auto t1 = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        return result;
    }
#endif

    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Job was cancelled.";
        return result;
    }

    // ── Raster path (OpenCV) ──────────────────────────────────────────────────
    bool isJpeg = (req.detectedFormat == DetectedFormat::JPEG);
    bool isPng  = (req.detectedFormat == DetectedFormat::PNG);
    bool isWebP = (req.detectedFormat == DetectedFormat::WebP);
    bool isTiff = (req.detectedFormat == DetectedFormat::TIFF);
    bool isBmp  = (req.detectedFormat == DetectedFormat::BMP);

    // Pass-through guard: if already within target, copy and return
    if (req.targetBytes > 0 && inInfo.size() <= req.targetBytes) {
        QString outDir = req.outputDirectory.isEmpty()
            ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
        QDir().mkpath(outDir);
        QString stem = inInfo.completeBaseName();
        QString ext  = FormatDetector::outputExtension(req.detectedFormat);
        QString outPath = QDir(outDir).filePath(stem + "_compressed" + ext);
        int ctr = 1;
        while (QFile::exists(outPath)) {
            outPath = QDir(outDir).filePath(
                stem + "_compressed (" + QString::number(ctr++) + ")" + ext);
        }
        QFile::copy(req.inputFilePath, outPath);
        result.success = true;
        result.outputFilePath = outPath;
        result.outputSizeBytes = inInfo.size();
        result.outputFormat = FormatDetector::formatName(req.detectedFormat);
        if (progress) progress(1.0);
        auto t1 = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        return result;
    }

    // Load image
    cv::Mat img = loadDecoded(req.inputFilePath, isJpeg);
    if (img.empty()) {
        result.success = false;
        result.errorMessage = "Cannot decode image: " + req.inputFilePath;
        return result;
    }

    if (progress) progress(0.15);
    if (cancel && cancel->load()) {
        result.success = false; result.errorMessage = "Cancelled."; return result;
    }

    std::vector<uint8_t> encoded;
    size_t targetMax = (req.targetBytes > 0) ? static_cast<size_t>(req.targetBytes) : 0;

    if (isPng) {
        encoded = pngCompress(img, targetMax);
    } else if (isWebP) {
        encoded = jpegBinarySearch(img, targetMax, true, false);
    } else if (isTiff) {
        // TIFF: Deflate compression
        std::vector<int> params = {
            cv::IMWRITE_TIFF_COMPRESSION, 5, // COMPRESSION_DEFLATE
            cv::IMWRITE_TIFF_XDPI, 150,
            cv::IMWRITE_TIFF_YDPI, 150
        };
        cv::imencode(".tiff", img, encoded, params);
        // If still over, scale
        size_t safeMax = targetMax > 0 ? static_cast<size_t>(targetMax * 0.97) : 0;
        if (safeMax > 0 && encoded.size() > safeMax) {
            cv::Mat work = img;
            for (int s = 0; s < 8 && encoded.size() > safeMax; ++s) {
                cv::resize(work, work, cv::Size(), 0.85, 0.85, cv::INTER_AREA);
                cv::imencode(".tiff", work, encoded, params);
            }
        }
    } else if (isBmp) {
        cv::imencode(".bmp", img, encoded);
        size_t safeMax = targetMax > 0 ? static_cast<size_t>(targetMax * 0.97) : 0;
        if (safeMax > 0 && encoded.size() > safeMax) {
            cv::Mat work = img;
            for (int s = 0; s < 10 && encoded.size() > safeMax; ++s) {
                cv::resize(work, work, cv::Size(), 0.85, 0.85, cv::INTER_AREA);
                cv::imencode(".bmp", work, encoded, {});
            }
        }
    } else {
        // JPEG default
        encoded = jpegBinarySearch(img, targetMax, false, false);
        // Patch DPI
        if (!encoded.empty()) {
            JfifDpiPatcher::patchJpegDpi(encoded, 72);
        }
    }

    if (progress) progress(0.9);

    if (encoded.empty()) {
        result.success = false;
        result.errorMessage = "Failed to encode image.";
        return result;
    }

    // ── Write output ──────────────────────────────────────────────────────────
    QString outDir = req.outputDirectory.isEmpty()
        ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
    QDir().mkpath(outDir);

    QString stem = inInfo.completeBaseName();
    QString ext  = FormatDetector::outputExtension(req.detectedFormat);
    int finalKb  = static_cast<int>(std::ceil(static_cast<double>(encoded.size()) / 1024.0));
    QString outName = stem + "_compressed_" + QString::number(finalKb) + "kb" + ext;
    QString outPath = QDir(outDir).filePath(outName);
    int ctr = 1;
    while (QFile::exists(outPath)) {
        outPath = QDir(outDir).filePath(
            stem + "_compressed_" + QString::number(finalKb) + "kb (" + QString::number(ctr++) + ")" + ext);
    }

    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        result.success = false;
        result.errorMessage = "Cannot write output: " + outPath;
        return result;
    }
    outFile.write(reinterpret_cast<const char*>(encoded.data()),
                  static_cast<qint64>(encoded.size()));
    outFile.close();

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<qint64>(encoded.size());
    result.outputWidth  = img.cols;
    result.outputHeight = img.rows;
    result.outputDpi    = 72;
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
