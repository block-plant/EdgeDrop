#pragma once

#include "ICompressor.hpp"
#include "ProcessingTypes.hpp"
#include <opencv2/core.hpp>
#include <vector>
#include <cstdint>

namespace EdgeDrop {

class ImageCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;

    // ── Reusable low-level helpers (also used by legacy tests) ──────────────

    // Binary-search quality compression for JPEG / WebP.
    // Returns highest-quality encode ≤ targetMaxBytes (97% safety margin applied).
    // If even q=15 exceeds limit, progressively downscales (lockDimensions=false)
    // or applies mild blur (lockDimensions=true).
    static std::vector<uint8_t> jpegBinarySearch(cv::Mat& img,
                                                  size_t targetMaxBytes,
                                                  bool isWebP = false,
                                                  bool lockDimensions = false);

    // PNG: max zlib compression, then palette quantisation, then gentle scaling.
    static std::vector<uint8_t> pngCompress(cv::Mat& img, size_t targetMaxBytes);

    // Quantise colors (k-means) for PNG palette reduction.
    static cv::Mat quantizeColors(const cv::Mat& src, int maxColors);

    // Load image from file, apply EXIF orientation, flatten alpha for JPEG.
    static cv::Mat loadDecoded(const QString& filePath, bool flattenAlpha = false);
};

} // namespace EdgeDrop
