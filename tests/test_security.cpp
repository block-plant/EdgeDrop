#include "edgedrop/security_guards.hpp"
#include "edgedrop/office_compressor.hpp"
#include "edgedrop/compressor_router.hpp"
#include "edgedrop/types.hpp"
#include <zlib.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cassert>
#include <filesystem>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace EdgeDrop;

#pragma pack(push, 1)
struct ZipLH {
    uint32_t signature = 0x04034b50;
    uint16_t version = 20;
    uint16_t flags = 0;
    uint16_t compression = 0;
    uint16_t modTime = 0;
    uint16_t modDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLen = 0;
    uint16_t extraLen = 0;
};

struct ZipCD {
    uint32_t signature = 0x02014b50;
    uint16_t versionMadeBy = 20;
    uint16_t versionNeeded = 20;
    uint16_t flags = 0;
    uint16_t compression = 0;
    uint16_t modTime = 0;
    uint16_t modDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t fileNameLen = 0;
    uint16_t extraLen = 0;
    uint16_t commentLen = 0;
    uint16_t diskStart = 0;
    uint16_t intAttr = 0;
    uint32_t extAttr = 0;
    uint32_t localHeaderOffset = 0;
};

struct ZipEOCD {
    uint32_t signature = 0x06054b50;
    uint16_t diskNum = 0;
    uint16_t centralDirDisk = 0;
    uint16_t numEntriesOnDisk = 0;
    uint16_t numEntries = 0;
    uint32_t centralDirSize = 0;
    uint32_t centralDirOffset = 0;
    uint16_t commentLen = 0;
};
#pragma pack(pop)

static fs::path createZipWithEntry(const fs::path& outPath,
                                   const std::string& entryName,
                                   const std::string& payload,
                                   uint32_t spoofUncompressedSize = 0) {
    std::vector<uint8_t> out;
    std::vector<ZipCD> cds;

    uint32_t crc = static_cast<uint32_t>(crc32(0L, reinterpret_cast<const Bytef*>(payload.data()), static_cast<uInt>(payload.size())));
    uint32_t compSize = static_cast<uint32_t>(payload.size());
    uint32_t uncompSize = spoofUncompressedSize > 0 ? spoofUncompressedSize : compSize;

    uint32_t off = static_cast<uint32_t>(out.size());
    ZipLH lh;
    lh.crc32 = crc;
    lh.compressedSize = compSize;
    lh.uncompressedSize = uncompSize;
    lh.fileNameLen = static_cast<uint16_t>(entryName.size());

    const auto* lhPtr = reinterpret_cast<const uint8_t*>(&lh);
    out.insert(out.end(), lhPtr, lhPtr + sizeof(lh));
    out.insert(out.end(), entryName.begin(), entryName.end());
    out.insert(out.end(), payload.begin(), payload.end());

    ZipCD cd;
    cd.crc32 = crc;
    cd.compressedSize = compSize;
    cd.uncompressedSize = uncompSize;
    cd.fileNameLen = static_cast<uint16_t>(entryName.size());
    cd.localHeaderOffset = off;
    cds.push_back(cd);

    uint32_t cdOff = static_cast<uint32_t>(out.size());
    const auto* cdPtr = reinterpret_cast<const uint8_t*>(&cds[0]);
    out.insert(out.end(), cdPtr, cdPtr + sizeof(ZipCD));
    out.insert(out.end(), entryName.begin(), entryName.end());

    ZipEOCD eocd;
    eocd.numEntriesOnDisk = 1;
    eocd.numEntries = 1;
    eocd.centralDirSize = static_cast<uint32_t>(out.size() - cdOff);
    eocd.centralDirOffset = cdOff;

    const auto* eocdPtr = reinterpret_cast<const uint8_t*>(&eocd);
    out.insert(out.end(), eocdPtr, eocdPtr + sizeof(eocd));

    std::ofstream ofs(outPath, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(out.data()), out.size());
    ofs.close();

    return outPath;
}

static void test_zip_slip_rejection() {
    std::cout << "[Test 1] Testing Zip Slip rejection..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_slip";
    fs::create_directories(tempDir);

    fs::path maliciousDocx = tempDir / "zip_slip.docx";
    createZipWithEntry(maliciousDocx, "../../evil_payload.txt", "MALICIOUS PAYLOAD");

    OfficeCompressor compressor;
    JobRequest req;
    req.inputFilePath = maliciousDocx.string();
    req.outputDirectory = tempDir.string();
    req.detectedFormat = DetectedFormat::DOCX;

    JobResult res = compressor.compress(req, nullptr, nullptr);
    assert(!res.success);
    assert(res.errorMessage.find("Zip Slip") != std::string::npos ||
           res.errorMessage.find("malicious") != std::string::npos);

    fs::path absPathDocx = tempDir / "zip_abs.docx";
    createZipWithEntry(absPathDocx, "/etc/shadow", "MALICIOUS ABSOLUTE PATH");
    JobRequest reqAbs;
    reqAbs.inputFilePath = absPathDocx.string();
    reqAbs.outputDirectory = tempDir.string();
    reqAbs.detectedFormat = DetectedFormat::DOCX;

    JobResult resAbs = compressor.compress(reqAbs, nullptr, nullptr);
    assert(!resAbs.success);

    fs::remove_all(tempDir);
    std::cout << "  ✓ Zip Slip attacks rejected properly." << std::endl;
}

static void test_zip_bomb_rejection() {
    std::cout << "[Test 2] Testing Zip Bomb rejection..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_bomb";
    fs::create_directories(tempDir);

    fs::path bombDocx = tempDir / "zip_bomb.docx";
    // Entry claims 200 MB uncompressed size (> 150 MB limit)
    createZipWithEntry(bombDocx, "word/document.xml", "tiny content", 200 * 1024 * 1024);

    OfficeCompressor compressor;
    JobRequest req;
    req.inputFilePath = bombDocx.string();
    req.outputDirectory = tempDir.string();
    req.detectedFormat = DetectedFormat::DOCX;

    JobResult res = compressor.compress(req, nullptr, nullptr);
    assert(!res.success);
    assert(res.errorMessage.find("Zip Bomb") != std::string::npos ||
           res.errorMessage.find("exceeds limit") != std::string::npos);

    fs::remove_all(tempDir);
    std::cout << "  ✓ Zip Bomb attacks rejected properly without OOM." << std::endl;
}

static void test_jpeg_privacy_stripping() {
    std::cout << "[Test 3] Testing JPEG EXIF/GPS/Comment stripping..." << std::endl;
    // Construct JPEG stream: SOI (FF D8) + APP1 (FF E1, length 8, "EXIF\0\0") + COM (FF FE, length 6, "TEST") + SOS (FF DA) + EOI (FF D9)
    std::vector<uint8_t> jpegData = {
        0xFF, 0xD8, // SOI
        0xFF, 0xE1, 0x00, 0x08, 'E', 'x', 'i', 'f', 0x00, 0x00, // APP1 with EXIF
        0xFF, 0xFE, 0x00, 0x06, 'C', 'O', 'M', 'M',             // COM marker
        0xFF, 0xDA, 0x00, 0x02, // SOS
        0x12, 0x34, 0x56,       // Entropy data
        0xFF, 0xD9              // EOI
    };

    SecurityGuards::stripJpegPrivacyMetadata(jpegData);

    // Verify APP1 and COM are completely stripped
    bool foundApp1 = false;
    bool foundCom = false;
    for (size_t i = 0; i + 1 < jpegData.size(); ++i) {
        if (jpegData[i] == 0xFF && jpegData[i + 1] == 0xE1) foundApp1 = true;
        if (jpegData[i] == 0xFF && jpegData[i + 1] == 0xFE) foundCom = true;
    }
    if (foundApp1 || foundCom) {
        std::cerr << "Error: APP1 or COM marker not stripped from JPEG!" << std::endl;
        std::abort();
    }

    // Verify SOI and EOI are intact
    assert(jpegData.size() >= 4);
    assert(jpegData[0] == 0xFF && jpegData[1] == 0xD8);
    assert(jpegData[jpegData.size() - 2] == 0xFF && jpegData[jpegData.size() - 1] == 0xD9);

    std::cout << "  ✓ JPEG privacy metadata (APP1/COM) successfully stripped." << std::endl;
}

static void test_input_path_validation() {
    std::cout << "[Test 4] Testing zero-trust input path validation..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_paths";
    fs::create_directories(tempDir);

    // Empty path
    assert(!SecurityGuards::validateInputPath("").empty());

    // Non-existent path
    assert(!SecurityGuards::validateInputPath((tempDir / "non_existent.jpg").string()).empty());

    // 0-byte file
    fs::path zeroByteFile = tempDir / "empty.png";
    std::ofstream(zeroByteFile).close();
    std::string errZero = SecurityGuards::validateInputPath(zeroByteFile.string());
    assert(!errZero.empty() && errZero.find("empty") != std::string::npos);

    // Null-byte in path
    std::string nullPath = "valid_name.jpg";
    nullPath.push_back('\0');
    nullPath += "evil.exe";
    assert(!SecurityGuards::validateInputPath(nullPath).empty());

#ifndef _WIN32
    // Symlink rejection
    fs::path targetFile = tempDir / "real_file.txt";
    std::ofstream(targetFile) << "regular content";
    fs::path symlinkPath = tempDir / "symlink.txt";
    std::error_code ec;
    fs::create_symlink(targetFile, symlinkPath, ec);
    if (!ec) {
        std::string errSym = SecurityGuards::validateInputPath(symlinkPath.string());
        assert(!errSym.empty() && errSym.find("symlink") != std::string::npos);
    }
#endif

    // Filename sanitization
    assert(SecurityGuards::sanitizeFilename("../../etc/passwd") == "passwd");
    assert(SecurityGuards::sanitizeFilename("photo:image?.jpg") == "photoimage.jpg");
    assert(SecurityGuards::sanitizeFilename("-sDEVICE=null.pdf") == "sDEVICE=null.pdf");

    fs::remove_all(tempDir);
    std::cout << "  ✓ Path validation blocks empty, non-existent, symlink, and 0-byte inputs." << std::endl;
}

static void test_magic_bytes_verification() {
    std::cout << "[Test 5] Testing magic bytes verification..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_magic";
    fs::create_directories(tempDir);

    fs::path jpgFile = tempDir / "test.jpg";
    std::ofstream(jpgFile, std::ios::binary) << "\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00";
    assert(SecurityGuards::verifyOutputMagic(jpgFile, "jpeg"));

    fs::path pngFile = tempDir / "test.png";
    std::ofstream(pngFile, std::ios::binary) << "\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR";
    assert(SecurityGuards::verifyOutputMagic(pngFile, "png"));

    fs::path fakePdf = tempDir / "fake.pdf";
    std::ofstream(fakePdf, std::ios::binary) << "NOT A VALID PDF FILE AT ALL";
    assert(!SecurityGuards::verifyOutputMagic(fakePdf, "pdf"));

    fs::path realPdf = tempDir / "real.pdf";
    std::ofstream(realPdf, std::ios::binary) << "%PDF-1.4\n%real pdf content";
    assert(SecurityGuards::verifyOutputMagic(realPdf, "pdf"));

    fs::remove_all(tempDir);
    std::cout << "  ✓ Magic byte verification correctly validates output formats." << std::endl;
}

static void test_secure_temp_file() {
    std::cout << "[Test 6] Testing SecureTempFile RAII lifecycle..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_raii";
    fs::create_directories(tempDir);

    fs::path abandonedPath;
    {
        SecureTempFile tmp(tempDir, ".tmp");
        abandonedPath = tmp.path();
        assert(tmp.isValid());
        assert(fs::exists(abandonedPath));

#ifndef _WIN32
        // Check 0600 permissions
        struct stat st{};
        stat(abandonedPath.c_str(), &st);
        assert((st.st_mode & 0777) == 0600);
#endif
        // Exiting scope without commit() should unlink
    }
    assert(!fs::exists(abandonedPath));

    // Test commit with valid magic
    fs::path finalOutput = tempDir / "final_committed.jpg";
    {
        SecureTempFile tmp(tempDir, ".jpg");
        assert(tmp.isValid());
        {
            std::ofstream ofs(tmp.path(), std::ios::binary);
            const char validJpg[] = "\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00";
            ofs.write(validJpg, sizeof(validJpg) - 1);
        }

        assert(tmp.commit(finalOutput, "jpeg"));
        assert(fs::exists(finalOutput));
        assert(!fs::exists(tmp.path()));
    }

    fs::remove_all(tempDir);
    std::cout << "  ✓ SecureTempFile enforces 0600 permissions and auto-unlink on discard." << std::endl;
}

static void test_output_filename_deduplication() {
    std::cout << "[Test 7] Testing output filename generation and deduplication..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_dedup";
    fs::create_directories(tempDir);

    fs::path inPath = tempDir / "photo.jpg";
    fs::path out1 = SecurityGuards::generateCompressedOutputPath(inPath, tempDir, ".jpg");
    assert(out1 == tempDir / "photo_compressed.jpg");

    // Create photo_compressed.jpg so next call must deduplicate
    std::ofstream(out1) << "dummy";
    fs::path out2 = SecurityGuards::generateCompressedOutputPath(inPath, tempDir, ".jpg");
    assert(out2 == tempDir / "photo_compressed_2.jpg");

    // Create photo_compressed_2.jpg
    std::ofstream(out2) << "dummy";
    fs::path out3 = SecurityGuards::generateCompressedOutputPath(inPath, tempDir, ".jpg");
    assert(out3 == tempDir / "photo_compressed_3.jpg");

    // Test input file already named with _compressed suffix does not become photo_compressed_compressed.jpg
    fs::path inAlreadyCompressed = tempDir / "document_compressed.pdf";
    fs::path outDoc1 = SecurityGuards::generateCompressedOutputPath(inAlreadyCompressed, tempDir, ".pdf");
    assert(outDoc1 == tempDir / "document_compressed.pdf");

    std::ofstream(outDoc1) << "dummy";
    fs::path outDoc2 = SecurityGuards::generateCompressedOutputPath(inAlreadyCompressed, tempDir, ".pdf");
    assert(outDoc2 == tempDir / "document_compressed_2.pdf");

    fs::remove_all(tempDir);
    std::cout << "  ✓ Filename generation strips duplicate _compressed suffixes and increments cleanly." << std::endl;
}

static void test_oversized_and_invalid_inputs() {
    std::cout << "[Test 8] Testing oversized and invalid input validation..." << std::endl;
    fs::path tempDir = fs::temp_directory_path() / "edgedrop_sec_test_oversized";
    fs::create_directories(tempDir);

    // Non-existent path
    assert(!SecurityGuards::validateInputFile(tempDir / "does_not_exist.png"));

    // Empty path
    assert(!SecurityGuards::validateInputFile(fs::path()));

    // 0-byte file
    fs::path zeroByte = tempDir / "empty.bin";
    std::ofstream(zeroByte).close();
    assert(!SecurityGuards::validateInputFile(zeroByte));

    fs::remove_all(tempDir);
    std::cout << "  ✓ Input validation properly rejects non-existent, empty, and 0-byte files." << std::endl;
}

int main() {
    std::cout << "=== EdgeDrop Defense-in-Depth Security Test Suite ===" << std::endl;
    test_zip_slip_rejection();
    test_zip_bomb_rejection();
    test_jpeg_privacy_stripping();
    test_input_path_validation();
    test_magic_bytes_verification();
    test_secure_temp_file();
    test_output_filename_deduplication();
    test_oversized_and_invalid_inputs();
    std::cout << "=== All Security Tests Passed Successfully (8/8) ===" << std::endl;
    return 0;
}
