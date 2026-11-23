#include "ImageProcessor.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cassert>
#include <iostream>
#include <cmath>

int main() {
    std::cout << "[RUN] Testing Binary Search Compressor..." << std::endl;

    // 1. Synthetic Flat Image (500x500 smooth gradient)
    cv::Mat flatImg(500, 500, CV_8UC3);
    for (int y = 0; y < flatImg.rows; ++y) {
        for (int x = 0; x < flatImg.cols; ++x) {
            flatImg.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<uint8_t>((x * 255) / flatImg.cols),
                static_cast<uint8_t>((y * 255) / flatImg.rows),
                200
            );
        }
    }

    // Target: Max 50 KB ceiling, Min 5 KB floor
    size_t minBytes = 5 * 1024;
    size_t maxBytes = 50 * 1024;

    std::vector<uint8_t> encodedFlat = EdgeDrop::ImageProcessor::compressToTargetSize(
        flatImg,
        EdgeDrop::TargetFormat::JPG,
        minBytes,
        maxBytes,
        200,
        true
    );

    std::cout << "Flat Image Encoded Size: " << encodedFlat.size() << " bytes (Target: "
              << minBytes << " - " << maxBytes << " bytes)" << std::endl;
    assert(!encodedFlat.empty());
    assert(encodedFlat.size() <= maxBytes);
    assert(encodedFlat.size() >= minBytes);

    // 2. Synthetic High-Frequency Noisy Image (800x800 random noise)
    cv::Mat noisyImg(800, 800, CV_8UC3);
    cv::randu(noisyImg, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));

    // Target: Max 100 KB ceiling with unlocked dimensions
    size_t noisyMaxBytes = 100 * 1024;
    std::vector<uint8_t> encodedNoisy = EdgeDrop::ImageProcessor::compressToTargetSize(
        noisyImg,
        EdgeDrop::TargetFormat::JPG,
        0,
        noisyMaxBytes,
        300,
        false // dimensions unlocked -> can downscale if needed
    );

    std::cout << "Noisy Image Encoded Size: " << encodedNoisy.size() << " bytes (Target: <= "
              << noisyMaxBytes << " bytes)" << std::endl;
    assert(!encodedNoisy.empty());
    assert(encodedNoisy.size() <= noisyMaxBytes);

    std::cout << "[PASS] Binary Search Compressor test passed successfully!" << std::endl;
    return 0;
}
