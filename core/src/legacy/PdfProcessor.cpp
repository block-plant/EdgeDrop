#include "PdfProcessor.hpp"
#include "ImageProcessor.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace EdgeDrop {

std::vector<uint8_t> PdfProcessor::buildPdfFromJpegStreams(
    const std::vector<std::pair<std::vector<uint8_t>, std::pair<int, int>>>& pageJpegStreams,
    bool isGreyscale
) {
    if (pageJpegStreams.empty()) return {};

    std::ostringstream pdf;
    std::vector<size_t> objOffsets;

    pdf << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";

    auto startObj = [&](int /*id*/) {
        objOffsets.push_back(pdf.tellp());
        pdf << objOffsets.size() << " 0 obj\n";
    };

    // Object 1: Catalog
    startObj(1);
    pdf << "<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";

    // Object 2: Pages root
    startObj(2);
    int numPages = static_cast<int>(pageJpegStreams.size());
    pdf << "<< /Type /Pages /Kids [";
    for (int i = 0; i < numPages; ++i) {
        int pageObjId = 3 + i * 3;
        pdf << pageObjId << " 0 R ";
    }
    pdf << "] /Count " << numPages << " >>\nendobj\n";

    // For each page:
    // (3 + i*3): Page Object
    // (4 + i*3): Contents stream (drawing the image onto page media box)
    // (5 + i*3): Image XObject
    for (int i = 0; i < numPages; ++i) {
        const auto& streamData = pageJpegStreams[i].first;
        int imgW = pageJpegStreams[i].second.first;
        int imgH = pageJpegStreams[i].second.second;

        // Points in PDF: 72 points per inch. Use standard 72 DPI layout points
        // MediaBox: [0 0 imgW imgH]
        int pageObjId = 3 + i * 3;
        int contentsObjId = 4 + i * 3;
        int imageObjId = 5 + i * 3;

        // Page Object
        startObj(pageObjId);
        pdf << "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " << imgW << " " << imgH << "] "
            << "/Resources << /XObject << /Im" << i << " " << imageObjId << " 0 R >> >> "
            << "/Contents " << contentsObjId << " 0 R >>\nendobj\n";

        // Contents Stream
        std::ostringstream contentStream;
        contentStream << "q\n" << imgW << " 0 0 " << imgH << " 0 0 cm\n/Im" << i << " Do\nQ\n";
        std::string contentStr = contentStream.str();

        startObj(contentsObjId);
        pdf << "<< /Length " << contentStr.size() << " >>\nstream\n"
            << contentStr << "endstream\nendobj\n";

        // Image XObject
        startObj(imageObjId);
        pdf << "<< /Type /XObject /Subtype /Image /Width " << imgW << " /Height " << imgH << " "
            << "/ColorSpace " << (isGreyscale ? "/DeviceGray" : "/DeviceRGB") << " "
            << "/BitsPerComponent 8 /Filter /DCTDecode /Length " << streamData.size() << " >>\nstream\n";

        pdf.write(reinterpret_cast<const char*>(streamData.data()), streamData.size());
        pdf << "\nendstream\nendobj\n";
    }

    // xref table
    size_t startXref = pdf.tellp();
    pdf << "xref\n0 " << (objOffsets.size() + 1) << "\n";
    pdf << "0000000000 65535 f \n";
    for (size_t offset : objOffsets) {
        pdf << std::setfill('0') << std::setw(10) << offset << " 00000 n \n";
    }

    // Trailer
    pdf << "trailer\n<< /Size " << (objOffsets.size() + 1) << " /Root 1 0 R >>\n";
    pdf << "startxref\n" << startXref << "\n%%EOF\n";

    std::string pdfStr = pdf.str();
    return std::vector<uint8_t>(pdfStr.begin(), pdfStr.end());
}

class DefaultNativePdfEngine : public IPdfEngine {
public:
    bool imagesToPdf(const std::vector<std::string>& imagePaths,
                     const std::string& outputPath,
                     int targetDpi,
                     size_t maxBytes,
                     size_t minBytes,
                     bool greyscale) override {
        if (imagePaths.empty()) return false;

        std::vector<cv::Mat> images;
        for (const auto& path : imagePaths) {
            cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
            if (!img.empty()) {
                if (greyscale) {
                    cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
                }
                images.push_back(img);
            }
        }
        if (images.empty()) return false;

        // Binary search on embedded JPEG stream quality
        size_t safeMaxBytes = (maxBytes > 0) ? static_cast<size_t>(maxBytes * 0.97) : 0;
        int lowQ = 10, highQ = 95;
        std::vector<uint8_t> bestPdf;

        auto encodeAll = [&](int quality, double scale) -> std::vector<uint8_t> {
            std::vector<std::pair<std::vector<uint8_t>, std::pair<int, int>>> streams;
            for (const auto& img : images) {
                cv::Mat work = img;
                if (scale < 1.0) {
                    cv::resize(img, work, cv::Size(), scale, scale, cv::INTER_AREA);
                }
                std::vector<uint8_t> jpegData;
                std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality, cv::IMWRITE_JPEG_OPTIMIZE, 1};
                cv::imencode(".jpg", work, jpegData, params);
                streams.push_back({std::move(jpegData), {work.cols, work.rows}});
            }
            return PdfProcessor::buildPdfFromJpegStreams(streams, greyscale);
        };

        if (safeMaxBytes == 0) {
            bestPdf = encodeAll(85, 1.0);
        } else {
            double currentScale = 1.0;
            for (int attempt = 0; attempt < 5; ++attempt) {
                lowQ = 10;
                highQ = 95;
                std::vector<uint8_t> roundBest;

                for (int iter = 0; iter < 6 && lowQ <= highQ; ++iter) {
                    int midQ = lowQ + (highQ - lowQ) / 2;
                    std::vector<uint8_t> cand = encodeAll(midQ, currentScale);
                    if (cand.size() <= safeMaxBytes) {
                        roundBest = std::move(cand);
                        lowQ = midQ + 1;
                    } else {
                        highQ = midQ - 1;
                    }
                }

                if (!roundBest.empty()) {
                    bestPdf = std::move(roundBest);
                    break;
                }
                currentScale *= 0.80; // Scale resolution down if quality alone exceeds limit
            }

            if (bestPdf.empty()) {
                bestPdf = encodeAll(10, 0.50);
            }
        }

        if (bestPdf.empty()) return false;

        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(reinterpret_cast<const char*>(bestPdf.data()), bestPdf.size());
        return true;
    }

    bool pdfToImages(const std::string& pdfPath,
                     const std::string& outputDir,
                     int targetDpi,
                     std::vector<std::string>& outImagePaths) override {
        // Fallback / extraction: checks if PDF has embedded images or page streams
        std::ifstream in(pdfPath, std::ios::binary);
        if (!in.is_open()) return false;

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();

        // Simple stream extraction of embedded JPEGs
        // Look for JPEG SOI FF D8 and EOI FF D9
        int extractedCount = 0;
        size_t pos = 0;
        while (pos + 4 < buffer.size()) {
            if (buffer[pos] == 0xFF && buffer[pos + 1] == 0xD8) {
                // Find EOI
                size_t eoi = pos + 2;
                while (eoi + 1 < buffer.size()) {
                    if (buffer[eoi] == 0xFF && buffer[eoi + 1] == 0xD9) {
                        break;
                    }
                    eoi++;
                }

                if (eoi + 1 < buffer.size()) {
                    size_t len = eoi + 2 - pos;
                    if (len > 1024) { // Valid image stream
                        extractedCount++;
                        std::string outImg = outputDir + "/page_" + std::to_string(extractedCount) + ".jpg";
                        std::ofstream imgOut(outImg, std::ios::binary | std::ios::trunc);
                        if (imgOut.is_open()) {
                            imgOut.write(reinterpret_cast<const char*>(buffer.data() + pos), len);
                            outImagePaths.push_back(outImg);
                        }
                    }
                    pos = eoi + 2;
                    continue;
                }
            }
            pos++;
        }

        return !outImagePaths.empty();
    }

    bool compressPdf(const std::string& inputPdfPath,
                     const std::string& outputPath,
                     int targetDpi,
                     size_t maxBytes,
                     bool greyscale) override {
        std::vector<std::string> extractedPages;
        auto tempDir = std::filesystem::temp_directory_path() /
            ("edgedrop_pdf_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(tempDir);

        if (!pdfToImages(inputPdfPath, tempDir.string(), targetDpi, extractedPages)) {
            std::error_code ec;
            std::filesystem::copy_file(inputPdfPath, outputPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove_all(tempDir, ec);
            return true;
        }

        bool ok = imagesToPdf(extractedPages, outputPath, targetDpi, maxBytes, 0, greyscale);
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
        return ok;
    }
};

std::unique_ptr<IPdfEngine> PdfProcessor::createEngine() {
    return std::make_unique<DefaultNativePdfEngine>();
}

JobResult PdfProcessor::processPdfJob(const JobRequest& request) {
    auto startTime = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = request.inputFilePath;

    if (!std::filesystem::exists(request.inputFilePath)) {
        result.success = false;
        result.errorMessage = "Input PDF file does not exist: " + request.inputFilePath.string();
        return result;
    }
    result.inputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(request.inputFilePath));

    auto outDir = request.outputDirectory.empty()
        ? (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads")
        : request.outputDirectory;
    std::filesystem::create_directories(outDir);

    std::string stem = request.customFileName.empty() ? request.inputFilePath.stem().string() : request.customFileName;
    std::string presetOrFmt = !request.presetId.empty() ? request.presetId : "pdf";
    int targetKbVal = request.targetKb > 0 ? request.targetKb : (request.maxKb > 0 ? request.maxKb : 450);

    std::filesystem::path fullPath;
    int counter = 0;
    do {
        std::string sfx = (counter == 0) ? "" : " (" + std::to_string(counter) + ")";
        fullPath = outDir / (stem + "_" + presetOrFmt + "_" + std::to_string(targetKbVal) + "kb" + sfx + ".pdf");
        counter++;
    } while (std::filesystem::exists(fullPath));

    auto engine = createEngine();
    size_t maxBytes = request.maxKb > 0 ? static_cast<size_t>(request.maxKb) * 1024 : 0;
    size_t minBytes = request.minKb > 0 ? static_cast<size_t>(request.minKb) * 1024 : 0;

    bool ok = false;
    std::string ext = request.inputFilePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".pdf") {
        // Compress existing PDF
        ok = engine->compressPdf(request.inputFilePath.string(),
                                 fullPath.string(),
                                 request.dpi,
                                 maxBytes,
                                 request.greyscale);
    } else {
        // Image -> PDF
        std::vector<std::string> imgs = {request.inputFilePath.string()};
        ok = engine->imagesToPdf(imgs,
                                 fullPath.string(),
                                 request.dpi,
                                 maxBytes,
                                 minBytes,
                                 request.greyscale);
    }

    auto endTime = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    if (!ok) {
        result.success = false;
        result.errorMessage = "Failed to process PDF job.";
        return result;
    }

    result.success = true;
    result.outputFilePath = fullPath;
    result.outputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(fullPath));
    result.outputDpi = request.dpi;
    result.outputFormat = "PDF";
    result.outputWidth = request.targetWidth;
    result.outputHeight = request.targetHeight;
    return result;
}

} // namespace EdgeDrop
