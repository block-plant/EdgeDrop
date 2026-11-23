#include "ImageProcessor.hpp"
#include "JfifDpiPatcher.hpp"
#include "SignatureCleaner.hpp"
#include "PortraitEnhancer.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace EdgeDrop {

cv::Mat ImageProcessor::loadAndNormalize(const std::string& filePath, TargetFormat targetFormat) {
    cv::Mat img = cv::imread(filePath, cv::IMREAD_UNCHANGED);
    if (img.empty()) return cv::Mat();

    // If 4-channel BGRA and target is JPG, composite onto pure white
    if (img.channels() == 4) {
        if (targetFormat == TargetFormat::JPG) {
            cv::Mat bgr(img.rows, img.cols, CV_8UC3, cv::Scalar(255, 255, 255));
            for (int y = 0; y < img.rows; ++y) {
                const cv::Vec4b* srcRow = img.ptr<cv::Vec4b>(y);
                cv::Vec3b* dstRow = bgr.ptr<cv::Vec3b>(y);
                for (int x = 0; x < img.cols; ++x) {
                    float alpha = srcRow[x][3] / 255.0f;
                    dstRow[x][0] = static_cast<uint8_t>(srcRow[x][0] * alpha + 255.0f * (1.0f - alpha));
                    dstRow[x][1] = static_cast<uint8_t>(srcRow[x][1] * alpha + 255.0f * (1.0f - alpha));
                    dstRow[x][2] = static_cast<uint8_t>(srcRow[x][2] * alpha + 255.0f * (1.0f - alpha));
                }
            }
            return bgr;
        }
    } else if (img.channels() == 1) {
        // Convert grayscale to 3-channel BGR for unified processing
        cv::Mat bgr;
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
        return bgr;
    }

    return img;
}

cv::Mat ImageProcessor::resizeCoverCrop(const cv::Mat& src, int targetW, int targetH, const cv::Point* centerOfFocus) {
    if (src.empty() || targetW <= 0 || targetH <= 0) return src;

    double targetAspect = static_cast<double>(targetW) / targetH;
    double srcAspect = static_cast<double>(src.cols) / src.rows;

    int cropW, cropH;
    int cropX, cropY;

    if (srcAspect > targetAspect) {
        // Source is wider: crop sides
        cropH = src.rows;
        cropW = static_cast<int>(std::round(cropH * targetAspect));
        if (centerOfFocus) {
            cropX = centerOfFocus->x - cropW / 2;
        } else {
            cropX = (src.cols - cropW) / 2;
        }
        cropY = 0;
    } else {
        // Source is taller: crop top/bottom
        cropW = src.cols;
        cropH = static_cast<int>(std::round(cropW / targetAspect));
        cropX = 0;
        if (centerOfFocus) {
            cropY = centerOfFocus->y - cropH / 2;
        } else {
            cropY = (src.rows - cropH) / 2;
        }
    }

    cropX = std::clamp(cropX, 0, src.cols - cropW);
    cropY = std::clamp(cropY, 0, src.rows - cropH);
    cropW = std::clamp(cropW, 1, src.cols - cropX);
    cropH = std::clamp(cropH, 1, src.rows - cropY);

    cv::Mat cropped = src(cv::Rect(cropX, cropY, cropW, cropH));
    cv::Mat resized;

    if (targetW < cropped.cols || targetH < cropped.rows) {
        cv::resize(cropped, resized, cv::Size(targetW, targetH), 0, 0, cv::INTER_AREA);
    } else {
        cv::resize(cropped, resized, cv::Size(targetW, targetH), 0, 0, cv::INTER_LANCZOS4);
    }

    return resized;
}

cv::Mat ImageProcessor::quantizeColors(const cv::Mat& src, int maxColors) {
    if (src.empty() || maxColors <= 0 || maxColors >= 256) return src;

    cv::Mat data;
    src.reshape(1, src.rows * src.cols).convertTo(data, CV_32F);

    cv::Mat labels, centers;
    cv::kmeans(data, maxColors, labels,
               cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0),
               3, cv::KMEANS_PP_CENTERS, centers);

    centers.convertTo(centers, CV_8U);
    cv::Mat quantized(src.size(), src.type());
    cv::Mat resultFlat = quantized.reshape(1, src.rows * src.cols);

    for (int i = 0; i < src.rows * src.cols; ++i) {
        int clusterIdx = labels.at<int>(i);
        for (int c = 0; c < src.channels(); ++c) {
            resultFlat.at<uint8_t>(i, c) = centers.at<uint8_t>(clusterIdx, c);
        }
    }

    return quantized;
}

std::vector<uint8_t> ImageProcessor::compressToTargetSize(cv::Mat& img,
                                                          TargetFormat format,
                                                          size_t minBytes,
                                                          size_t maxBytes,
                                                          int dpi,
                                                          bool lockDimensions) {
    std::vector<uint8_t> buffer;
    if (img.empty()) return buffer;

    // Safety margin 3% for maxBytes
    size_t targetMaxSafe = (maxBytes > 0) ? static_cast<size_t>(maxBytes * 0.97) : 0;

    std::string ext = (format == TargetFormat::PNG) ? ".png" :
                      (format == TargetFormat::WebP) ? ".webp" : ".jpg";

    if (format == TargetFormat::PNG) {
        // PNG Mode: try IMWRITE_PNG_COMPRESSION = 9
        std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 9};
        cv::imencode(".png", img, buffer, params);

        // If over maxBytes and dimensions unlocked, scale progressively
        int scaleAttempts = 0;
        cv::Mat workImg = img;
        while (targetMaxSafe > 0 && buffer.size() > targetMaxSafe && !lockDimensions && scaleAttempts < 8) {
            cv::resize(workImg, workImg, cv::Size(), 0.90, 0.90, cv::INTER_AREA);
            cv::imencode(".png", workImg, buffer, params);
            scaleAttempts++;
        }

        // If still over targetMaxSafe, quantize colors
        if (targetMaxSafe > 0 && buffer.size() > targetMaxSafe) {
            for (int colors : {256, 128, 64, 32}) {
                cv::Mat q = quantizeColors(workImg, colors);
                std::vector<uint8_t> qBuf;
                cv::imencode(".png", q, qBuf, params);
                if (qBuf.size() <= targetMaxSafe || colors == 32) {
                    buffer = std::move(qBuf);
                    img = q;
                    break;
                }
            }
        }

        // Tag PNG DPI
        JfifDpiPatcher::patchPngDpi(buffer, dpi);
        return buffer;
    }

    // JPG or WebP: Binary Search on quality
    int paramId = (format == TargetFormat::WebP) ? cv::IMWRITE_WEBP_QUALITY : cv::IMWRITE_JPEG_QUALITY;
    cv::Mat workImg = img;

    auto tryEncode = [&](int quality) -> std::vector<uint8_t> {
        std::vector<uint8_t> buf;
        std::vector<int> params;
        params.push_back(paramId);
        params.push_back(quality);
        if (format == TargetFormat::JPG) {
            params.push_back(cv::IMWRITE_JPEG_OPTIMIZE);
            params.push_back(1);
        }
        cv::imencode(ext, workImg, buf, params);
        return buf;
    };

    if (targetMaxSafe == 0) {
        // No ceiling: default to high quality 90
        buffer = tryEncode(90);
    } else {
        // Binary search for highest quality where buf.size() <= targetMaxSafe
        int low = 10, high = 98;
        int bestQ = -1;
        std::vector<uint8_t> bestBuf;

        for (int iter = 0; iter < 7 && low <= high; ++iter) {
            int mid = low + (high - low) / 2;
            std::vector<uint8_t> current = tryEncode(mid);
            if (current.size() <= targetMaxSafe) {
                bestQ = mid;
                bestBuf = std::move(current);
                low = mid + 1; // Try higher quality
            } else {
                high = mid - 1; // Try lower quality
            }
        }

        if (bestQ != -1) {
            buffer = std::move(bestBuf);
        } else {
            // Even q=10 exceeds targetMaxSafe!
            if (!lockDimensions) {
                // Progressively downscale dimensions by 0.90x
                int scaleStep = 0;
                while (scaleStep < 10 && (bestQ == -1)) {
                    scaleStep++;
                    cv::resize(workImg, workImg, cv::Size(), 0.90, 0.90, cv::INTER_AREA);
                    // Retry search
                    low = 10; high = 95;
                    for (int iter = 0; iter < 7 && low <= high; ++iter) {
                        int mid = low + (high - low) / 2;
                        std::vector<uint8_t> current = tryEncode(mid);
                        if (current.size() <= targetMaxSafe) {
                            bestQ = mid;
                            bestBuf = std::move(current);
                            low = mid + 1;
                        } else {
                            high = mid - 1;
                        }
                    }
                }
                buffer = bestBuf.empty() ? tryEncode(10) : std::move(bestBuf);
                img = workImg;
            } else {
                // Dimensions locked: apply mild Gaussian blur (sigma=0.5) to remove high-frequency noise
                cv::GaussianBlur(workImg, workImg, cv::Size(3, 3), 0.5);
                buffer = tryEncode(10);
            }
        }
    }

    // Min-KB Floor Protection
    if (minBytes > 0 && buffer.size() < minBytes) {
        // Step quality up to 100
        std::vector<uint8_t> maxQBuf = tryEncode(100);
        if (maxQBuf.size() >= minBytes && (targetMaxSafe == 0 || maxQBuf.size() <= targetMaxSafe)) {
            buffer = std::move(maxQBuf);
        }

        // If still under minBytes and output is JPG, append harmless JFIF COM padding
        if (buffer.size() < minBytes && format == TargetFormat::JPG) {
            JfifDpiPatcher::padJpegToMinBytes(buffer, minBytes + 256);
        }
    }

    // Tag exact DPI
    if (format == TargetFormat::JPG) {
        JfifDpiPatcher::patchJpegDpi(buffer, dpi);
    }

    return buffer;
}

JobResult ImageProcessor::processImage(const JobRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = request.inputFilePath;

    if (!std::filesystem::exists(request.inputFilePath)) {
        result.success = false;
        result.errorMessage = "Input file does not exist: " + request.inputFilePath.string();
        return result;
    }

    result.inputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(request.inputFilePath));

    // 1. Load image and normalize
    cv::Mat img = loadAndNormalize(request.inputFilePath.string(), request.targetFormat);
    if (img.empty()) {
        result.success = false;
        result.errorMessage = "Failed to decode image: " + request.inputFilePath.string();
        return result;
    }

    // 2. Category-specific processing
    if (request.category == FileCategory::Signature) {
        // Clean signature: remove shadow, whiten paper, tight ink crop
        img = SignatureCleaner::cleanSignature(img, request.targetWidth, request.targetHeight, request.greyscale);
    } else if (request.category == FileCategory::Photo) {
        if (request.autoCrop || request.whitenBackground) {
            img = PortraitEnhancer::enhancePortrait(img,
                                                   request.targetWidth,
                                                   request.targetHeight,
                                                   request.faceRatio,
                                                   request.whitenBackground);
        } else if (request.targetWidth > 0 && request.targetHeight > 0) {
            img = resizeCoverCrop(img, request.targetWidth, request.targetHeight);
        }
    } else {
        // Document / General
        if (request.targetWidth > 0 && request.targetHeight > 0) {
            img = resizeCoverCrop(img, request.targetWidth, request.targetHeight);
        } else if (request.maxLongEdge > 0) {
            int longEdge = std::max(img.cols, img.rows);
            if (longEdge > request.maxLongEdge) {
                double scale = static_cast<double>(request.maxLongEdge) / longEdge;
                cv::resize(img, img, cv::Size(), scale, scale, cv::INTER_AREA);
            }
        }
    }

    // Greyscale conversion if requested and not already done
    if (request.greyscale && img.channels() > 1) {
        cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
        cv::cvtColor(img, img, cv::COLOR_GRAY2BGR); // keep 3-channel for format encoding consistency
    }

    // 3. Compress to target size
    size_t minBytes = request.minKb > 0 ? static_cast<size_t>(request.minKb) * 1024 : 0;
    size_t maxBytes = request.maxKb > 0 ? static_cast<size_t>(request.maxKb) * 1024 : 0;
    bool lockDimensions = (request.targetWidth > 0 && request.targetHeight > 0);

    std::vector<uint8_t> encodedData = compressToTargetSize(
        img,
        request.targetFormat,
        minBytes,
        maxBytes,
        request.dpi,
        lockDimensions
    );

    if (encodedData.empty()) {
        result.success = false;
        result.errorMessage = "Failed to encode image to target specifications.";
        return result;
    }

    // 4. Generate unique output path
    auto outDir = request.outputDirectory.empty()
        ? (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads")
        : request.outputDirectory;
    std::filesystem::create_directories(outDir);

    std::string stem = request.customFileName.empty() ? request.inputFilePath.stem().string() : request.customFileName;
    std::string presetOrFmt = !request.presetId.empty() ? request.presetId : formatToString(request.targetFormat);
    std::transform(presetOrFmt.begin(), presetOrFmt.end(), presetOrFmt.begin(), ::tolower);
    int finalKb = static_cast<int>(std::round(static_cast<double>(encodedData.size()) / 1024.0));
    std::string ext = (request.targetFormat == TargetFormat::PNG) ? ".png" :
                      (request.targetFormat == TargetFormat::WebP) ? ".webp" : ".jpg";

    std::filesystem::path fullPath;
    int counter = 0;
    do {
        std::string sfx = (counter == 0) ? "" : " (" + std::to_string(counter) + ")";
        fullPath = outDir / (stem + "_" + presetOrFmt + "_" + std::to_string(finalKb) + "kb" + sfx + ext);
        counter++;
    } while (std::filesystem::exists(fullPath));

    // Write output file
    std::ofstream outFile(fullPath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
        result.success = false;
        result.errorMessage = "Failed to open output file for writing: " + fullPath.string();
        return result;
    }
    outFile.write(reinterpret_cast<const char*>(encodedData.data()), encodedData.size());
    outFile.close();

    result.success = true;
    result.outputFilePath = fullPath;
    result.outputSizeBytes = static_cast<int64_t>(encodedData.size());
    result.outputDpi = request.dpi;
    result.outputFormat = formatToString(request.targetFormat);
    result.outputWidth = img.cols;
    result.outputHeight = img.rows;

    auto endTime = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    return result;
}

} // namespace EdgeDrop
