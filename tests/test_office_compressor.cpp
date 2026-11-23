#include "edgedrop/office_compressor.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <zlib.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>

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

static std::filesystem::path createSyntheticDocx(const std::filesystem::path& path) {
    // Generate large embedded JPG and PNG media
    cv::Mat mediaJpg(600, 800, CV_8UC3);
    cv::randu(mediaJpg, 0, 255);
    std::vector<uint8_t> jpgBytes;
    cv::imencode(".jpg", mediaJpg, jpgBytes);

    cv::Mat mediaPng(500, 500, CV_8UC4, cv::Scalar(100, 150, 200, 255));
    std::vector<uint8_t> pngBytes;
    cv::imencode(".png", mediaPng, pngBytes);

    std::string xmlContentTypes =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">\n"
        "  <Default Extension=\"xml\" ContentType=\"application/xml\"/>\n"
        "  <Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>\n"
        "  <Default Extension=\"png\" ContentType=\"image/png\"/>\n"
        "</Types>";

    std::string docXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">\n"
        "  <w:body><w:p><w:r><w:t>Synthetic EdgeDrop DOCX Document</w:t></w:r></w:p></w:body>\n"
        "</w:document>";

    struct Entry {
        std::string name;
        std::vector<uint8_t> data;
    };

    std::vector<Entry> entries = {
        {"[Content_Types].xml", {xmlContentTypes.begin(), xmlContentTypes.end()}},
        {"word/document.xml",   {docXml.begin(), docXml.end()}},
        {"word/media/image1.jpeg", jpgBytes},
        {"word/media/image2.png",  pngBytes}
    };

    std::vector<uint8_t> out;
    std::vector<ZipCD> cds;

    for (const auto& e : entries) {
        uint32_t off = static_cast<uint32_t>(out.size());
        uint32_t crc = static_cast<uint32_t>(crc32(0L, e.data.data(), static_cast<uInt>(e.data.size())));

        ZipLH lh;
        lh.crc32 = crc;
        lh.compressedSize = static_cast<uint32_t>(e.data.size());
        lh.uncompressedSize = static_cast<uint32_t>(e.data.size());
        lh.fileNameLen = static_cast<uint16_t>(e.name.size());

        const auto* lhPtr = reinterpret_cast<const uint8_t*>(&lh);
        out.insert(out.end(), lhPtr, lhPtr + sizeof(lh));
        out.insert(out.end(), e.name.begin(), e.name.end());
        out.insert(out.end(), e.data.begin(), e.data.end());

        ZipCD cd;
        cd.crc32 = crc;
        cd.compressedSize = static_cast<uint32_t>(e.data.size());
        cd.uncompressedSize = static_cast<uint32_t>(e.data.size());
        cd.fileNameLen = static_cast<uint16_t>(e.name.size());
        cd.localHeaderOffset = off;
        cds.push_back(cd);
    }

    uint32_t cdOff = static_cast<uint32_t>(out.size());
    for (size_t i = 0; i < cds.size(); ++i) {
        const auto* cdPtr = reinterpret_cast<const uint8_t*>(&cds[i]);
        out.insert(out.end(), cdPtr, cdPtr + sizeof(ZipCD));
        out.insert(out.end(), entries[i].name.begin(), entries[i].name.end());
    }

    ZipEOCD eocd;
    eocd.numEntriesOnDisk = static_cast<uint16_t>(entries.size());
    eocd.numEntries = static_cast<uint16_t>(entries.size());
    eocd.centralDirSize = static_cast<uint32_t>(out.size() - cdOff);
    eocd.centralDirOffset = cdOff;

    const auto* ePtr = reinterpret_cast<const uint8_t*>(&eocd);
    out.insert(out.end(), ePtr, ePtr + sizeof(ZipEOCD));

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), out.size());
    return path;
}

int main() {
    std::cout << "[RUN] Testing pure C++ OfficeCompressor..." << std::endl;

    auto tmpDir = std::filesystem::temp_directory_path() / "edgedrop_test_office_v3";
    std::filesystem::create_directories(tmpDir);

    int passed = 0, failed = 0;

    auto check = [&](const std::string& label, bool cond) {
        if (cond) { std::cout << "PASS: " << label << "\n"; ++passed; }
        else      { std::cout << "FAIL: " << label << "\n"; ++failed; }
    };

    OfficeCompressor compressor;

    // ── Test: Synthetic DOCX with embedded media ──────────────────────────────
    {
        auto docxPath = tmpDir / "sample.docx";
        createSyntheticDocx(docxPath);
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(docxPath));

        JobRequest req;
        req.inputFilePath = docxPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = 50 * 1024; // Shrink to 50 KB
        req.detectedFormat = DetectedFormat::DOCX;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("Office DOCX compression success", res.success);
        check("Output exists", std::filesystem::exists(res.outputFilePath));
        check("Output is .docx", res.outputFilePath.extension() == ".docx");
        check("Output size reduced", res.outputSizeBytes < srcSize);

        std::cout << "    DOCX in=" << srcSize/1024 << " KB  out=" << res.outputSizeBytes/1024 << " KB\n";

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(docxPath, ec);
    }

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    assert(failed == 0);
    return 0;
}
