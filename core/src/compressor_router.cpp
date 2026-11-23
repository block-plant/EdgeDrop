#include "edgedrop/compressor_router.hpp"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"
#include "edgedrop/image_compressor.hpp"
#include "edgedrop/pdf_compressor.hpp"
#include "edgedrop/office_compressor.hpp"
#include "edgedrop/media_compressor.hpp"

namespace EdgeDrop {

void CompressorRouter::setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner) {
    PdfCompressor::setSubprocessRunner(runner);
    MediaCompressor::setSubprocessRunner(runner);
}

void CompressorRouter::setNativeImageCodec(std::shared_ptr<INativeImageCodec> codec) {
    ImageCompressor::setNativeCodec(codec);
}

JobResult CompressorRouter::route(const JobRequest& req,
                                  ProgressCallback cb,
                                  CancelToken cancel) {
    JobResult failResult;
    failResult.inputFilePath = req.inputFilePath;
    failResult.success = false;

    // Zero-Trust Input & Filesystem Validation
    std::string pathErr = SecurityGuards::validateInputPath(req.inputFilePath);
    if (!pathErr.empty()) {
        failResult.errorMessage = pathErr;
        return failResult;
    }

    if (!req.outputDirectory.empty()) {
        std::string dirErr = SecurityGuards::validateOutputDir(req.outputDirectory, req.inputFilePath);
        if (!dirErr.empty()) {
            failResult.errorMessage = dirErr;
            return failResult;
        }
    }

    JobRequest resolvedReq = req;
    if (resolvedReq.detectedFormat == DetectedFormat::Unknown) {
        resolvedReq.detectedFormat = FormatDetector::detect(resolvedReq.inputFilePath);
    }

    static ImageCompressor  s_imageCompressor;
    static PdfCompressor    s_pdfCompressor;
    static OfficeCompressor s_officeCompressor;
    static MediaCompressor  s_mediaCompressor;

    if (s_imageCompressor.canHandle(resolvedReq.detectedFormat)) {
        return s_imageCompressor.compress(resolvedReq, cb, cancel);
    }
    if (s_pdfCompressor.canHandle(resolvedReq.detectedFormat)) {
        return s_pdfCompressor.compress(resolvedReq, cb, cancel);
    }
    if (s_officeCompressor.canHandle(resolvedReq.detectedFormat)) {
        return s_officeCompressor.compress(resolvedReq, cb, cancel);
    }
    if (s_mediaCompressor.canHandle(resolvedReq.detectedFormat)) {
        return s_mediaCompressor.compress(resolvedReq, cb, cancel);
    }

    failResult.errorMessage = "Unsupported file format: " + req.inputFilePath.extension().string();
    return failResult;
}

} // namespace EdgeDrop
