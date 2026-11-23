#include "edgedrop/compressor_router.hpp"
#include "edgedrop/format_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iostream>
#include <fstream>
#include <future>
#include <vector>
#include <cassert>

using namespace EdgeDrop;

int main() {
    std::cout << "[RUN] Testing Concurrency, Safety & Unicode Handling..." << std::endl;

    auto tmpDir = std::filesystem::temp_directory_path() / "edgedrop_test_concurrency_v3";
    std::filesystem::create_directories(tmpDir);

    int passed = 0, failed = 0;

    auto check = [&](const std::string& label, bool cond) {
        if (cond) { std::cout << "PASS: " << label << "\n"; ++passed; }
        else      { std::cout << "FAIL: " << label << "\n"; ++failed; }
    };

    // ── Test 1: Unicode & Spaces in File Paths ─────────────────────────────────
    {
        // Path with Chinese characters, German umlauts, spaces
        auto uniPath = tmpDir / "test 文档 üö space file.jpg";
        cv::Mat img(400, 400, CV_8UC3, cv::Scalar(100, 200, 50));
        cv::imwrite(uniPath.string(), img);

        JobRequest req;
        req.inputFilePath = uniPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 50 * 1024;
        req.detectedFormat = DetectedFormat::JPEG;

        JobResult res = CompressorRouter::route(req);
        check("Unicode path compress success", res.success);
        check("Unicode output file exists", std::filesystem::exists(res.outputFilePath));

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(uniPath, ec);
    }

    // ── Test 2: Mid-job Cancellation Leaves Zero .edgedrop.tmp.* Files ─────────
    {
        cv::Mat bigImg(2000, 2000, CV_8UC3);
        cv::randu(bigImg, 0, 255);
        auto srcPath = tmpDir / "cancel_test.jpg";
        cv::imwrite(srcPath.string(), bigImg);

        std::atomic<bool> cancelToken{false};

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 100 * 1024;
        req.detectedFormat = DetectedFormat::JPEG;

        // Cancel on first progress callback
        auto cb = [&cancelToken](double p) {
            if (p >= 0.1) {
                cancelToken.store(true);
            }
        };

        JobResult res = CompressorRouter::route(req, cb, &cancelToken);
        check("Cancelled job returns success=false", !res.success);

        // Verify zero .edgedrop.tmp.* files left in tmpDir
        int tmpFilesCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(tmpDir)) {
            std::string name = entry.path().filename().string();
            if (name.find(".edgedrop.tmp") != std::string::npos) {
                ++tmpFilesCount;
            }
        }
        check("Zero .edgedrop.tmp.* leftovers after cancel", tmpFilesCount == 0);

        std::error_code ec;
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 3: Concurrent 4-File Batch Compression ───────────────────────────
    {
        std::vector<std::filesystem::path> srcPaths;
        for (int i = 0; i < 4; ++i) {
            auto p = tmpDir / ("concurrent_" + std::to_string(i) + ".jpg");
            cv::Mat img(600, 600, CV_8UC3);
            cv::randu(img, 0, 255);
            cv::imwrite(p.string(), img);
            srcPaths.push_back(p);
        }

        std::vector<std::future<JobResult>> futures;
        for (int i = 0; i < 4; ++i) {
            futures.push_back(std::async(std::launch::async, [&srcPaths, &tmpDir, i]() {
                JobRequest req;
                req.inputFilePath = srcPaths[i];
                req.outputDirectory = tmpDir;
                req.targetBytes = 60 * 1024;
                req.detectedFormat = DetectedFormat::JPEG;
                return CompressorRouter::route(req);
            }));
        }

        bool allSuccess = true;
        for (auto& f : futures) {
            JobResult res = f.get();
            if (!res.success || res.outputSizeBytes > 60 * 1024) {
                allSuccess = false;
            }
            std::error_code ec;
            std::filesystem::remove(res.outputFilePath, ec);
        }
        check("Concurrent 4-file batch all succeeded", allSuccess);

        for (const auto& p : srcPaths) {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    }

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    assert(failed == 0);
    return 0;
}
