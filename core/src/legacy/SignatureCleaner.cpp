#include "SignatureCleaner.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

namespace EdgeDrop {

cv::Rect SignatureCleaner::findInkBoundingBox(const cv::Mat& grayNorm, double inkThreshold) {
    if (grayNorm.empty()) return cv::Rect(0, 0, 0, 0);

    // Ink mask: 1 where pixel is ink (darker than inkThreshold)
    cv::Mat inkMask = grayNorm < inkThreshold;

    // Filter isolated 1-2 px dust specs via connected components
    cv::Mat labels, stats, centroids;
    int numLabels = cv::connectedComponentsWithStats(inkMask, labels, stats, centroids, 8, CV_32S);

    int minX = grayNorm.cols, minY = grayNorm.rows, maxX = -1, maxY = -1;
    bool foundValidInk = false;

    // Label 0 is background
    for (int i = 1; i < numLabels; ++i) {
        int area = stats.at<int>(i, cv::CC_STAT_AREA);
        // Ignore tiny dust specs <= 3 pixels
        if (area <= 3) continue;

        int left = stats.at<int>(i, cv::CC_STAT_LEFT);
        int top = stats.at<int>(i, cv::CC_STAT_TOP);
        int width = stats.at<int>(i, cv::CC_STAT_WIDTH);
        int height = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        minX = std::min(minX, left);
        minY = std::min(minY, top);
        maxX = std::max(maxX, left + width);
        maxY = std::max(maxY, top + height);
        foundValidInk = true;
    }

    if (!foundValidInk) {
        // Fallback: return full image rect
        return cv::Rect(0, 0, grayNorm.cols, grayNorm.rows);
    }

    int boxW = maxX - minX;
    int boxH = maxY - minY;

    // Expand by 12% padding
    int padX = static_cast<int>(std::round(boxW * 0.12));
    int padY = static_cast<int>(std::round(boxH * 0.12));

    minX = std::max(0, minX - padX);
    minY = std::max(0, minY - padY);
    maxX = std::min(grayNorm.cols, maxX + padX);
    maxY = std::min(grayNorm.rows, maxY + padY);

    return cv::Rect(minX, minY, maxX - minX, maxY - minY);
}

cv::Mat SignatureCleaner::cleanSignature(const cv::Mat& inputImg,
                                        int targetWidth,
                                        int targetHeight,
                                        bool greyscale) {
    if (inputImg.empty()) return cv::Mat();

    // 1. Normalize color channels to BGR
    cv::Mat bgr;
    if (inputImg.channels() == 4) {
        // Composite onto white background
        bgr = cv::Mat(inputImg.rows, inputImg.cols, CV_8UC3, cv::Scalar(255, 255, 255));
        for (int y = 0; y < inputImg.rows; ++y) {
            const cv::Vec4b* srcRow = inputImg.ptr<cv::Vec4b>(y);
            cv::Vec3b* dstRow = bgr.ptr<cv::Vec3b>(y);
            for (int x = 0; x < inputImg.cols; ++x) {
                float alpha = srcRow[x][3] / 255.0f;
                dstRow[x][0] = static_cast<uint8_t>(srcRow[x][0] * alpha + 255.0f * (1.0f - alpha));
                dstRow[x][1] = static_cast<uint8_t>(srcRow[x][1] * alpha + 255.0f * (1.0f - alpha));
                dstRow[x][2] = static_cast<uint8_t>(srcRow[x][2] * alpha + 255.0f * (1.0f - alpha));
            }
        }
    } else if (inputImg.channels() == 1) {
        cv::cvtColor(inputImg, bgr, cv::COLOR_GRAY2BGR);
    } else {
        bgr = inputImg.clone();
    }

    // Convert to grayscale for illumination estimation
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    // 2. Large-kernel morphological closing to estimate background illumination
    int kSize = std::max(15, (std::min(gray.cols, gray.rows) / 8) | 1);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(kSize, kSize));
    cv::Mat bgEstimate;
    cv::morphologyEx(gray, bgEstimate, cv::MORPH_CLOSE, kernel);

    // 3. Divide luminance by background map to remove shadows: norm = (gray / bg) * 255
    cv::Mat grayNorm(gray.rows, gray.cols, CV_8UC1);
    for (int y = 0; y < gray.rows; ++y) {
        const uint8_t* gRow = gray.ptr<uint8_t>(y);
        const uint8_t* bgRow = bgEstimate.ptr<uint8_t>(y);
        uint8_t* normRow = grayNorm.ptr<uint8_t>(y);
        for (int x = 0; x < gray.cols; ++x) {
            float gVal = static_cast<float>(gRow[x]);
            float bgVal = static_cast<float>(std::max(1, static_cast<int>(bgRow[x])));
            float div = (gVal / bgVal) * 255.0f;
            normRow[x] = static_cast<uint8_t>(std::clamp(div, 0.0f, 255.0f));
        }
    }

    // 4. Otsu threshold + soft contrast ramp for paper snap to pure #FFFFFF
    cv::Mat otsuMask;
    double otsuThresh = cv::threshold(grayNorm, otsuMask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Soft ramp: values above (otsuThresh + 15) snap to 255 (pure paper)
    // Values below are scaled down smoothly to enhance contrast of dark ink
    double paperCutoff = std::min(250.0, otsuThresh + 15.0);
    double inkFloor = std::max(20.0, otsuThresh - 40.0);

    cv::Mat cleanedImg(bgr.rows, bgr.cols, CV_8UC3);
    for (int y = 0; y < bgr.rows; ++y) {
        const uint8_t* normRow = grayNorm.ptr<uint8_t>(y);
        const cv::Vec3b* bgrRow = bgr.ptr<cv::Vec3b>(y);
        cv::Vec3b* outRow = cleanedImg.ptr<cv::Vec3b>(y);

        for (int x = 0; x < bgr.cols; ++x) {
            float val = normRow[x];
            if (val >= paperCutoff) {
                outRow[x] = cv::Vec3b(255, 255, 255);
            } else {
                // Soft curve between inkFloor and paperCutoff
                float t = (val - inkFloor) / static_cast<float>(paperCutoff - inkFloor);
                t = std::clamp(t, 0.0f, 1.0f);
                // smoothstep curve
                float factor = t * t * (3.0f - 2.0f * t);

                if (greyscale) {
                    uint8_t v = static_cast<uint8_t>(factor * 255.0f);
                    outRow[x] = cv::Vec3b(v, v, v);
                } else {
                    // Preserve original ink color modulated cleanly onto white
                    for (int c = 0; c < 3; ++c) {
                        float inkChannel = bgrRow[x][c];
                        float blended = factor * 255.0f + (1.0f - factor) * inkChannel;
                        outRow[x][c] = static_cast<uint8_t>(std::clamp(blended, 0.0f, 255.0f));
                    }
                }
            }
        }
    }

    // 5. Find tight bounding box around ink
    cv::Rect inkRect = findInkBoundingBox(grayNorm, paperCutoff - 10.0);
    cv::Mat cropped = cleanedImg(inkRect);

    // If no target dimensions requested, return cropped cleaned signature
    if (targetWidth <= 0 && targetHeight <= 0) {
        return cropped;
    }

    // Target dimensions requested
    int outW = targetWidth > 0 ? targetWidth : cropped.cols;
    int outH = targetHeight > 0 ? targetHeight : cropped.rows;

    // Create target canvas filled with pure white
    cv::Mat result(outH, outW, CV_8UC3, cv::Scalar(255, 255, 255));

    // Scale cropped signature to fit inside outW x outH while preserving aspect ratio
    double scaleX = static_cast<double>(outW * 0.90) / cropped.cols;
    double scaleY = static_cast<double>(outH * 0.85) / cropped.rows;
    double scale = std::min(scaleX, scaleY);

    int scaledW = std::max(1, static_cast<int>(std::round(cropped.cols * scale)));
    int scaledH = std::max(1, static_cast<int>(std::round(cropped.rows * scale)));

    cv::Mat scaled;
    if (scale < 1.0) {
        cv::resize(cropped, scaled, cv::Size(scaledW, scaledH), 0, 0, cv::INTER_AREA);
    } else {
        cv::resize(cropped, scaled, cv::Size(scaledW, scaledH), 0, 0, cv::INTER_LANCZOS4);
    }

    // Center scaled signature on white canvas
    int posX = (outW - scaledW) / 2;
    int posY = (outH - scaledH) / 2;
    cv::Rect roi(posX, posY, scaledW, scaledH);
    scaled.copyTo(result(roi));

    return result;
}

} // namespace EdgeDrop
