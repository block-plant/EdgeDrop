#include "edgedrop/format_detector.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

using namespace EdgeDrop;

static bool writeFile(const std::filesystem::path& path, const std::initializer_list<uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(bytes.begin()), bytes.size());
    return true;
}

int main() {
    std::cout << "[RUN] Testing pure C++ FormatDetector..." << std::endl;

    auto tmpDir = std::filesystem::temp_directory_path() / "edgedrop_test_fmt_v3";
    std::filesystem::create_directories(tmpDir);

    int passed = 0, failed = 0;

    auto check = [&](const std::string& label, DetectedFormat got, DetectedFormat expected) {
        if (got == expected) {
            std::cout << "PASS: " << label << "\n";
            ++passed;
        } else {
            std::cout << "FAIL: " << label << " — got " << (int)got << ", expected " << (int)expected << "\n";
            ++failed;
        }
    };

    // 1. JPEG
    {
        auto p = tmpDir / "test.jpg";
        writeFile(p, {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46});
        check("JPEG magic", FormatDetector::detect(p), DetectedFormat::JPEG);
    }

    // 2. PNG
    {
        auto p = tmpDir / "test.png";
        writeFile(p, {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A});
        check("PNG magic", FormatDetector::detect(p), DetectedFormat::PNG);
    }

    // 3. WebP
    {
        auto p = tmpDir / "test.webp";
        writeFile(p, {'R', 'I', 'F', 'F', 0x10, 0, 0, 0, 'W', 'E', 'B', 'P'});
        check("WebP magic", FormatDetector::detect(p), DetectedFormat::WebP);
    }

    // 4. PDF
    {
        auto p = tmpDir / "test.pdf";
        writeFile(p, {'%', 'P', 'D', 'F', '-', '1', '.', '4'});
        check("PDF magic", FormatDetector::detect(p), DetectedFormat::PDF);
    }

    // 5. GIF
    {
        auto p = tmpDir / "test.gif";
        writeFile(p, {'G', 'I', 'F', '8', '9', 'a'});
        check("GIF magic", FormatDetector::detect(p), DetectedFormat::GIF);
    }

    // 6. BMP
    {
        auto p = tmpDir / "test.bmp";
        writeFile(p, {'B', 'M', 0x36, 0, 0, 0});
        check("BMP magic", FormatDetector::detect(p), DetectedFormat::BMP);
    }

    // 7. TIFF
    {
        auto p = tmpDir / "test.tiff";
        writeFile(p, {0x49, 0x49, 0x2A, 0x00});
        check("TIFF magic", FormatDetector::detect(p), DetectedFormat::TIFF);
    }

    // 8. HEIC
    {
        auto p = tmpDir / "test.heic";
        writeFile(p, {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c'});
        check("HEIC magic", FormatDetector::detect(p), DetectedFormat::HEIC);
    }

    // 9. MP4
    {
        auto p = tmpDir / "test.mp4";
        writeFile(p, {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm'});
        check("MP4 magic", FormatDetector::detect(p), DetectedFormat::MP4);
    }

    // 10. WAV
    {
        auto p = tmpDir / "test.wav";
        writeFile(p, {'R', 'I', 'F', 'F', 0x24, 0, 0, 0, 'W', 'A', 'V', 'E'});
        check("WAV magic", FormatDetector::detect(p), DetectedFormat::WAV);
    }

    // 11. OGG
    {
        auto p = tmpDir / "test.ogg";
        writeFile(p, {'O', 'g', 'g', 'S', 0, 2});
        check("OGG magic", FormatDetector::detect(p), DetectedFormat::OGG);
    }

    // 12. M4A
    {
        auto p = tmpDir / "test.m4a";
        writeFile(p, {0, 0, 0, 0x18, 'f', 't', 'y', 'p', 'M', '4', 'A', ' '});
        check("M4A magic", FormatDetector::detect(p), DetectedFormat::M4A);
    }

    // 13. AAC (ADTS frame)
    {
        auto p = tmpDir / "test.aac";
        writeFile(p, {0xFF, 0xF1, 0x4C, 0x80});
        check("AAC ADTS magic", FormatDetector::detect(p), DetectedFormat::AAC);
    }

    // 14. Corrupt / 0-byte file rejection
    {
        auto p = tmpDir / "empty.dat";
        std::ofstream f(p);
        f.close();
        check("0-byte file rejection", FormatDetector::detect(p), DetectedFormat::Unknown);
    }

    // 15. Non-existent file rejection
    {
        auto p = tmpDir / "non_existent_file.xyz";
        check("Non-existent file rejection", FormatDetector::detect(p), DetectedFormat::Unknown);
    }

    // Cleanup
    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    assert(failed == 0);
    return 0;
}
