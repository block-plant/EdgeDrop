#pragma once

#include "types.hpp"
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <filesystem>

namespace EdgeDrop {

// ── Subprocess Runner Interface ──────────────────────────────────────────────

struct SubprocessResult {
    int exitCode = -1;
    std::string stdOut;
    std::string stdErr;
    bool timedOut = false;
    bool cancelled = false;
};

class ISubprocessRunner {
public:
    virtual ~ISubprocessRunner() = default;
    virtual SubprocessResult run(const std::string& program,
                                 const std::vector<std::string>& args,
                                 ProgressCallback progress = nullptr,
                                 CancelToken cancel = nullptr) = 0;
};

// ── Native Image Codec Interface ─────────────────────────────────────────────

class INativeImageCodec {
public:
    virtual ~INativeImageCodec() = default;
    virtual bool canHandle(DetectedFormat fmt) const = 0;
    virtual bool decodeToRgba(const std::filesystem::path& path,
                              std::vector<uint8_t>& outRgba,
                              int& width, int& height) = 0;
    virtual bool encodeHeic(const uint8_t* rgba,
                            int width, int height,
                            size_t targetBytes,
                            std::vector<uint8_t>& outBytes) = 0;
};

// ── Compressor Interface ─────────────────────────────────────────────────────

class ICompressor {
public:
    virtual ~ICompressor() = default;
    virtual bool canHandle(DetectedFormat fmt) const = 0;
    virtual JobResult compress(const JobRequest& req,
                               ProgressCallback progress,
                               CancelToken cancel) = 0;
};

} // namespace EdgeDrop
