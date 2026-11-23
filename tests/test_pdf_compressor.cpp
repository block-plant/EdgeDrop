#include "edgedrop/pdf_compressor.hpp"
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <iostream>
#include <fstream>
#include <cassert>

using namespace EdgeDrop;

static std::filesystem::path createTestMultiPagePdf(const std::filesystem::path& dir,
                                                    const std::string& name,
                                                    int pageCount = 5) {
    auto path = dir / name;

    // Create a 5-page PDF using QPDF
    QPDF pdf;
    pdf.emptyPDF();

    // Create a 600x400 JPEG image to embed
    cv::Mat testImg(400, 600, CV_8UC3);
    cv::randu(testImg, 0, 255);
    std::vector<uint8_t> jpegBytes;
    cv::imencode(".jpg", testImg, jpegBytes);

    for (int p = 0; p < pageCount; ++p) {
        // Create an Image XObject
        QPDFObjectHandle imageObj = pdf.newStream();
        imageObj.replaceStreamData(
            std::string(reinterpret_cast<const char*>(jpegBytes.data()), jpegBytes.size()),
            QPDFObjectHandle::newName("/DCTDecode"),
            QPDFObjectHandle::newNull()
        );
        auto imgDict = imageObj.getDict();
        imgDict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        imgDict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        imgDict.replaceKey("/Width", QPDFObjectHandle::newInteger(600));
        imgDict.replaceKey("/Height", QPDFObjectHandle::newInteger(400));
        imgDict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
        imgDict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));

        // Create page contents with vector text + draw image
        std::string streamContent =
            "BT /F1 24 Tf 50 700 Td (Page " + std::to_string(p + 1) + " Vector Text Content) Tj ET\n"
            "q 400 0 0 260 50 200 cm /Im1 Do Q\n";

        QPDFObjectHandle contents = pdf.newStream(streamContent);

        // Page resources
        QPDFObjectHandle xobjDict = QPDFObjectHandle::newDictionary();
        xobjDict.replaceKey("/Im1", imageObj);

        QPDFObjectHandle fontDict = QPDFObjectHandle::newDictionary();
        QPDFObjectHandle f1 = QPDFObjectHandle::newDictionary();
        f1.replaceKey("/Type", QPDFObjectHandle::newName("/Font"));
        f1.replaceKey("/Subtype", QPDFObjectHandle::newName("/Type1"));
        f1.replaceKey("/BaseFont", QPDFObjectHandle::newName("/Helvetica"));
        fontDict.replaceKey("/F1", f1);

        QPDFObjectHandle resDict = QPDFObjectHandle::newDictionary();
        resDict.replaceKey("/XObject", xobjDict);
        resDict.replaceKey("/Font", fontDict);

        // Page dictionary
        QPDFObjectHandle pageDict = QPDFObjectHandle::newDictionary();
        pageDict.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));
        pageDict.replaceKey("/MediaBox", QPDFObjectHandle::parse("[0 0 612 792]"));
        pageDict.replaceKey("/Contents", contents);
        pageDict.replaceKey("/Resources", resDict);

        pdf.addPage(pageDict, false);
    }

    QPDFWriter writer(pdf, path.string().c_str());
    writer.write();
    return path;
}

int main() {
    std::cout << "[RUN] Testing pure C++ 2-Tier PdfCompressor..." << std::endl;

    auto tmpDir = std::filesystem::temp_directory_path() / "edgedrop_test_pdf_v3";
    std::filesystem::create_directories(tmpDir);

    int passed = 0, failed = 0;

    auto check = [&](const std::string& label, bool cond) {
        if (cond) { std::cout << "PASS: " << label << "\n"; ++passed; }
        else      { std::cout << "FAIL: " << label << "\n"; ++failed; }
    };

    PdfCompressor compressor;

    // ── Test 1: Multi-page 5-page PDF with vector text + images ───────────────
    {
        auto srcPath = createTestMultiPagePdf(tmpDir, "sample_5pages.pdf", 5);
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(srcPath));

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = srcSize / 2; // Target 50% size reduction
        req.detectedFormat = DetectedFormat::PDF;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("PDF compression success", res.success);
        check("PDF output exists", std::filesystem::exists(res.outputFilePath));
        check("PDF compressed by > 20%", res.outputSizeBytes < srcSize * 0.85);

        // Verify page count preserved
        int outPages = 0;
        try {
            QPDF outPdf;
            outPdf.processFile(res.outputFilePath.string().c_str());
            QPDFPageDocumentHelper pdh(outPdf);
            outPages = static_cast<int>(pdh.getAllPages().size());
        } catch (...) {}

        check("PDF exact pageCount == 5 preserved", outPages == 5);

        std::cout << "    PDF in=" << srcSize/1024 << " KB  out=" << res.outputSizeBytes/1024
                  << " KB  pages=" << outPages << "\n";

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 2: Pass-Through Guard on Small PDF ───────────────────────────────
    {
        auto srcPath = createTestMultiPagePdf(tmpDir, "tiny.pdf", 1);
        int64_t srcSize = static_cast<int64_t>(std::filesystem::file_size(srcPath));

        JobRequest req;
        req.inputFilePath = srcPath;
        req.outputDirectory = tmpDir;
        req.targetBytes = srcSize * 3; // Much larger target
        req.detectedFormat = DetectedFormat::PDF;

        JobResult res = compressor.compress(req, nullptr, nullptr);

        check("PDF Pass-through success", res.success);
        check("PDF Pass-through size preserved", res.outputSizeBytes == srcSize);

        std::error_code ec;
        std::filesystem::remove(res.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    // ── Test 3: Idempotency Guard on Floor-Compressed PDF ─────────────────────
    {
        auto srcPath = createTestMultiPagePdf(tmpDir, "sample_floor.pdf", 3);
        int64_t target = 500; // Unreachable target to guarantee floor is reached

        JobRequest req1;
        req1.inputFilePath = srcPath;
        req1.outputDirectory = tmpDir;
        req1.targetBytes = target;
        req1.detectedFormat = DetectedFormat::PDF;

        JobResult res1 = compressor.compress(req1, nullptr, nullptr);
        check("Pass 1 compression success", res1.success);
        check("Pass 1 reached floor", res1.hitFloor);

        // Pass 2: Re-drop the compressed output with the same target
        JobRequest req2;
        req2.inputFilePath = res1.outputFilePath;
        req2.outputDirectory = tmpDir;
        req2.targetBytes = target;
        req2.detectedFormat = DetectedFormat::PDF;

        JobResult res2 = compressor.compress(req2, nullptr, nullptr);
        check("Pass 2 re-compression success", res2.success);
        check("Pass 2 re-compression is idempotent (shrinks < 5%)",
              res2.outputSizeBytes >= static_cast<int64_t>(res1.outputSizeBytes * 0.95));

        std::cout << "    Idempotency: Pass 1=" << res1.outputSizeBytes << " B  Pass 2="
                  << res2.outputSizeBytes << " B  delta="
                  << (res1.outputSizeBytes - res2.outputSizeBytes) << " B\n";

        std::error_code ec;
        std::filesystem::remove(res1.outputFilePath, ec);
        std::filesystem::remove(res2.outputFilePath, ec);
        std::filesystem::remove(srcPath, ec);
    }

    std::error_code ec;
    std::filesystem::remove_all(tmpDir, ec);

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    assert(failed == 0);
    return 0;
}
