#pragma once

#include "ProcessingTypes.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace EdgeDrop {

class IPdfEngine {
public:
    virtual ~IPdfEngine() = default;

    virtual bool imagesToPdf(const std::vector<std::string>& imagePaths,
                             const std::string& outputPath,
                             int targetDpi,
                             size_t maxBytes,
                             size_t minBytes,
                             bool greyscale) = 0;

    virtual bool pdfToImages(const std::string& pdfPath,
                             const std::string& outputDir,
                             int targetDpi,
                             std::vector<std::string>& outImagePaths) = 0;

    virtual bool compressPdf(const std::string& inputPdfPath,
                             const std::string& outputPath,
                             int targetDpi,
                             size_t maxBytes,
                             bool greyscale) = 0;
};

class PdfProcessor {
public:
    // Main entry point for PDF JobRequests
    static JobResult processPdfJob(const JobRequest& request);

    // Native PDF generation: builds valid standard PDF containing JPEG-compressed page streams
    static std::vector<uint8_t> buildPdfFromJpegStreams(
        const std::vector<std::pair<std::vector<uint8_t>, std::pair<int, int>>>& pageJpegStreams,
        bool isGreyscale = false
    );

    // Factory method for active engine
    static std::unique_ptr<IPdfEngine> createEngine();
};

} // namespace EdgeDrop
