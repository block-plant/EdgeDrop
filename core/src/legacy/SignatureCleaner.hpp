#pragma once

#include <opencv2/core.hpp>

namespace EdgeDrop {

class SignatureCleaner {
public:
    // Cleans signature: removes uneven shadows, whitens paper to pure #FFFFFF,
    // preserves anti-aliased ink strokes, crops tightly to ink with padding.
    // targetWidth/targetHeight can be 0 (preserve natural aspect ratio).
    static cv::Mat cleanSignature(const cv::Mat& inputImg,
                                  int targetWidth = 0,
                                  int targetHeight = 0,
                                  bool greyscale = false);

    // Helper: Finds tight bounding box around ink pixels, filtering tiny dust particles
    static cv::Rect findInkBoundingBox(const cv::Mat& grayNorm, double inkThreshold = 225.0);
};

} // namespace EdgeDrop
