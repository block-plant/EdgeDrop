#include "edgedrop/image_compressor.hpp"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"
#include "vendor/lodepng.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <random>
#include <array>
#include <queue>

namespace EdgeDrop {

static std::shared_ptr<INativeImageCodec> s_nativeCodec = nullptr;

void ImageCompressor::setNativeCodec(std::shared_ptr<INativeImageCodec> codec) {
    s_nativeCodec = codec;
}

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

// ── Decode Image from File Safely ─────────────────────────────────────────────

cv::Mat ImageCompressor::loadDecoded(const std::filesystem::path& filePath, bool flattenAlpha) {
    if (!std::filesystem::exists(filePath)) return {};
    auto fsize = std::filesystem::file_size(filePath);
    if (fsize == 0) return {};

    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return {};

    std::vector<uint8_t> buf(static_cast<size_t>(fsize));
    f.read(reinterpret_cast<char*>(buf.data()), fsize);

    cv::Mat img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    if (img.empty()) return {};

    // Guard against decompression bombs (>100M pixels or oversized dimensions)
    if (img.cols <= 0 || img.rows <= 0 || img.cols > 32768 || img.rows > 32768) {
        return {};
    }
    uint64_t totalPixels = static_cast<uint64_t>(img.cols) * static_cast<uint64_t>(img.rows);
    if (totalPixels > 100000000ULL) {
        return {};
    }

    // Flatten 4-channel BGRA onto white for JPEG/BMP output
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

    if (img.channels() == 1) {
        cv::Mat bgr;
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }

    return img;
}

// ── K-Means Quantization ──────────────────────────────────────────────────────

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

// ── Median-Cut Quantization with Floyd-Steinberg Dithering ────────────────────

struct ColorBox {
    std::vector<size_t> pixelIndices;
    uint8_t minR = 255, maxR = 0;
    uint8_t minG = 255, maxG = 0;
    uint8_t minB = 255, maxB = 0;

    int volume() const {
        return (maxR - minR) * (maxG - minG) * (maxB - minB);
    }
    int maxDimension() const {
        return std::max({maxR - minR, maxG - minG, maxB - minB});
    }
};

static void updateBoxBounds(ColorBox& box, const std::vector<cv::Vec4b>& pixels) {
    box.minR = 255; box.maxR = 0;
    box.minG = 255; box.maxG = 0;
    box.minB = 255; box.maxB = 0;
    for (size_t idx : box.pixelIndices) {
        const auto& p = pixels[idx];
        box.minR = std::min(box.minR, p[2]);
        box.maxR = std::max(box.maxR, p[2]);
        box.minG = std::min(box.minG, p[1]);
        box.maxG = std::max(box.maxG, p[1]);
        box.minB = std::min(box.minB, p[0]);
        box.maxB = std::max(box.maxB, p[0]);
    }
}

static std::vector<uint8_t> medianCutQuantize(const cv::Mat& srcRgba, int K, std::vector<uint8_t>& paletteRgba) {
    int w = srcRgba.cols;
    int h = srcRgba.rows;
    size_t total = static_cast<size_t>(w * h);

    // Extract sample pixels for palette generation
    std::vector<cv::Vec4b> pixels;
    pixels.reserve(std::min(total, size_t(200000)));
    size_t step = std::max(size_t(1), total / 200000);
    bool hasTransparent = false;

    for (size_t i = 0; i < total; i += step) {
        int y = static_cast<int>(i / w);
        int x = static_cast<int>(i % w);
        cv::Vec4b p = srcRgba.at<cv::Vec4b>(y, x);
        if (p[3] < 128) {
            hasTransparent = true;
        } else {
            pixels.push_back(p);
        }
    }

    paletteRgba.clear();
    int targetK = K;
    if (hasTransparent) {
        // Reserve index 0 for transparent
        paletteRgba.insert(paletteRgba.end(), {0, 0, 0, 0});
        targetK = std::max(1, K - 1);
    }

    if (pixels.empty()) {
        paletteRgba.insert(paletteRgba.end(), {0, 0, 0, 255});
    } else {
        std::vector<ColorBox> boxes;
        boxes.emplace_back();
        boxes[0].pixelIndices.resize(pixels.size());
        for (size_t i = 0; i < pixels.size(); ++i) boxes[0].pixelIndices[i] = i;
        updateBoxBounds(boxes[0], pixels);

        while (static_cast<int>(boxes.size()) < targetK) {
            // Find box with greatest spread
            int bestIdx = -1;
            int maxSpread = -1;
            for (size_t i = 0; i < boxes.size(); ++i) {
                int spread = boxes[i].maxDimension();
                if (spread > maxSpread && boxes[i].pixelIndices.size() > 1) {
                    maxSpread = spread;
                    bestIdx = static_cast<int>(i);
                }
            }
            if (bestIdx == -1 || maxSpread == 0) break;

            ColorBox& box = boxes[bestIdx];
            int rSpan = box.maxR - box.minR;
            int gSpan = box.maxG - box.minG;
            int bSpan = box.maxB - box.minB;
            int axis = 2; // R
            if (gSpan >= rSpan && gSpan >= bSpan) axis = 1; // G
            else if (bSpan >= rSpan && bSpan >= gSpan) axis = 0; // B

            std::sort(box.pixelIndices.begin(), box.pixelIndices.end(),
                      [&pixels, axis](size_t a, size_t b) {
                          return pixels[a][axis] < pixels[b][axis];
                      });

            size_t median = box.pixelIndices.size() / 2;
            ColorBox newBox;
            newBox.pixelIndices.assign(box.pixelIndices.begin() + median, box.pixelIndices.end());
            box.pixelIndices.erase(box.pixelIndices.begin() + median, box.pixelIndices.end());

            updateBoxBounds(box, pixels);
            updateBoxBounds(newBox, pixels);
            boxes.push_back(std::move(newBox));
        }

        for (const auto& box : boxes) {
            if (box.pixelIndices.empty()) continue;
            uint64_t sumR = 0, sumG = 0, sumB = 0;
            for (size_t idx : box.pixelIndices) {
                sumR += pixels[idx][2];
                sumG += pixels[idx][1];
                sumB += pixels[idx][0];
            }
            size_t n = box.pixelIndices.size();
            paletteRgba.push_back(static_cast<uint8_t>(sumR / n));
            paletteRgba.push_back(static_cast<uint8_t>(sumG / n));
            paletteRgba.push_back(static_cast<uint8_t>(sumB / n));
            paletteRgba.push_back(255);
        }
    }

    // Floyd-Steinberg error-diffusion dithering
    std::vector<uint8_t> indexed(w * h, 0);
    size_t numPaletteColors = paletteRgba.size() / 4;

    // Fast color finder
    auto findNearest = [&paletteRgba, numPaletteColors](int r, int g, int b, int a) -> uint8_t {
        if (a < 128 && paletteRgba[3] == 0) return 0;
        int bestDist = 1000000;
        uint8_t bestIdx = 0;
        for (size_t i = 0; i < numPaletteColors; ++i) {
            int pr = paletteRgba[i * 4 + 0];
            int pg = paletteRgba[i * 4 + 1];
            int pb = paletteRgba[i * 4 + 2];
            int pa = paletteRgba[i * 4 + 3];
            if (pa == 0) continue;
            int dr = r - pr;
            int dg = g - pg;
            int db = b - pb;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = static_cast<uint8_t>(i);
            }
        }
        return bestIdx;
    };

    // Row buffers for Floyd-Steinberg errors (R, G, B)
    std::vector<std::array<float, 3>> currErr(w + 2, {0.f, 0.f, 0.f});
    std::vector<std::array<float, 3>> nextErr(w + 2, {0.f, 0.f, 0.f});

    for (int y = 0; y < h; ++y) {
        std::fill(nextErr.begin(), nextErr.end(), std::array<float, 3>{0.f, 0.f, 0.f});
        for (int x = 0; x < w; ++x) {
            cv::Vec4b p = srcRgba.at<cv::Vec4b>(y, x);
            if (p[3] < 128 && hasTransparent) {
                indexed[y * w + x] = 0;
                continue;
            }

            float r = std::clamp(p[2] + currErr[x + 1][0], 0.f, 255.f);
            float g = std::clamp(p[1] + currErr[x + 1][1], 0.f, 255.f);
            float b = std::clamp(p[0] + currErr[x + 1][2], 0.f, 255.f);

            uint8_t idx = findNearest(static_cast<int>(r + 0.5f),
                                      static_cast<int>(g + 0.5f),
                                      static_cast<int>(b + 0.5f),
                                      p[3]);
            indexed[y * w + x] = idx;

            float errR = r - paletteRgba[idx * 4 + 0];
            float errG = g - paletteRgba[idx * 4 + 1];
            float errB = b - paletteRgba[idx * 4 + 2];

            // Distribute errors: 7/16 right, 3/16 down-left, 5/16 down, 1/16 down-right
            currErr[x + 2][0] += errR * (7.f / 16.f);
            currErr[x + 2][1] += errG * (7.f / 16.f);
            currErr[x + 2][2] += errB * (7.f / 16.f);

            nextErr[x][0] += errR * (3.f / 16.f);
            nextErr[x][1] += errG * (3.f / 16.f);
            nextErr[x][2] += errB * (3.f / 16.f);

            nextErr[x + 1][0] += errR * (5.f / 16.f);
            nextErr[x + 1][1] += errG * (5.f / 16.f);
            nextErr[x + 1][2] += errB * (5.f / 16.f);

            nextErr[x + 2][0] += errR * (1.f / 16.f);
            nextErr[x + 2][1] += errG * (1.f / 16.f);
            nextErr[x + 2][2] += errB * (1.f / 16.f);
        }
        currErr = nextErr;
    }

    return indexed;
}

// ── PNG Compression ──────────────────────────────────────────────────────────

std::vector<uint8_t> ImageCompressor::pngCompress(cv::Mat& img, size_t targetMaxBytes) {
    if (img.empty()) return {};

    cv::Mat rgba;
    if (img.channels() == 4) {
        rgba = img;
    } else if (img.channels() == 3) {
        cv::cvtColor(img, rgba, cv::COLOR_BGR2BGRA);
    } else {
        cv::cvtColor(img, rgba, cv::COLOR_GRAY2BGRA);
    }

    // Try standard lodepng 32-bit RGBA first if targetMaxBytes == 0
    if (targetMaxBytes == 0) {
        std::vector<uint8_t> out;
        cv::Mat standardRgba;
        cv::cvtColor(rgba, standardRgba, cv::COLOR_BGRA2RGBA);
        lodepng::encode_rgba(out, standardRgba.data, standardRgba.cols, standardRgba.rows);
        return out;
    }

    // Try palette quantisation with K in {256, 128, 64, 32}
    cv::Mat currentRgba = rgba;
    const std::vector<int> kValues = {256, 128, 64, 32};
    std::vector<uint8_t> bestEncoded;

    for (int k : kValues) {
        std::vector<uint8_t> palette;
        std::vector<uint8_t> indexed = medianCutQuantize(currentRgba, k, palette);

        std::vector<uint8_t> encoded;
        if (lodepng::encode_indexed(encoded, indexed.data(), currentRgba.cols, currentRgba.rows, palette) == 0) {
            bestEncoded = encoded;
            if (encoded.size() <= static_cast<size_t>(targetMaxBytes * 0.99)) {
                return encoded;
            }
        }
    }

    // If K=32 still exceeds targetMaxBytes, apply power-law downscaling and re-encode
    while (bestEncoded.size() > targetMaxBytes && currentRgba.cols > 100 && currentRgba.rows > 100) {
        double currBytes = static_cast<double>(bestEncoded.size());
        double scale = std::clamp(std::sqrt(static_cast<double>(targetMaxBytes) / currBytes) * 0.95, 0.15, 0.95);
        if (scale >= 0.98) scale = 0.90;

        int newW = std::max(64, static_cast<int>(currentRgba.cols * scale));
        int newH = std::max(64, static_cast<int>(currentRgba.rows * scale));
        cv::resize(currentRgba, currentRgba, cv::Size(newW, newH), 0, 0, cv::INTER_AREA);

        std::vector<uint8_t> palette;
        std::vector<uint8_t> indexed = medianCutQuantize(currentRgba, 32, palette);
        std::vector<uint8_t> encoded;
        if (lodepng::encode_indexed(encoded, indexed.data(), currentRgba.cols, currentRgba.rows, palette) == 0) {
            bestEncoded = encoded;
            if (encoded.size() <= targetMaxBytes) {
                break;
            }
        } else {
            break;
        }
    }

    return bestEncoded;
}

// ── Binary Search Quality for JPEG / WebP ─────────────────────────────────────

std::vector<uint8_t> ImageCompressor::jpegBinarySearch(cv::Mat& img,
                                                       size_t targetMaxBytes,
                                                       bool isWebP,
                                                       bool lockDimensions) {
    if (img.empty()) return {};

    std::string ext = isWebP ? ".webp" : ".jpg";
    int qualityParam = isWebP ? cv::IMWRITE_WEBP_QUALITY : cv::IMWRITE_JPEG_QUALITY;

    auto encodeAt = [&](cv::Mat& m, int q) -> std::vector<uint8_t> {
        std::vector<uint8_t> buf;
        std::vector<int> params;
        if (!isWebP) {
            params = {qualityParam, q, cv::IMWRITE_JPEG_OPTIMIZE, 1, cv::IMWRITE_JPEG_PROGRESSIVE, 1};
        } else {
            params = {qualityParam, q};
        }
        cv::imencode(ext, m, buf, params);
        return buf;
    };

    if (targetMaxBytes == 0) {
        return encodeAt(img, 85);
    }

    cv::Mat working = img;
    size_t effectiveTarget = static_cast<size_t>(targetMaxBytes * 0.99);
    std::vector<uint8_t> smallestBuf;

    // Progressive scale passes if initial encode at Q=18 is too large
    for (int pass = 0; pass < 4; ++pass) {
        int low = 18;
        int high = 95;
        std::vector<uint8_t> bestBuf;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            auto buf = encodeAt(working, mid);
            if (smallestBuf.empty() || buf.size() < smallestBuf.size()) {
                smallestBuf = buf;
            }
            if (buf.size() <= effectiveTarget) {
                bestBuf = std::move(buf);
                low = mid + 1; // Try higher quality
            } else {
                high = mid - 1; // Needs lower quality
            }
        }

        if (!bestBuf.empty()) {
            return bestBuf;
        }

        // Even at Q=18 it's too large, downscale via closed-form power-law law
        if (lockDimensions) break;

        auto q18 = encodeAt(working, 18);
        if (smallestBuf.empty() || q18.size() < smallestBuf.size()) {
            smallestBuf = q18;
        }
        double currBytes = static_cast<double>(q18.size());
        double scale = std::clamp(std::sqrt(static_cast<double>(effectiveTarget) / currBytes) * 0.95, 0.15, 0.95);
        if (scale >= 0.98) scale = 0.90;

        int newW = std::max(64, static_cast<int>(working.cols * scale));
        int newH = std::max(64, static_cast<int>(working.rows * scale));
        if (newW == working.cols && newH == working.rows) break;

        cv::resize(working, working, cv::Size(newW, newH), 0, 0, cv::INTER_AREA);
    }

    if (!smallestBuf.empty()) {
        return smallestBuf;
    }

    // Final fallback: encode at minimum quality
    return encodeAt(working, 18);
}



// ── Main Compression Execution ────────────────────────────────────────────────

JobResult ImageCompressor::compress(const JobRequest& req,
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

    // Determine output directory and path
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

    // Load decoded image
    bool flattenAlpha = (req.detectedFormat == DetectedFormat::JPEG || req.detectedFormat == DetectedFormat::BMP);
    cv::Mat img = loadDecoded(req.inputFilePath, flattenAlpha);
    if (img.empty()) {
        result.success = false;
        result.errorMessage = "Corrupt image or decompression bomb (>100M pixels).";
        return result;
    }

    if (progress) progress(0.3);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    std::vector<uint8_t> outBytes;
    size_t targetMax = (req.targetBytes > 0) ? static_cast<size_t>(req.targetBytes) : 0;

    switch (req.detectedFormat) {
        case DetectedFormat::PNG:
            outBytes = pngCompress(img, targetMax);
            break;

        case DetectedFormat::JPEG:
            outBytes = jpegBinarySearch(img, targetMax, false);
            break;

        case DetectedFormat::WebP:
            outBytes = jpegBinarySearch(img, targetMax, true);
            break;

        case DetectedFormat::BMP: {
            if (targetMax > 0 && static_cast<size_t>(img.total() * img.elemSize() + 54) > targetMax) {
                double currBytes = static_cast<double>(img.total() * img.elemSize() + 54);
                double scale = std::clamp(std::sqrt(static_cast<double>(targetMax) / currBytes) * 0.95, 0.15, 0.95);
                cv::resize(img, img, cv::Size(), scale, scale, cv::INTER_AREA);
            }
            cv::imencode(".bmp", img, outBytes);
            break;
        }

        case DetectedFormat::TIFF: {
            std::vector<int> params = {cv::IMWRITE_TIFF_COMPRESSION, 5}; // LZW
            if (targetMax > 0) {
                // Downscale if needed
                cv::imencode(".tiff", img, outBytes, params);
                if (outBytes.size() > targetMax) {
                    double currBytes = static_cast<double>(outBytes.size());
                    double scale = std::clamp(std::sqrt(static_cast<double>(targetMax) / currBytes) * 0.95, 0.15, 0.95);
                    cv::resize(img, img, cv::Size(), scale, scale, cv::INTER_AREA);
                    cv::imencode(".tiff", img, outBytes, params);
                }
            } else {
                cv::imencode(".tiff", img, outBytes, params);
            }
            break;
        }

        default:
            outBytes = jpegBinarySearch(img, targetMax, false);
            break;
    }

    if (progress) progress(0.8);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    if (outBytes.empty()) {
        result.success = false;
        result.errorMessage = "Failed to encode output image.";
        return result;
    }

    // Privacy Metadata Stripping: ensure EXIF GPS, IPTC, and comment chunks are stripped
    if (req.detectedFormat == DetectedFormat::JPEG) {
        SecurityGuards::stripJpegPrivacyMetadata(outBytes);
    }

    // Secure Atomic Write via SecureTempFile
    SecureTempFile tempFile(outDir, ext);
    if (!tempFile.isValid()) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    std::ofstream outStream(tempFile.path(), std::ios::binary);
    if (!outStream.is_open()) {
        result.success = false;
        result.errorMessage = "Failed to open temporary file for writing.";
        return result;
    }
    outStream.write(reinterpret_cast<const char*>(outBytes.data()), outBytes.size());
    outStream.close();

    if (!tempFile.commit(outPath, FormatDetector::formatName(req.detectedFormat))) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<int64_t>(outBytes.size());
    result.outputWidth = img.cols;
    result.outputHeight = img.rows;
    result.outputFormat = FormatDetector::formatName(req.detectedFormat);

    // Memory Hygiene: immediately release large buffers
    img.release();
    outBytes.clear();
    outBytes.shrink_to_fit();

    if (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes) {
        result.hitFloor = true;
        result.floorNote = "Reached maximum compression floor while maintaining visual legibility.";
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
