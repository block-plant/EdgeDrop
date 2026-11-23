#pragma once

#include "ProcessingTypes.hpp"
#include <opencv2/core.hpp>
#include <vector>
#include <string>

namespace EdgeDrop {

class ImageProcessor {
public:
    // Main image processing pipeline for JobRequest
    static JobResult processImage(const JobRequest& request);

    // In-memory binary search compressor for target size bounds [minBytes, maxBytes]
    // Returns encoded byte buffer.
    static std::vector<uint8_t> compressToTargetSize(cv::Mat& img,
                                                     TargetFormat format,
                                                     size_t minBytes,
                                                     size_t maxBytes,
                                                     int dpi,
                                                     bool lockDimensions = true);

    // Helpers
    static cv::Mat loadAndNormalize(const std::string& filePath, TargetFormat targetFormat);
    static cv::Mat resizeCoverCrop(const cv::Mat& src, int targetW, int targetH, const cv::Point* centerOfFocus = nullptr);
    static cv::Mat quantizeColors(const cv::Mat& src, int maxColors);
};

} // namespace EdgeDrop
