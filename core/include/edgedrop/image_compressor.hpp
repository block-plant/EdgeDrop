#pragma once

#include "interfaces.hpp"
#include <opencv2/core.hpp>
#include <vector>
#include <cstdint>
#include <memory>

namespace EdgeDrop {

class ImageCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;

    // ── Low-level compression helpers ─────────────────────────────────────────

    // Binary-search quality compression for JPEG / WebP
    static std::vector<uint8_t> jpegBinarySearch(cv::Mat& img,
                                                 size_t targetMaxBytes,
                                                 bool isWebP = false,
                                                 bool lockDimensions = false);

    // Indexed PNG compression using lodepng + median cut quantization
    static std::vector<uint8_t> pngCompress(cv::Mat& img, size_t targetMaxBytes);

    // K-means color quantization helper
    static cv::Mat quantizeColors(const cv::Mat& src, int maxColors);

    // Load image from filesystem via binary buffer
    static cv::Mat loadDecoded(const std::filesystem::path& filePath, bool flattenAlpha = false);

    // Pluggable native codec (e.g. for macOS ImageIO HEIC)
    static void setNativeCodec(std::shared_ptr<INativeImageCodec> codec);
};

} // namespace EdgeDrop
