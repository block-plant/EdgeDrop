#include "PortraitEnhancer.hpp"
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace EdgeDrop {

cv::Rect PortraitEnhancer::detectFace(const cv::Mat& img, const std::string& /*modelPath*/) {
    if (img.empty()) return cv::Rect();

    // Heuristic face positioning: Upper-third center of image
    int heurW = static_cast<int>(img.cols * 0.35);
    int heurH = static_cast<int>(img.rows * 0.35);
    int heurX = (img.cols - heurW) / 2;
    int heurY = static_cast<int>(img.rows * 0.15);
    return cv::Rect(heurX, heurY, heurW, heurH);
}

cv::Rect PortraitEnhancer::computeFaceCropRect(int imgW, int imgH,
                                              const cv::Rect& faceBox,
                                              int targetW, int targetH,
                                              double faceRatio) {
    if (imgW <= 0 || imgH <= 0) return cv::Rect(0, 0, 0, 0);

    double aspect = (targetW > 0 && targetH > 0) ? static_cast<double>(targetW) / targetH
                                                 : static_cast<double>(imgW) / imgH;

    int cropW, cropH;
    int cropX, cropY;

    if (faceBox.width > 0 && faceBox.height > 0) {
        // We want the face height to be approximately faceRatio of the cropped height
        double desiredCropH = static_cast<double>(faceBox.height) / std::clamp(faceRatio, 0.4, 0.95);
        double desiredCropW = desiredCropH * aspect;

        // If desired crop exceeds image dimensions, scale down to fit
        if (desiredCropW > imgW || desiredCropH > imgH) {
            double scaleW = static_cast<double>(imgW) / desiredCropW;
            double scaleH = static_cast<double>(imgH) / desiredCropH;
            double fitScale = std::min(scaleW, scaleH);
            desiredCropW *= fitScale;
            desiredCropH *= fitScale;
        }

        cropW = std::max(10, static_cast<int>(std::round(desiredCropW)));
        cropH = std::max(10, static_cast<int>(std::round(desiredCropH)));

        // Position face in upper-mid portion: headroom is ~12% of crop height
        int faceCenterX = faceBox.x + faceBox.width / 2;
        cropX = faceCenterX - cropW / 2;
        cropY = faceBox.y - static_cast<int>(cropH * 0.12);

        // Clamp inside image bounds
        if (cropX < 0) cropX = 0;
        if (cropY < 0) cropY = 0;
        if (cropX + cropW > imgW) cropX = imgW - cropW;
        if (cropY + cropH > imgH) cropY = imgH - cropH;
    } else {
        // Fallback: maximum aspect-preserving crop anchored near upper third
        if (static_cast<double>(imgW) / imgH > aspect) {
            cropH = imgH;
            cropW = static_cast<int>(std::round(cropH * aspect));
            cropX = (imgW - cropW) / 2;
            cropY = 0;
        } else {
            cropW = imgW;
            cropH = static_cast<int>(std::round(cropW / aspect));
            cropX = 0;
            cropY = static_cast<int>((imgH - cropH) * 0.15); // slightly upper bias
        }
    }

    cropX = std::clamp(cropX, 0, imgW - 1);
    cropY = std::clamp(cropY, 0, imgH - 1);
    cropW = std::clamp(cropW, 1, imgW - cropX);
    cropH = std::clamp(cropH, 1, imgH - cropY);

    return cv::Rect(cropX, cropY, cropW, cropH);
}

cv::Mat PortraitEnhancer::whitenPortraitBackground(const cv::Mat& img, const cv::Rect& faceBox) {
    if (img.empty()) return img;

    cv::Mat bgr = img;
    if (img.channels() == 4) {
        cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
    } else if (img.channels() == 1) {
        cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
    }

    // Downscale to max 400px for speed
    double maxDim = std::max(bgr.cols, bgr.rows);
    double scale = 1.0;
    cv::Mat smallImg = bgr;
    if (maxDim > 400.0) {
        scale = 400.0 / maxDim;
        cv::resize(bgr, smallImg, cv::Size(), scale, scale, cv::INTER_AREA);
    }

    cv::Rect smallFace(
        static_cast<int>(faceBox.x * scale),
        static_cast<int>(faceBox.y * scale),
        static_cast<int>(faceBox.width * scale),
        static_cast<int>(faceBox.height * scale)
    );

    // Build GrabCut mask: GC_BGD=0, GC_FGD=1, GC_PR_BGD=2, GC_PR_FGD=3
    cv::Mat mask(smallImg.rows, smallImg.cols, CV_8UC1, cv::Scalar(cv::GC_PR_BGD));

    // Mark corners as definite background
    int cornerW = std::max(10, smallImg.cols / 8);
    int cornerH = std::max(10, smallImg.rows / 8);
    mask(cv::Rect(0, 0, cornerW, cornerH)).setTo(cv::GC_BGD);
    mask(cv::Rect(smallImg.cols - cornerW, 0, cornerW, cornerH)).setTo(cv::GC_BGD);

    if (smallFace.width > 0 && smallFace.height > 0) {
        // Face area is definite foreground
        int fX = std::clamp(smallFace.x + smallFace.width / 6, 0, smallImg.cols - 1);
        int fY = std::clamp(smallFace.y + smallFace.height / 6, 0, smallImg.rows - 1);
        int fW = std::clamp(static_cast<int>(smallFace.width * 0.65), 1, smallImg.cols - fX);
        int fH = std::clamp(static_cast<int>(smallFace.height * 0.65), 1, smallImg.rows - fY);
        mask(cv::Rect(fX, fY, fW, fH)).setTo(cv::GC_FGD);

        // Torso/shoulders below face is probable foreground
        int torsoX = std::max(0, smallFace.x - smallFace.width / 2);
        int torsoY = smallFace.y + smallFace.height;
        int torsoW = std::min(smallImg.cols - torsoX, smallFace.width * 2);
        int torsoH = std::min(smallImg.rows - torsoY, smallFace.height * 2);
        if (torsoW > 0 && torsoH > 0 && torsoY < smallImg.rows) {
            mask(cv::Rect(torsoX, torsoY, torsoW, torsoH)).setTo(cv::GC_PR_FGD);
        }
    } else {
        // Heuristic center region as probable foreground
        int cX = smallImg.cols / 4;
        int cY = smallImg.rows / 6;
        int cW = smallImg.cols / 2;
        int cH = static_cast<int>(smallImg.rows * 0.7);
        mask(cv::Rect(cX, cY, cW, cH)).setTo(cv::GC_PR_FGD);
    }

    // Run 2 iterations of grabCut
    cv::Mat bgdModel, fgdModel;
    try {
        cv::grabCut(smallImg, mask, cv::Rect(), bgdModel, fgdModel, 2, cv::GC_INIT_WITH_MASK);
    } catch (...) {
        // On error, return original
        return bgr;
    }

    // Extract foreground mask
    cv::Mat fgMaskSmall = (mask == cv::GC_FGD) | (mask == cv::GC_PR_FGD);

    // Upscale mask to original dimensions
    cv::Mat fgMaskFull;
    cv::resize(fgMaskSmall, fgMaskFull, bgr.size(), 0, 0, cv::INTER_LINEAR);

    // Feather edges with Gaussian blur (sigma ~ 1.5 - 2.0)
    cv::Mat alphaMat;
    fgMaskFull.convertTo(alphaMat, CV_32FC1, 1.0 / 255.0);
    cv::GaussianBlur(alphaMat, alphaMat, cv::Size(5, 5), 1.5);

    // Composite over pure white
    cv::Mat result(bgr.rows, bgr.cols, CV_8UC3);
    for (int y = 0; y < bgr.rows; ++y) {
        const cv::Vec3b* srcRow = bgr.ptr<cv::Vec3b>(y);
        const float* aRow = alphaMat.ptr<float>(y);
        cv::Vec3b* dstRow = result.ptr<cv::Vec3b>(y);

        for (int x = 0; x < bgr.cols; ++x) {
            float a = aRow[x];
            dstRow[x][0] = static_cast<uint8_t>(std::clamp(srcRow[x][0] * a + 255.0f * (1.0f - a), 0.0f, 255.0f));
            dstRow[x][1] = static_cast<uint8_t>(std::clamp(srcRow[x][1] * a + 255.0f * (1.0f - a), 0.0f, 255.0f));
            dstRow[x][2] = static_cast<uint8_t>(std::clamp(srcRow[x][2] * a + 255.0f * (1.0f - a), 0.0f, 255.0f));
        }
    }

    return result;
}

cv::Mat PortraitEnhancer::enhancePortrait(const cv::Mat& inputImg,
                                         int targetWidth,
                                         int targetHeight,
                                         double faceRatio,
                                         bool whitenBackground,
                                         const std::string& modelPath) {
    if (inputImg.empty()) return cv::Mat();

    // 1. Detect face
    cv::Rect faceBox = detectFace(inputImg, modelPath);

    // 2. Crop rectangle
    cv::Rect cropRect = computeFaceCropRect(inputImg.cols, inputImg.rows, faceBox, targetWidth, targetHeight, faceRatio);
    cv::Mat cropped = inputImg(cropRect).clone();

    // Re-adjust face box relative to cropped frame
    cv::Rect relFace(faceBox.x - cropRect.x, faceBox.y - cropRect.y, faceBox.width, faceBox.height);

    // 3. Optional background whitening
    if (whitenBackground) {
        cropped = whitenPortraitBackground(cropped, relFace);
    }

    // 4. Resize if target dimensions specified
    if (targetWidth > 0 && targetHeight > 0) {
        cv::Mat resized;
        if (targetWidth < cropped.cols || targetHeight < cropped.rows) {
            cv::resize(cropped, resized, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_AREA);
        } else {
            cv::resize(cropped, resized, cv::Size(targetWidth, targetHeight), 0, 0, cv::INTER_LANCZOS4);
        }
        return resized;
    }

    return cropped;
}

} // namespace EdgeDrop
