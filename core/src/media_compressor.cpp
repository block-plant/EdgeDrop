#include "edgedrop/media_compressor.hpp"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"
#include "default_subprocess_runner.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace EdgeDrop {

static std::shared_ptr<ISubprocessRunner> s_mediaRunner = nullptr;

void MediaCompressor::setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner) {
    s_mediaRunner = runner;
}

static std::shared_ptr<ISubprocessRunner> getMediaRunner() {
    if (!s_mediaRunner) {
        s_mediaRunner = std::make_shared<DefaultSubprocessRunner>();
    }
    return s_mediaRunner;
}

std::string MediaCompressor::locateFfmpeg() {
    std::vector<std::string> candidates = {
        "/opt/homebrew/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/usr/bin/ffmpeg",
#ifdef _WIN32
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "ffmpeg.exe",
#endif
        "ffmpeg"
    };
    auto runner = getMediaRunner();
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    auto res = runner->run("ffmpeg", {"-version"});
    if (res.exitCode == 0) return "ffmpeg";
    return "";
}

std::string MediaCompressor::locateFfprobe() {
    std::vector<std::string> candidates = {
        "/opt/homebrew/bin/ffprobe",
        "/usr/local/bin/ffprobe",
        "/usr/bin/ffprobe",
#ifdef _WIN32
        "C:\\ffmpeg\\bin\\ffprobe.exe",
        "ffprobe.exe",
#endif
        "ffprobe"
    };
    auto runner = getMediaRunner();
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    auto res = runner->run("ffprobe", {"-version"});
    if (res.exitCode == 0) return "ffprobe";
    return "";
}

bool MediaCompressor::canHandle(DetectedFormat fmt) const {
    switch (fmt) {
        case DetectedFormat::MP4:
        case DetectedFormat::MOV:
        case DetectedFormat::MKV:
        case DetectedFormat::WEBM:
        case DetectedFormat::MP3:
        case DetectedFormat::M4A:
        case DetectedFormat::AAC:
        case DetectedFormat::OGG:
        case DetectedFormat::WAV:
        case DetectedFormat::GIF:
            return true;
        default:
            return false;
    }
}

struct MediaInfo {
    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 25.0;
    int audioChannels = 2;
    int sampleRate = 44100;
    bool hasVideo = false;
    bool hasAudio = false;
};

static MediaInfo probeFile(const std::string& ffprobeBin, const std::filesystem::path& path) {
    MediaInfo info;
    auto runner = getMediaRunner();
    std::string safePath = "file:" + std::filesystem::absolute(path).string();
    auto res = runner->run(ffprobeBin, {
        "-v", "quiet",
        "-nostdin",
        "-protocol_whitelist", "file,pipe",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        "--",
        safePath
    });

    if (res.exitCode != 0 || res.stdOut.empty()) return info;

    try {
        auto j = nlohmann::json::parse(res.stdOut);
        if (j.contains("format") && j["format"].contains("duration")) {
            info.duration = std::stod(j["format"]["duration"].get<std::string>());
        }

        if (j.contains("streams") && j["streams"].is_array()) {
            for (const auto& s : j["streams"]) {
                std::string codecType = s.value("codec_type", "");
                if (codecType == "video" && !info.hasVideo) {
                    info.hasVideo = true;
                    info.width = s.value("width", 0);
                    info.height = s.value("height", 0);
                    if (s.contains("r_frame_rate")) {
                        std::string rfr = s["r_frame_rate"];
                        auto slash = rfr.find('/');
                        if (slash != std::string::npos) {
                            double num = std::stod(rfr.substr(0, slash));
                            double den = std::stod(rfr.substr(slash + 1));
                            if (den > 0) info.fps = num / den;
                        }
                    }
                } else if (codecType == "audio" && !info.hasAudio) {
                    info.hasAudio = true;
                    info.audioChannels = s.value("channels", 2);
                    info.sampleRate = std::stoi(s.value("sample_rate", "44100"));
                }
            }
        }
    } catch (...) {}
    return info;
}

JobResult MediaCompressor::compress(const JobRequest& req,
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
        result.errorMessage = "Input media file is empty (0 bytes).";
        return result;
    }

    auto outDir = req.outputDirectory.empty()
        ? (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / "Downloads")
        : req.outputDirectory;
    std::filesystem::create_directories(outDir);

    std::string stem = req.inputFilePath.stem().string();
    std::string ext = FormatDetector::outputExtension(req.detectedFormat);
    if (ext.empty()) ext = req.inputFilePath.extension().string();

    std::filesystem::path outPath;
    int ctr = 0;
    do {
        std::string sfx = (ctr == 0) ? "" : " (" + std::to_string(ctr) + ")";
        outPath = outDir / (stem + "_compressed" + sfx + ext);
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
            result.outputFormat = FormatDetector::formatName(req.detectedFormat);
            auto t1 = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (progress) progress(1.0);
            return result;
        }
    }

    std::string ffmpeg = locateFfmpeg();
    std::string ffprobe = locateFfprobe();
    if (ffmpeg.empty() || ffprobe.empty()) {
        result.success = false;
        result.hitFloor = true;
        result.errorMessage = "ffmpeg and ffprobe are required for media compression. Please install them.";
        return result;
    }

    if (progress) progress(0.05);
    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    MediaInfo info = probeFile(ffprobe, req.inputFilePath);
    if (info.duration <= 0.0) {
        info.duration = 1.0;
    }

    auto runner = getMediaRunner();
    std::string safeIn = "file:" + std::filesystem::absolute(req.inputFilePath).string();
    SecureTempFile tempFile(outDir, ext);
    if (!tempFile.isValid()) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    bool isVideo = (req.detectedFormat == DetectedFormat::MP4 ||
                    req.detectedFormat == DetectedFormat::MOV ||
                    req.detectedFormat == DetectedFormat::MKV ||
                    req.detectedFormat == DetectedFormat::WEBM);

    bool isAudio = (req.detectedFormat == DetectedFormat::MP3 ||
                    req.detectedFormat == DetectedFormat::M4A ||
                    req.detectedFormat == DetectedFormat::AAC ||
                    req.detectedFormat == DetectedFormat::OGG ||
                    req.detectedFormat == DetectedFormat::WAV);

    bool isGif = (req.detectedFormat == DetectedFormat::GIF);

    if (isVideo) {
        // Compute bitrates
        double targetBytes = (req.targetBytes > 0) ? static_cast<double>(req.targetBytes) : static_cast<double>(result.inputSizeBytes * 0.7);
        double rTotal = (targetBytes * 8.0 * 0.95) / info.duration;
        double rA = std::clamp(0.12 * rTotal, 48000.0, 128000.0);
        double rV = std::max(rTotal - rA, 80000.0);

        // BPP Law
        int outW = info.width > 0 ? info.width : 1280;
        int outH = info.height > 0 ? info.height : 720;
        double fps = std::min(info.fps, 30.0);
        double bpp = rV / (static_cast<double>(outW * outH) * fps);

        if (bpp < 0.05) {
            double s = std::sqrt(rV / (0.06 * static_cast<double>(outW * outH) * fps));
            outW = (static_cast<int>(outW * s) / 2) * 2;
            outH = (static_cast<int>(outH * s) / 2) * 2;
            outW = std::max(outW, 320);
            outH = std::max(outH, 240);
        }

        std::string scaleFilter = "scale=" + std::to_string(outW) + ":" + std::to_string(outH) + ",fps=min(30\\,fps)";
        std::filesystem::path passlogDir = outDir / (".passlog_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(passlogDir);
        std::string passlogPrefix = (passlogDir / "ffmpeg2pass").string();

#ifdef _WIN32
        std::string nullDev = "NUL";
#else
        std::string nullDev = "/dev/null";
#endif

        // Pass 1
        std::vector<std::string> pass1Args = {
            "-y",
            "-nostdin",
            "-protocol_whitelist", "file,pipe",
            "-i", safeIn,
            "-c:v", "libx264",
            "-b:v", std::to_string(static_cast<int>(rV)),
            "-pass", "1",
            "-passlogfile", passlogPrefix,
            "-vf", scaleFilter,
            "-an", "-f", "null", nullDev
        };
        runner->run(ffmpeg, pass1Args, nullptr, cancel);

        if (progress) progress(0.5);
        if (cancel && cancel->load()) {
            std::filesystem::remove_all(passlogDir);
            result.success = false; result.errorMessage = "Cancelled."; return result;
        }

        // Pass 2
        std::vector<std::string> pass2Args = {
            "-y",
            "-nostdin",
            "-protocol_whitelist", "file,pipe",
            "-i", safeIn,
            "-c:v", "libx264",
            "-b:v", std::to_string(static_cast<int>(rV)),
            "-pass", "2",
            "-passlogfile", passlogPrefix,
            "-vf", scaleFilter,
            "-c:a", "aac",
            "-b:a", std::to_string(static_cast<int>(rA)),
            tempFile.path().string()
        };
        runner->run(ffmpeg, pass2Args, nullptr, cancel);
        std::filesystem::remove_all(passlogDir);

    } else if (isAudio) {
        double targetBytes = (req.targetBytes > 0) ? static_cast<double>(req.targetBytes) : static_cast<double>(result.inputSizeBytes * 0.7);
        if (req.detectedFormat == DetectedFormat::WAV) {
            // Downmix WAV
            std::vector<std::string> args = {
                "-y",
                "-nostdin",
                "-protocol_whitelist", "file,pipe",
                "-i", safeIn,
                "-ac", "1",
                "-ar", "22050",
                tempFile.path().string()
            };
            runner->run(ffmpeg, args, nullptr, cancel);
        } else {
            double rA = std::clamp((targetBytes * 8.0 * 0.96) / info.duration, 32000.0, 320000.0);
            std::vector<std::string> args = {
                "-y",
                "-nostdin",
                "-protocol_whitelist", "file,pipe",
                "-i", safeIn,
                "-b:a", std::to_string(static_cast<int>(rA))
            };
            if (rA < 64000.0) {
                args.push_back("-ac"); args.push_back("1");
                args.push_back("-ar"); args.push_back(rA < 40000.0 ? "22050" : "32000");
            }
            args.push_back(tempFile.path().string());
            runner->run(ffmpeg, args, nullptr, cancel);
        }
    } else if (isGif) {
        // GIF 2-pass palettegen / paletteuse
        std::vector<std::string> args = {
            "-y",
            "-nostdin",
            "-protocol_whitelist", "file,pipe",
            "-i", safeIn,
            "-vf", "fps=10,scale=iw*0.8:ih*0.8:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=128:stats_mode=diff[p];[s1][p]paletteuse=dither=bayer:bayer_scale=3",
            tempFile.path().string()
        };
        runner->run(ffmpeg, args, nullptr, cancel);
    }

    if (cancel && cancel->load()) {
        result.success = false;
        result.errorMessage = "Cancelled.";
        return result;
    }

    if (!tempFile.commit(outPath)) {
        result.success = false;
        result.errorMessage = "Media compression failed or output was invalid: " + tempFile.errorMessage();
        return result;
    }

    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = static_cast<int64_t>(std::filesystem::file_size(outPath));
    result.outputFormat = FormatDetector::formatName(req.detectedFormat);

    if (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes) {
        result.hitFloor = true;
        result.floorNote = "Reached minimum bitrate floor for audio/video playback.";
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
