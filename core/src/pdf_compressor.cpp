#include "edgedrop/pdf_compressor.hpp"
#include "edgedrop/image_compressor.hpp"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"
#include "default_subprocess_runner.hpp"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/Constants.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace EdgeDrop {

static std::shared_ptr<ISubprocessRunner> s_pdfSubprocessRunner = nullptr;

void PdfCompressor::setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner) {
    s_pdfSubprocessRunner = runner;
}

static std::shared_ptr<ISubprocessRunner> getPdfRunner() {
    if (!s_pdfSubprocessRunner) {
        s_pdfSubprocessRunner = std::make_shared<DefaultSubprocessRunner>();
    }
    return s_pdfSubprocessRunner;
}

bool PdfCompressor::canHandle(DetectedFormat fmt) const {
    return fmt == DetectedFormat::PDF;
}

// ── Locate Ghostscript (gs) ───────────────────────────────────────────────────

static std::string locateGhostscript() {
    std::vector<std::string> candidates = {
        "/opt/homebrew/bin/gs",
        "/usr/local/bin/gs",
        "/usr/bin/gs",
#ifdef _WIN32
        "C:\\Program Files\\gs\\gs10.02.1\\bin\\gswin64c.exe",
        "C:\\Program Files\\gs\\gs10.03.0\\bin\\gswin64c.exe",
        "gswin64c.exe",
#endif
        "gs"
    };

    auto runner = getPdfRunner();
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) {
            return c;
        }
    }

    // Try executing "gs --version"
    auto res = runner->run("gs", {"--version"});
    if (res.exitCode == 0) {
        return "gs";
    }
#ifdef _WIN32
    auto resWin = runner->run("gswin64c.exe", {"--version"});
    if (resWin.exitCode == 0) {
        return "gswin64c.exe";
    }
#endif
    return "";
}

static size_t hashBuffer(const uint8_t* data, size_t len) {
    size_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

// ── Tier 1: Fixed Deep libqpdf Structural Compression ─────────────────────────

static bool tier1Compress(const std::filesystem::path& inPath,
                          const std::filesystem::path& outPath,
                          int64_t targetBytes,
                          ProgressCallback progress,
                          CancelToken cancel,
                          int overrideJpegQ = 0) {
    try {
        QPDF pdf;
        pdf.processFile(inPath.string().c_str());

        // Find mask object IDs so we do NOT compress masks
        std::unordered_set<int> maskObjIds;
        for (auto& obj : pdf.getAllObjects()) {
            if (!obj.isStream()) continue;
            auto dict = obj.getDict();
            if (!dict.isDictionary()) continue;

            if (dict.hasKey("/ImageMask")) {
                auto m = dict.getKey("/ImageMask");
                if (m.isBool() && m.getBoolValue()) {
                    maskObjIds.insert(obj.getObjGen().getObj());
                }
            }
            if (dict.hasKey("/Mask")) {
                auto m = dict.getKey("/Mask");
                if (m.isIndirect()) {
                    maskObjIds.insert(m.getObjGen().getObj());
                }
            }
            if (dict.hasKey("/SMask")) {
                auto sm = dict.getKey("/SMask");
                if (sm.isIndirect()) {
                    maskObjIds.insert(sm.getObjGen().getObj());
                }
            }
        }

        // Collect all compressible image XObjects across pages and nested /Form XObjects
        struct ImageObjInfo {
            QPDFObjectHandle handle;
            int width = 0;
            int height = 0;
            size_t rawSize = 0;
            bool isDct = false;
        };

        std::vector<ImageObjInfo> images;
        std::unordered_map<size_t, QPDFObjectHandle> deduplicatedStreams;

        constexpr size_t kMaxEmbeddedImages = 2048;
        for (auto& obj : pdf.getAllObjects()) {
            if (images.size() >= kMaxEmbeddedImages) break;
            if (cancel && cancel->load()) return false;
            if (!obj.isStream()) continue;
            auto dict = obj.getDict();
            if (!dict.isDictionary()) continue;
            if (!dict.hasKey("/Subtype")) continue;
            auto subtype = dict.getKey("/Subtype");
            if (!subtype.isName() || subtype.getName() != "/Image") continue;

            int objId = obj.getObjGen().getObj();
            if (maskObjIds.count(objId)) continue; // Skip masks themselves

            int w = 0, h = 0;
            if (dict.hasKey("/Width") && dict.getKey("/Width").isInteger()) {
                w = static_cast<int>(dict.getKey("/Width").getIntValue());
            }
            if (dict.hasKey("/Height") && dict.getKey("/Height").isInteger()) {
                h = static_cast<int>(dict.getKey("/Height").getIntValue());
            }
            if (w <= 16 || h <= 16) continue; // Skip tiny icons/dots

            bool isDct = false;
            if (dict.hasKey("/Filter")) {
                auto filter = dict.getKey("/Filter");
                if (filter.isName() && filter.getName() == "/DCTDecode") {
                    isDct = true;
                } else if (filter.isArray()) {
                    for (int i = 0; i < filter.getArrayNItems(); ++i) {
                        if (filter.getArrayItem(i).isName() && filter.getArrayItem(i).getName() == "/DCTDecode") {
                            isDct = true;
                            break;
                        }
                    }
                }
            }

            size_t rawLen = 0;
            std::shared_ptr<Buffer> rawBuf = obj.getRawStreamData();
            if (rawBuf) rawLen = rawBuf->getSize();

            images.push_back({obj, w, h, rawLen, isDct});
        }

        if (progress) progress(0.3);

        int targetJpegQ = 65;
        if (overrideJpegQ > 0) {
            targetJpegQ = overrideJpegQ;
        } else if (targetBytes > 0) {
            int64_t inSize = static_cast<int64_t>(std::filesystem::file_size(inPath));
            if (inSize > 0) {
                double ratio = static_cast<double>(targetBytes) / static_cast<double>(inSize);
                if (ratio < 0.25) targetJpegQ = 35;
                else if (ratio < 0.50) targetJpegQ = 50;
                else if (ratio < 0.75) targetJpegQ = 65;
                else targetJpegQ = 78;
            }
        }

        for (auto& info : images) {
            if (cancel && cancel->load()) return false;
            cv::Mat decoded;

            if (info.isDct) {
                std::shared_ptr<Buffer> buf = info.handle.getRawStreamData();
                if (buf && buf->getSize() > 256) {
                    std::vector<uint8_t> rawVec(buf->getBuffer(), buf->getBuffer() + buf->getSize());
                    decoded = cv::imdecode(rawVec, cv::IMREAD_COLOR);
                }
            } else {
                std::shared_ptr<Buffer> buf = info.handle.getStreamData(qpdf_dl_all);
                if (buf && buf->getSize() > 256) {
                    auto dict = info.handle.getDict();
                    int bpc = 8;
                    if (dict.hasKey("/BitsPerComponent") && dict.getKey("/BitsPerComponent").isInteger()) {
                        bpc = static_cast<int>(dict.getKey("/BitsPerComponent").getIntValue());
                    }

                    if (bpc == 8 && info.width > 0 && info.height > 0) {
                        size_t expectedRgb = static_cast<size_t>(info.width * info.height * 3);
                        size_t expectedCmyk = static_cast<size_t>(info.width * info.height * 4);
                        size_t expectedGray = static_cast<size_t>(info.width * info.height);

                        if (buf->getSize() >= expectedRgb) {
                            cv::Mat rgb(info.height, info.width, CV_8UC3, buf->getBuffer());
                            cv::cvtColor(rgb, decoded, cv::COLOR_RGB2BGR);
                        } else if (buf->getSize() >= expectedCmyk) {
                            cv::Mat cmyk(info.height, info.width, CV_8UC4, buf->getBuffer());
                            // Convert CMYK to BGR
                            decoded.create(info.height, info.width, CV_8UC3);
                            for (int y = 0; y < info.height; ++y) {
                                const uint8_t* pC = cmyk.ptr<uint8_t>(y);
                                uint8_t* pB = decoded.ptr<uint8_t>(y);
                                for (int x = 0; x < info.width; ++x) {
                                    float c = pC[x * 4 + 0] / 255.f;
                                    float m = pC[x * 4 + 1] / 255.f;
                                    float ycol = pC[x * 4 + 2] / 255.f;
                                    float k = pC[x * 4 + 3] / 255.f;
                                    pB[x * 3 + 2] = static_cast<uint8_t>(255.f * (1.f - c) * (1.f - k)); // R
                                    pB[x * 3 + 1] = static_cast<uint8_t>(255.f * (1.f - m) * (1.f - k)); // G
                                    pB[x * 3 + 0] = static_cast<uint8_t>(255.f * (1.f - ycol) * (1.f - k)); // B
                                }
                            }
                        } else if (buf->getSize() >= expectedGray) {
                            cv::Mat gray(info.height, info.width, CV_8UC1, buf->getBuffer());
                            cv::cvtColor(gray, decoded, cv::COLOR_GRAY2BGR);
                        }
                    }
                }
            }

            if (decoded.empty()) continue;

            // Power-law downscale if image is high resolution (limit to max 1280px or 800px depending on budget)
            int maxLongEdge = (targetJpegQ <= 40) ? 960 : 1600;
            int longEdge = std::max(decoded.cols, decoded.rows);
            if (longEdge > maxLongEdge) {
                double scale = static_cast<double>(maxLongEdge) / static_cast<double>(longEdge);
                int nw = std::max(320, static_cast<int>(decoded.cols * scale));
                int nh = std::max(320, static_cast<int>(decoded.rows * scale));
                cv::resize(decoded, decoded, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);
            }

            std::vector<uint8_t> compressedJpg;
            std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, targetJpegQ, cv::IMWRITE_JPEG_OPTIMIZE, 1};
            cv::imencode(".jpg", decoded, compressedJpg, params);

            if (compressedJpg.empty()) continue;

            // Deduplication check
            size_t streamHash = hashBuffer(compressedJpg.data(), compressedJpg.size());
            if (deduplicatedStreams.count(streamHash)) {
                auto existing = deduplicatedStreams[streamHash];
                info.handle.replaceStreamData(
                    existing.getRawStreamData(),
                    existing.getDict().getKey("/Filter"),
                    QPDFObjectHandle::newNull()
                );
                auto dict = info.handle.getDict();
                dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
                dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
                dict.replaceKey("/Width",  QPDFObjectHandle::newInteger(decoded.cols));
                dict.replaceKey("/Height", QPDFObjectHandle::newInteger(decoded.rows));
                continue;
            }

            // Only replace stream if saving at least 5% (idempotency guard against endless degradation)
            if (info.rawSize == 0 || compressedJpg.size() < static_cast<size_t>(info.rawSize * 0.95)) {
                info.handle.replaceStreamData(
                    std::string(reinterpret_cast<const char*>(compressedJpg.data()), compressedJpg.size()),
                    QPDFObjectHandle::newName("/DCTDecode"),
                    QPDFObjectHandle::newNull()
                );
                auto dict = info.handle.getDict();
                dict.replaceKey("/ColorSpace", QPDFObjectHandle::newName("/DeviceRGB"));
                dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
                dict.replaceKey("/Width",  QPDFObjectHandle::newInteger(decoded.cols));
                dict.replaceKey("/Height", QPDFObjectHandle::newInteger(decoded.rows));
                deduplicatedStreams[streamHash] = info.handle;
            }
        }

        if (progress) progress(0.6);

        // Write using QPDFWriter with deep flate recompression
        QPDFWriter writer(pdf, outPath.string().c_str());
        writer.setStaticID(true);
        writer.setStreamDataMode(qpdf_s_compress);
        writer.setObjectStreamMode(qpdf_o_generate);
        writer.setRecompressFlate(true);
        writer.setPreserveUnreferencedObjects(false);
        writer.write();
        return true;
    } catch (...) {
        return false;
    }
}

// ── Optimize with QPDF (Lossless Object Stream Compression) ───────────────────

static bool optimizeWithQpdf(const std::filesystem::path& inPdf, const std::filesystem::path& outPdf) {
    try {
        QPDF pdf;
        pdf.processFile(inPdf.string().c_str());
        QPDFWriter writer(pdf, outPdf.string().c_str());
        writer.setStaticID(true);
        writer.setStreamDataMode(qpdf_s_compress);
        writer.setObjectStreamMode(qpdf_o_generate);
        writer.setRecompressFlate(true);
        writer.setPreserveUnreferencedObjects(false);
        writer.write();
        return std::filesystem::exists(outPdf) && std::filesystem::file_size(outPdf) > 0;
    } catch (...) {
        return false;
    }
}

// ── Tier 2: Ghostscript (gs) Deep Progressive Passes ──────────────────────────

static bool tier2GhostscriptPass(const std::string& gsBin,
                                 const std::filesystem::path& inPath,
                                 const std::filesystem::path& outPath,
                                 int dpi, int jpegQuality,
                                 CancelToken cancel) {
    auto runner = getPdfRunner();
    std::string absOut = std::filesystem::absolute(outPath).string();
    std::string absIn = std::filesystem::absolute(inPath).string();
    std::vector<std::string> args = {
        "-sDEVICE=pdfwrite",
        "-dSAFER",
        "-dNOOUTERSAVE",
        "-dCompatibilityLevel=1.4",
        "-dNOPAUSE",
        "-dQUIET",
        "-dBATCH",
        "-dDetectDuplicateImages=true",
        "-dSubsetFonts=true",
        "-dCompressFonts=true",
        "-dEmbedAllFonts=true",
        "-dDownsampleColorImages=true",
        "-dDownsampleGrayImages=true",
        "-dDownsampleMonoImages=true",
        "-dEncodeColorImages=true",
        "-dEncodeGrayImages=true",
        "-dEncodeMonoImages=true",
        "-dColorImageDownsampleType=/Bicubic",
        "-dGrayImageDownsampleType=/Bicubic",
        "-dMonoImageDownsampleType=/Bicubic",
        "-dColorImageDownsampleThreshold=1.0",
        "-dGrayImageDownsampleThreshold=1.0",
        "-dMonoImageDownsampleThreshold=1.0",
        "-dColorImageResolution=" + std::to_string(dpi),
        "-dGrayImageResolution=" + std::to_string(dpi),
        "-dMonoImageResolution=" + std::to_string(dpi),
        "-dAutoFilterColorImages=false",
        "-dColorImageFilter=/DCTEncode",
        "-dJPEGQ=" + std::to_string(jpegQuality),
        "-sOutputFile=" + absOut,
        "--",
        absIn
    };

    auto res = runner->run(gsBin, args, nullptr, cancel);
    if (res.cancelled || res.exitCode != 0) {
        std::error_code ec;
        std::filesystem::remove(outPath, ec);
        return false;
    }
    if (!std::filesystem::exists(outPath) || std::filesystem::file_size(outPath) == 0) {
        return false;
    }

    // Chain Ghostscript -> libqpdf object streams + flate recompression in same run
    std::filesystem::path optPath = outPath;
    optPath += ".qpdf.tmp";
    if (optimizeWithQpdf(outPath, optPath)) {
        if (std::filesystem::file_size(optPath) < std::filesystem::file_size(outPath)) {
            std::error_code ec;
            std::filesystem::rename(optPath, outPath, ec);
        } else {
            std::error_code ec;
            std::filesystem::remove(optPath, ec);
        }
    }

    return true;
}

// ── Main Compression Entry Point ──────────────────────────────────────────────

JobResult PdfCompressor::compress(const JobRequest& req,
                                  ProgressCallback progress,
                                  CancelToken cancel) {
    auto t0 = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = req.inputFilePath;

    if (!std::filesystem::exists(req.inputFilePath)) {
        result.success = false;
        result.errorMessage = "File not found: " + req.inputFilePath.string();
        return result;
    }

    result.inputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(req.inputFilePath));
    if (result.inputSizeBytes == 0) {
        result.success = false;
        result.errorMessage = "Input PDF is empty (0 bytes).";
        return result;
    }

    auto outDir = req.outputDirectory.empty()
        ? (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads")
        : req.outputDirectory;
    std::filesystem::create_directories(outDir);

    std::string stem = req.inputFilePath.stem().string();
    std::filesystem::path outPath;
    int ctr = 0;
    do {
        std::string sfx = (ctr == 0) ? "" : " (" + std::to_string(ctr) + ")";
        outPath = outDir / (stem + "_compressed" + sfx + ".pdf");
        ++ctr;
    } while (std::filesystem::exists(outPath));

    // ── Pass-Through Guard ────────────────────────────────────────────────────
    if (req.targetBytes > 0 && result.inputSizeBytes <= req.targetBytes) {
        std::error_code ec;
        std::filesystem::copy_file(req.inputFilePath, outPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.success = true;
            result.outputFilePath = outPath;
            result.outputSizeBytes = result.inputSizeBytes;
            result.outputFormat = "PDF";
            result.hitFloor = true;
            result.floorNote = "Already smaller than your target size.";
            auto t1 = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (progress) progress(1.0);
            return result;
        }
    }

    // ── Check Password Protection ─────────────────────────────────────────────
    int originalPageCount = 0;
    try {
        QPDF checkPdf;
        checkPdf.processFile(req.inputFilePath.string().c_str());
        if (checkPdf.isEncrypted()) {
            result.success = false;
            result.hitFloor = true;
            result.errorMessage = "This PDF is password-protected and cannot be compressed.";
            return result;
        }
        QPDFPageDocumentHelper pdh(checkPdf);
        originalPageCount = static_cast<int>(pdh.getAllPages().size());
    } catch (const QPDFExc&) {
        result.success = false;
        result.errorMessage = "This PDF is password-protected and cannot be compressed.";
        return result;
    } catch (...) {
        result.success = false;
        result.errorMessage = "Failed to parse PDF: " + req.inputFilePath.string();
        return result;
    }

    if (progress) progress(0.1);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    // ── Execute Tier 1: Fixed Deep libqpdf Compression ────────────────────────
    std::string tmpName1 = ".edgedrop.tmp.t1." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pdf";
    std::filesystem::path tier1TmpPath = outDir / tmpName1;

    bool t1Success = tier1Compress(req.inputFilePath, tier1TmpPath, req.targetBytes, progress, cancel);
    int64_t t1Size = t1Success ? static_cast<int64_t>(std::filesystem::file_size(tier1TmpPath)) : 0;

    std::filesystem::path bestCandidate = tier1TmpPath;
    int64_t bestSize = t1Size;

    if (progress) progress(0.7);

    // ── Execute Tier 2: Ghostscript Fallback if targetBytes not met ───────────
    std::string gsBin = locateGhostscript();

    if (req.targetBytes > 0 && (bestSize == 0 || bestSize > req.targetBytes) && !gsBin.empty()) {
        struct GsPreset { int dpi; int q; };
        std::vector<GsPreset> presets = {
            {150, 75}, // Pass A: Fine / high fidelity
            {130, 65}, // Pass B: High quality standard
            {110, 55}, // Pass C: Balanced
            {96,  45}, // Pass D: Medium
            {80,  35}, // Pass E: Compact
            {72,  30}, // Pass F: Floor
            {72,  22}  // Pass G: Deep floor
        };

        const auto& gsInput = (t1Success && t1Size > 0) ? tier1TmpPath : req.inputFilePath;
        for (const auto& pr : presets) {
            if (cancel && cancel->load()) break;
            std::string tmpName2 = ".edgedrop.tmp.t2." + std::to_string(pr.dpi) + "." +
                                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pdf";
            std::filesystem::path tier2TmpPath = outDir / tmpName2;

            if (tier2GhostscriptPass(gsBin, gsInput, tier2TmpPath, pr.dpi, pr.q, cancel)) {
                // Verify output page count
                int t2Pages = 0;
                try {
                    QPDF check;
                    check.processFile(tier2TmpPath.string().c_str());
                    QPDFPageDocumentHelper pdh(check);
                    t2Pages = static_cast<int>(pdh.getAllPages().size());
                } catch (...) {
                    t2Pages = 0;
                }

                if (t2Pages == originalPageCount) {
                    // Chain GS output -> Tier 1 libqpdf image & object stream pass
                    std::string t1PostName = tier2TmpPath.string() + ".t1post.pdf";
                    std::filesystem::path t1PostPath(t1PostName);
                    if (tier1Compress(tier2TmpPath, t1PostPath, req.targetBytes, nullptr, cancel)) {
                        if (std::filesystem::exists(t1PostPath) &&
                            std::filesystem::file_size(t1PostPath) < std::filesystem::file_size(tier2TmpPath)) {
                            std::error_code ec;
                            std::filesystem::rename(t1PostPath, tier2TmpPath, ec);
                        } else {
                            std::error_code ec;
                            std::filesystem::remove(t1PostPath, ec);
                        }
                    }

                    int64_t t2Size = static_cast<int64_t>(std::filesystem::file_size(tier2TmpPath));
                    if (bestSize == 0 || t2Size < bestSize) {
                        if (bestCandidate != tier1TmpPath && std::filesystem::exists(bestCandidate)) {
                            std::error_code ec;
                            std::filesystem::remove(bestCandidate, ec);
                        }
                        bestCandidate = tier2TmpPath;
                        bestSize = t2Size;
                    } else {
                        std::error_code ec;
                        std::filesystem::remove(tier2TmpPath, ec);
                    }

                    if (bestSize <= req.targetBytes) {
                        break; // Met target
                    }
                } else {
                    std::error_code ec;
                    std::filesystem::remove(tier2TmpPath, ec);
                }
            }
        }
    } else if (req.targetBytes > 0 && (bestSize == 0 || bestSize > req.targetBytes) && gsBin.empty()) {
        // Fallback Tier 1 second pass at deeper compression (Q=35) when Ghostscript is absent
        std::string tmpName1b = ".edgedrop.tmp.t1b." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".pdf";
        std::filesystem::path tier1bTmpPath = outDir / tmpName1b;
        if (tier1Compress(req.inputFilePath, tier1bTmpPath, req.targetBytes, nullptr, cancel, 35)) {
            int64_t t1bSize = static_cast<int64_t>(std::filesystem::file_size(tier1bTmpPath));
            if (t1bSize > 0 && (bestSize == 0 || t1bSize < bestSize)) {
                if (bestCandidate != tier1TmpPath && std::filesystem::exists(bestCandidate)) {
                    std::error_code ec;
                    std::filesystem::remove(bestCandidate, ec);
                }
                bestCandidate = tier1bTmpPath;
                bestSize = t1bSize;
            } else {
                std::error_code ec;
                std::filesystem::remove(tier1bTmpPath, ec);
            }
        }
    }

    if (cancel && cancel->load()) {
        std::error_code ec;
        std::filesystem::remove(tier1TmpPath, ec);
        if (bestCandidate != tier1TmpPath) std::filesystem::remove(bestCandidate, ec);
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    if (bestSize == 0 || !std::filesystem::exists(bestCandidate)) {
        std::error_code ec;
        std::filesystem::remove(tier1TmpPath, ec);
        result.success = false;
        result.errorMessage = "PDF compression produced invalid output.";
        return result;
    }

    // Idempotency Guard: if output saves < 5% of input, verify whether to pass through original
    if (bestSize >= static_cast<int64_t>(result.inputSizeBytes * 0.95)) {
        if (bestSize >= result.inputSizeBytes) {
            std::error_code ec;
            std::filesystem::remove(tier1TmpPath, ec);
            if (bestCandidate != tier1TmpPath) std::filesystem::remove(bestCandidate, ec);
            std::filesystem::copy_file(req.inputFilePath, outPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.success = true;
            result.outputFilePath = outPath;
            result.outputSizeBytes = result.inputSizeBytes;
            result.outputFormat = "PDF";
            result.hitFloor = (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes);
            if (result.hitFloor) {
                result.floorNote = "Reached maximum compression floor (72 DPI) while maintaining vector text legibility.";
            }
            auto t1 = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (progress) progress(1.0);
            return result;
        }
    }

    // Atomically rename bestCandidate to outPath
    std::error_code ec;
    std::filesystem::rename(bestCandidate, outPath, ec);
    if (ec) {
        std::filesystem::remove(bestCandidate, ec);
        std::filesystem::remove(tier1TmpPath, ec);
        result.success = false;
        result.errorMessage = "Failed to write output PDF: " + outPath.string();
        return result;
    }

    if (bestCandidate != tier1TmpPath && std::filesystem::exists(tier1TmpPath)) {
        std::filesystem::remove(tier1TmpPath, ec);
    }

    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(outPath));
    result.outputFormat = "PDF";

    if (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes) {
        result.hitFloor = true;
        if (gsBin.empty()) {
            result.floorNote = "Compressed to visual limit via libqpdf. Install Ghostscript (gs) for deeper font subsetting and object stream compression.";
        } else {
            result.floorNote = "Reached maximum compression floor (72 DPI) while maintaining vector text legibility.";
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
