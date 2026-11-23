#include "CompressorRouter.hpp"
#include "ImageCompressor.hpp"
#include "PdfCompressor.hpp"
#include "OfficeCompressor.hpp"
#include "MediaCompressor.hpp"

namespace EdgeDrop {

JobResult CompressorRouter::route(const JobRequest& req,
                                  ProgressCallback cb,
                                  CancelToken cancel) {
    // Static instances (thread-safe: each call creates its own state via compress())
    static ImageCompressor  s_image;
    static PdfCompressor    s_pdf;
    static OfficeCompressor s_office;
    static MediaCompressor  s_media;

    if (s_image.canHandle(req.detectedFormat))  return s_image.compress(req, cb, cancel);
    if (s_pdf.canHandle(req.detectedFormat))    return s_pdf.compress(req, cb, cancel);
    if (s_office.canHandle(req.detectedFormat)) return s_office.compress(req, cb, cancel);
    if (s_media.canHandle(req.detectedFormat))  return s_media.compress(req, cb, cancel);

    JobResult err;
    err.success = false;
    err.inputFilePath = req.inputFilePath;
    err.errorMessage = "Unsupported file format. Supported: JPG, PNG, WebP, HEIC, TIFF, BMP, GIF, PDF, DOCX, PPTX, XLSX, MP4, MOV, MKV, WebM, MP3, M4A, AAC, OGG, WAV.";
    return err;
}

} // namespace EdgeDrop
