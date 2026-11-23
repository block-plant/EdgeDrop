#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace EdgeDrop {

class PortraitEnhancer {
public:
    // Performs face-aware framing/crop and optional pure white background whitening
    static cv::Mat enhancePortrait(const cv::Mat& inputImg,
                                   int targetWidth,
                                   int targetHeight,
                                   double faceRatio = 0.70,
                                   bool whitenBackground = false,
                                   const std::string& modelPath = "");

    // Detects primary face bounding box. Returns cv::Rect() if none found.
    static cv::Rect detectFace(const cv::Mat& img, const std::string& modelPath = "");

    // Computes target crop box centered around face (or upper third fallback)
    static cv::Rect computeFaceCropRect(int imgW, int imgH,
                                        const cv::Rect& faceBox,
                                        int targetW, int targetH,
                                        double faceRatio = 0.70);

    // Whitens the background offline using trimap + grabCut + edge feathering
    static cv::Mat whitenPortraitBackground(const cv::Mat& img, const cv::Rect& faceBox);
};

} // namespace EdgeDrop
