#include "JfifDpiPatcher.hpp"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    std::cout << "[RUN] Testing JFIF & PNG DPI Patcher..." << std::endl;

    // 1. Test JPEG JFIF Injection & Readback for 200 DPI
    std::vector<uint8_t> dummyJpeg = {0xFF, 0xD8, 0xFF, 0xD9}; // Minimal valid SOI + EOI
    bool patched200 = EdgeDrop::JfifDpiPatcher::patchJpegDpi(dummyJpeg, 200);
    (void)patched200;
    assert(patched200);

    auto [xDpi200, yDpi200] = EdgeDrop::JfifDpiPatcher::readJpegDpi(dummyJpeg);
    std::cout << "Readback JPEG DPI (Expected 200): " << xDpi200 << "x" << yDpi200 << std::endl;
    assert(xDpi200 == 200);
    assert(yDpi200 == 200);

    // 2. Test JPEG JFIF Overwrite & Readback for 300 DPI
    bool patched300 = EdgeDrop::JfifDpiPatcher::patchJpegDpi(dummyJpeg, 300);
    (void)patched300;
    assert(patched300);

    auto [xDpi300, yDpi300] = EdgeDrop::JfifDpiPatcher::readJpegDpi(dummyJpeg);
    std::cout << "Readback JPEG DPI (Expected 300): " << xDpi300 << "x" << yDpi300 << std::endl;
    assert(xDpi300 == 300);
    assert(yDpi300 == 300);

    // Verify raw binary inspection:
    // Offset 0-1: FF D8
    // Offset 2-3: FF E0
    // Offset 4-5: 00 10
    // Offset 6-10: 'J' 'F' 'I' 'F' 0x00
    // Offset 11-12: 01 02 (version)
    // Offset 13: 01 (DPI unit)
    // Offset 14-15: 01 2C (300 in big-endian hex)
    // Offset 16-17: 01 2C
    assert(dummyJpeg[2] == 0xFF && dummyJpeg[3] == 0xE0);
    assert(dummyJpeg[13] == 0x01);
    assert(dummyJpeg[14] == 0x01 && dummyJpeg[15] == 0x2C);
    assert(dummyJpeg[16] == 0x01 && dummyJpeg[17] == 0x2C);

    // 3. Test COM Padding for min-KB floor
    size_t targetMin = 4096; // 4 KB
    bool padded = EdgeDrop::JfifDpiPatcher::padJpegToMinBytes(dummyJpeg, targetMin);
    (void)padded;
    assert(padded);
    std::cout << "Padded JPEG size (Expected >= 4096): " << dummyJpeg.size() << " bytes" << std::endl;
    assert(dummyJpeg.size() >= targetMin);

    // 4. Test PNG pHYs DPI Patch & Readback
    // Minimal mock PNG: 8-byte signature + 25-byte IHDR chunk + 12-byte IEND chunk
    std::vector<uint8_t> dummyPng = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, // Sig
        0x00, 0x00, 0x00, 0x0D, // IHDR len = 13
        'I', 'H', 'D', 'R',
        0x00, 0x00, 0x00, 0x10, // width 16
        0x00, 0x00, 0x00, 0x10, // height 16
        0x08, 0x02, 0x00, 0x00, 0x00,
        0x90, 0x91, 0x68, 0x36, // CRC
        0x00, 0x00, 0x00, 0x00, // IEND len = 0
        'I', 'E', 'N', 'D',
        0xAE, 0x42, 0x60, 0x82  // CRC
    };

    bool pngPatched = EdgeDrop::JfifDpiPatcher::patchPngDpi(dummyPng, 300);
    (void)pngPatched;
    assert(pngPatched);

    auto [pngXDpi, pngYDpi] = EdgeDrop::JfifDpiPatcher::readPngDpi(dummyPng);
    std::cout << "Readback PNG DPI (Expected 300): " << pngXDpi << "x" << pngYDpi << std::endl;
    assert(pngXDpi >= 299 && pngXDpi <= 301); // 11811 ppm is 299.9994 DPI
    assert(pngYDpi >= 299 && pngYDpi <= 301);

    std::cout << "[PASS] JFIF & PNG DPI Patcher test passed successfully!" << std::endl;
    return 0;
}
