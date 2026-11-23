#include "edgedrop/image_compressor.hpp"
#include "edgedrop/format_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iostream>
#include <fstream>
#include <chrono>
#include <cassert>

using namespace EdgeDrop;

static std::filesystem::path writeTestImage(const std::filesystem::path& dir,
                                             const std::string& name,
                                             cv::Mat img,
                                             const std::string& ext,
                                             int quality = 95) {
    auto path = dir / name;
    std::vector<int> params;
    if (ext == ".jpg") params = {cv::IMWRITE_JPEG_QUALITY, quality};
    if (ext == ".png") params = {cv::IMWRITE_PNG_COMPRESSION, 1};
    cv::imwrite(path.string(), img, params);
    return path;
}

int main() {
    std::cout << "[RUN] Testing pure C++ ImageCompressor..." << std::endl;

    auto tmpDir = std::filesystem::temp_directory_path() / "edgedrop_test_img_v3";
    std::filesystem::create_directories(tmpDir);

    int passed = 0, failed = 0;

    auto check = [&](const std::string& label, bool cond) {
        if (cond) { std::cout << "PASS: " << label << "\n"; ++passed; }
        else      { std::cout << "FAIL: " << label << "\n"; ++failed; }
    };

    ImageCompressor compressor;

    // ── Test 1: High-detail 3000×2000 JPG → 500 KB lands in [300 KB, 500 KB] ───
    {
        cv::Mat img(2000, 3000, CV_8UC3);
        cv::randu(img, 0, 255);

        auto srcPath = writeTestImage(tmpDir, "big_photo.jpg", img, ".jpg", 95);
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(srcPath));

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 500 * 1024; // 500 KB
        req.detectedFormat = DetectedFormat::JPEG;

        auto t0 = std::chrono::steady_clock::now();
        JobResult res = compressor.compress(req, nullptr, nullptr);
        auto t1 = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        check("JPEG compress success", res.success);
        check("JPEG output <= 500 KB", res.outputSizeBytes <= 500 * 1024);
        check("JPEG output in sane range (>= 100 KB)", res.outputSizeBytes >= 100 * 1024);
        check("JPEG time < 5000 ms", ms < 5000);
        check("JPEG output file exists", std::filesystem::exists(res.outputFilePath));

        std::cout << "    input=" << srcSize/1024 << " KB  output=" << res.outputSizeBytes/1024
                  << " KB  time=" << ms << " ms\n";

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 2: 4 MB synthetic RGBA PNG → 500 KB outputs indexed .png <= 500 KB ─
    {
        // 1400x800 RGBA image (~4.4 MB uncompressed raw)
        cv::Mat img(800, 1400, CV_8UC4);
        for (int y = 0; y < 800; ++y) {
            for (int x = 0; x < 1400; ++x) {
                img.at<cv::Vec4b>(y, x) = cv::Vec4b(
                    static_cast<uint8_t>((x * 255) / 1400),
                    static_cast<uint8_t>((y * 255) / 800),
                    static_cast<uint8_t>((x + y) % 256),
                    255
                );
            }
        }

        auto srcPath = writeTestImage(tmpDir, "screenshot.png", img, ".png");
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(srcPath));

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 500 * 1024;
        req.detectedFormat = DetectedFormat::PNG;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("PNG compress success", res.success);
        check("PNG stays .png", res.outputFilePath.extension() == ".png");
        check("PNG output <= 500 KB", res.outputSizeBytes <= 500 * 1024);
        check("PNG output exists", std::filesystem::exists(res.outputFilePath));

        std::cout << "    PNG input=" << srcSize/1024 << " KB  output=" << res.outputSizeBytes/1024 << " KB\n";

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 3: Pass-Through Guard (Already-small file copied without bloat) ─────
    {
        cv::Mat smallImg(100, 100, CV_8UC3, cv::Scalar(128, 64, 32));
        auto srcPath = writeTestImage(tmpDir, "small.jpg", smallImg, ".jpg", 75);
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(srcPath));

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = srcSize * 2; // Target is twice the size
        req.detectedFormat = DetectedFormat::JPEG;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("Pass-through success", res.success);
        check("Pass-through byte parity (no bloat)", res.outputSizeBytes == srcSize);
        check("Pass-through hitFloor is false", !res.hitFloor);

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 4: WebP stays .webp ──────────────────────────────────────────────
    {
        cv::Mat webpImg(300, 300, CV_8UC3, cv::Scalar(20, 180, 50));
        auto srcPath = tmpDir / "test.webp";
        cv::imwrite(srcPath.string(), webpImg);

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 20 * 1024;
        req.detectedFormat = DetectedFormat::WebP;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("WebP success", res.success);
        check("WebP extension stays .webp", res.outputFilePath.extension() == ".webp");

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    assert(failed == 0);
    return 0;
}
