#include "SignatureCleaner.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <cassert>
#include <iostream>

int main() {
    std::cout << "[RUN] Testing Signature Cleaner..." << std::endl;

    // Create a 400x200 synthetic image simulating a phone photo of a signature:
    // - Shadow gradient across the paper: from light gray (210) to darker gray (140)
    // - Dark blue ink strokes in the center (RGB 20, 30, 80)
    cv::Mat photo(200, 400, CV_8UC3);
    for (int y = 0; y < photo.rows; ++y) {
        for (int x = 0; x < photo.cols; ++x) {
            // Shadow gradient across diagonal
            uint8_t paperShade = static_cast<uint8_t>(140 + (x + y) * 70 / (photo.cols + photo.rows));
            photo.at<cv::Vec3b>(y, x) = cv::Vec3b(paperShade - 10, paperShade, paperShade + 5); // yellowish paper
        }
    }

    // Draw ink strokes in the center
    cv::line(photo, cv::Point(80, 100), cv::Point(200, 80), cv::Scalar(80, 30, 20), 4, cv::LINE_AA);
    cv::line(photo, cv::Point(200, 80), cv::Point(320, 120), cv::Scalar(80, 30, 20), 4, cv::LINE_AA);
    cv::circle(photo, cv::Point(150, 130), 12, cv::Scalar(90, 40, 25), -1, cv::LINE_AA);

    // Clean signature
    cv::Mat cleaned = EdgeDrop::SignatureCleaner::cleanSignature(photo, 400, 200, false);
    assert(!cleaned.empty());
    assert(cleaned.cols == 400 && cleaned.rows == 200);

    // Verify paper pixels outside ink area snapped to pure white #FFFFFF (255, 255, 255)
    cv::Vec3b cornerPixel = cleaned.at<cv::Vec3b>(5, 5);
    std::cout << "Cleaned paper corner pixel: (" << (int)cornerPixel[0] << ", "
              << (int)cornerPixel[1] << ", " << (int)cornerPixel[2] << ")" << std::endl;
    assert(cornerPixel[0] == 255 && cornerPixel[1] == 255 && cornerPixel[2] == 255);

    // Verify ink stroke was preserved (center pixel of drawn circle is dark)
    // Find darkest pixel in cleaned image
    int minVal = 255;
    for (int y = 0; y < cleaned.rows; ++y) {
        for (int x = 0; x < cleaned.cols; ++x) {
            cv::Vec3b p = cleaned.at<cv::Vec3b>(y, x);
            int lum = (p[0] + p[1] + p[2]) / 3;
            if (lum < minVal) minVal = lum;
        }
    }
    std::cout << "Cleaned ink darkest pixel luminance: " << minVal << " (Expected < 100)" << std::endl;
    assert(minVal < 100);

    std::cout << "[PASS] Signature Cleaner test passed successfully!" << std::endl;
    return 0;
}
