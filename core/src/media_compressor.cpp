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
        "/snap/bin/ffmpeg",
#ifdef _WIN32
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "ffmpeg.exe",
#endif
        "ffmpeg"
    };

#ifdef _WIN32
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData) {
        candidates.insert(candidates.begin(), std::string(localAppData) + "\\Microsoft\\WinGet\\Links\\ffmpeg.exe");
    }
#endif

    auto runner = getMediaRunner();
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec)) return c;
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
        "/snap/bin/ffprobe",
#ifdef _WIN32
        "C:\\ffmpeg\\bin\\ffprobe.exe",
        "ffprobe.exe",
#endif
        "ffprobe"
    };

#ifdef _WIN32
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData) {
        candidates.insert(candidates.begin(), std::string(localAppData) + "\\Microsoft\\WinGet\\Links\\ffprobe.exe");
    }
#endif

    auto runner = getMediaRunner();
    for (const auto& c : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(c, ec)) return c;
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

static bool isOpusSupported(std::shared_ptr<ISubprocessRunner> runner, const std::string& ffmpeg) {
    static int s_opusSupported = -1;
    if (s_opusSupported != -1) return s_opusSupported == 1;
    auto res = runner->run(ffmpeg, {"-hide_banner", "-codecs"});
    if (res.exitCode == 0 && res.stdOut.find("libopus") != std::string::npos) {
        s_opusSupported = 1;
    } else {
        s_opusSupported = 0;
    }
    return s_opusSupported == 1;
}

static int snapMp3Bitrate(int targetKbps) {
    static const int kMp3Bitrates[] = {320, 256, 192, 160, 128, 96, 64, 48, 32};
    for (int b : kMp3Bitrates) {
        if (targetKbps >= b) return b;
    }
    return 32;
}

struct MediaInfo {
    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 25.0;
    int audioChannels = 2;
    int sampleRate = 44100;
    int64_t audioBitrate = 0;
    bool hasVideo = false;
    bool hasAudio = false;
};

static MediaInfo probeFile(const std::string& ffprobeBin, const std::filesystem::path& path) {
    MediaInfo info;
    auto runner = getMediaRunner();
    std::string safePath = "file:" + std::filesystem::absolute(path).string();
    auto res = runner->run(ffprobeBin, {
        "-v", "error",
        "-protocol_whitelist", "file,pipe",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        "--",
        safePath
    });

    if (res.exitCode != 0 || res.stdOut.empty()) {
        std::cerr << "probeFile failed for " << path.string() << " (exit code " << res.exitCode << "): " << res.stdErr << std::endl;
        return info;
    }

    try {
        auto j = nlohmann::json::parse(res.stdOut);
        if (j.contains("format")) {
            const auto& fmt = j["format"];
            if (fmt.contains("duration")) {
                try {
                    if (fmt["duration"].is_string()) {
                        std::string dStr = fmt["duration"].get<std::string>();
                        if (!dStr.empty() && dStr != "N/A") {
                            info.duration = std::stod(dStr);
                        }
                    } else if (fmt["duration"].is_number()) {
                        info.duration = fmt["duration"].get<double>();
                    }
                } catch (...) {}
            }
            if (fmt.contains("bit_rate")) {
                try {
                    if (fmt["bit_rate"].is_string()) {
                        std::string bStr = fmt["bit_rate"].get<std::string>();
                        if (!bStr.empty() && bStr != "N/A") {
                            info.audioBitrate = std::stoll(bStr);
                        }
                    } else if (fmt["bit_rate"].is_number()) {
                        info.audioBitrate = fmt["bit_rate"].get<int64_t>();
                    }
                } catch (...) {}
            }
        }

        if (j.contains("streams") && j["streams"].is_array()) {
            for (const auto& s : j["streams"]) {
                std::string codecType = s.value("codec_type", "");

                // Ignore embedded album art / cover pictures (attached_pic)
                bool isAttachedPic = false;
                if (s.contains("disposition") && s["disposition"].is_object()) {
                    const auto& disp = s["disposition"];
                    if (disp.contains("attached_pic")) {
                        if (disp["attached_pic"].is_number() && disp["attached_pic"].get<int>() == 1) {
                            isAttachedPic = true;
                        } else if (disp["attached_pic"].is_string() && disp["attached_pic"].get<std::string>() == "1") {
                            isAttachedPic = true;
                        } else if (disp["attached_pic"].is_boolean() && disp["attached_pic"].get<bool>()) {
                            isAttachedPic = true;
                        }
                    }
                }

                if (codecType == "video" && !isAttachedPic && !info.hasVideo) {
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
                    if (s.contains("bit_rate")) {
                        try {
                            if (s["bit_rate"].is_string()) {
                                std::string bStr = s["bit_rate"].get<std::string>();
                                if (!bStr.empty() && bStr != "N/A") {
                                    info.audioBitrate = std::stoll(bStr);
                                }
                            } else if (s["bit_rate"].is_number()) {
                                info.audioBitrate = s["bit_rate"].get<int64_t>();
                            }
                        } catch (...) {}
                    }
                }

                // Fallback stream duration check if format duration was absent or 0
                if (info.duration <= 0.0 && s.contains("duration")) {
                    try {
                        if (s["duration"].is_string()) {
                            std::string dStr = s["duration"].get<std::string>();
                            if (!dStr.empty() && dStr != "N/A") {
                                info.duration = std::stod(dStr);
                            }
                        } else if (s["duration"].is_number()) {
                            info.duration = s["duration"].get<double>();
                        }
                    } catch (...) {}
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

    bool isWav = (req.detectedFormat == DetectedFormat::WAV);
    std::string ext = isWav ? ".m4a" : FormatDetector::outputExtension(req.detectedFormat);
    if (ext.empty()) ext = req.inputFilePath.extension().string();

    // ── Pass-Through Guard ────────────────────────────────────────────────────
    if (req.targetBytes > 0 && result.inputSizeBytes <= req.targetBytes) {
        std::string origExt = FormatDetector::outputExtension(req.detectedFormat);
        if (origExt.empty()) origExt = req.inputFilePath.extension().string();
        std::filesystem::path passOutPath = SecurityGuards::generateCompressedOutputPath(req.inputFilePath, outDir, origExt);
        std::error_code ec;
        std::filesystem::copy_file(req.inputFilePath, passOutPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (!ec) {
            result.success = true;
            result.outputFilePath = passOutPath;
            result.outputSizeBytes = result.inputSizeBytes;
            result.outputFormat = FormatDetector::formatName(req.detectedFormat);
            result.hitFloor = true;
            result.floorNote = "Already smaller than your target size.";
            auto t1 = std::chrono::steady_clock::now();
            result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            if (progress) progress(1.0);
            return result;
        }
    }

    std::filesystem::path outPath = SecurityGuards::generateCompressedOutputPath(req.inputFilePath, outDir, ext);

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

    MediaInfo info = probeFile(ffprobe, req.inputFilePath);
    if ((isVideo || isAudio) && info.duration <= 0.0) {
        result.success = false;
        result.errorMessage = "Could not determine " + std::string(isAudio ? "audio" : "video") + " duration: media probe failed or file is unreadable.";
        return result;
    }
    if (info.duration <= 0.0) {
        info.duration = 1.0;
    }

    int minAudioFloorKbps = 32;

    auto runner = getMediaRunner();
    std::string safeIn = "file:" + std::filesystem::absolute(req.inputFilePath).string();
    SecureTempFile tempFile(outDir, ext);
    if (!tempFile.isValid()) {
        result.success = false;
        result.errorMessage = tempFile.errorMessage();
        return result;
    }

    struct DirCleanupGuard {
        std::filesystem::path dir;
        ~DirCleanupGuard() {
            std::error_code ec;
            if (!dir.empty() && std::filesystem::exists(dir, ec)) {
                std::filesystem::remove_all(dir, ec);
            }
        }
    };

    if (isVideo) {
        // Compute bitrates with 6% container overhead margin
        double targetBytes = (req.targetBytes > 0) ? static_cast<double>(req.targetBytes) : static_cast<double>(result.inputSizeBytes * 0.7);
        double rTotal = (targetBytes * 8.0 * 0.94) / info.duration;
        double rA = std::clamp(0.12 * rTotal, 48000.0, 128000.0);
        double rV = std::max(rTotal - rA, 64000.0);

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

        std::string scaleFilter = "scale=" + std::to_string(outW) + ":" + std::to_string(outH);
        if (info.fps > 30.0) {
            scaleFilter += ",fps=30";
        }
        std::filesystem::path passlogDir = std::filesystem::temp_directory_path() / (".edgedrop_passlog_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(passlogDir);
        DirCleanupGuard passlogGuard{passlogDir};
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
        auto res1 = runner->run(ffmpeg, pass1Args, nullptr, cancel);
        if (res1.cancelled) {
            result.success = false; result.errorMessage = "Cancelled."; return result;
        }
        if (res1.exitCode != 0) {
            result.success = false;
            result.errorMessage = !res1.stdErr.empty() ? res1.stdErr : "FFmpeg pass 1 failed.";
            return result;
        }

        if (progress) progress(0.5);
        if (cancel && cancel->load()) {
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
        auto res2 = runner->run(ffmpeg, pass2Args, nullptr, cancel);
        if (res2.cancelled) {
            result.success = false; result.errorMessage = "Cancelled."; return result;
        }
        if (res2.exitCode != 0) {
            result.success = false;
            result.errorMessage = !res2.stdErr.empty() ? res2.stdErr : "FFmpeg pass 2 failed.";
            return result;
        }

        // Check if actualBytes > req.targetBytes, run one corrective pass
        if (req.targetBytes > 0 && !(cancel && cancel->load())) {
            std::error_code ec;
            auto actualBytes = std::filesystem::file_size(tempFile.path(), ec);
            if (!ec && actualBytes > static_cast<uintmax_t>(req.targetBytes)) {
                double scaleFactor = (static_cast<double>(req.targetBytes) / static_cast<double>(actualBytes)) * 0.95;
                double newRV = std::max(rV * scaleFactor, 50000.0);
                double newRA = std::max(rA * scaleFactor, 40000.0);
                if (newRV < rV) {
                    SecureTempFile corrTemp(outDir, ext);
                    if (corrTemp.isValid()) {
                        std::vector<std::string> corrArgs = {
                            "-y",
                            "-nostdin",
                            "-protocol_whitelist", "file,pipe",
                            "-i", safeIn,
                            "-c:v", "libx264",
                            "-b:v", std::to_string(static_cast<int>(newRV)),
                            "-maxrate", std::to_string(static_cast<int>(newRV * 1.2)),
                            "-bufsize", std::to_string(static_cast<int>(newRV * 2)),
                            "-vf", scaleFilter,
                            "-c:a", "aac",
                            "-b:a", std::to_string(static_cast<int>(newRA)),
                            corrTemp.path().string()
                        };
                        auto resCorr = runner->run(ffmpeg, corrArgs, nullptr, cancel);
                        if (resCorr.exitCode == 0) {
                            auto corrSize = std::filesystem::file_size(corrTemp.path(), ec);
                            if (!ec && corrSize > 0 && corrSize < actualBytes) {
                                std::filesystem::rename(corrTemp.path(), tempFile.path(), ec);
                            }
                        }
                    }
                }
            }
        }

    } else if (isAudio) {
        double targetBytes = (req.targetBytes > 0)
            ? static_cast<double>(req.targetBytes)
            : static_cast<double>(result.inputSizeBytes * 0.70);

        // Compute bitrate from effectiveTarget = targetBytes * 0.96 (reserving 4% for container headers/metadata)
        double effectiveTarget = targetBytes * 0.96;
        int targetKbps = static_cast<int>(std::floor((effectiveTarget * 8.0) / info.duration / 1000.0));

        // Never upscale: Cap targetKbps at source audio bitrate (when lossy) and at 320 kbps max
        if (info.audioBitrate > 0 && req.detectedFormat != DetectedFormat::WAV) {
            int srcKbps = static_cast<int>(info.audioBitrate / 1000);
            if (srcKbps > 0) {
                targetKbps = std::min(targetKbps, srcKbps);
            }
        }
        targetKbps = std::min(targetKbps, 320);

        std::string codec;
        int encodeKbps = targetKbps;

        if (req.detectedFormat == DetectedFormat::MP3) {
            codec = "libmp3lame";
            minAudioFloorKbps = 32;
            encodeKbps = snapMp3Bitrate(targetKbps);
        } else if (req.detectedFormat == DetectedFormat::OGG) {
            if (isOpusSupported(runner, ffmpeg)) {
                codec = "libopus";
                minAudioFloorKbps = 24;
                encodeKbps = std::max(targetKbps, 24);
            } else {
                codec = "libvorbis";
                minAudioFloorKbps = 48;
                encodeKbps = std::max(targetKbps, 48);
            }
        } else {
            // M4A, AAC, or WAV (transcoded to M4A)
            codec = "aac";
            minAudioFloorKbps = 32;
            encodeKbps = std::max(targetKbps, 32);
        }

        if (targetKbps < minAudioFloorKbps) {
            result.hitFloor = true;
        }

        auto buildAudioArgs = [&](int kbps, const std::string& outFilePath) {
            std::vector<std::string> args = {
                "-y",
                "-nostdin",
                "-protocol_whitelist", "file,pipe",
                "-i", safeIn,
                "-vn",
                "-c:a", codec,
                "-b:a", std::to_string(kbps) + "k"
            };

            if (req.detectedFormat == DetectedFormat::MP3) {
                if (kbps <= 64) {
                    args.push_back("-ac");
                    args.push_back("1");
                    args.push_back("-ar");
                    args.push_back("22050");
                }
            } else if (req.detectedFormat == DetectedFormat::OGG) {
                if (codec == "libopus" && kbps <= 32) {
                    args.push_back("-ac");
                    args.push_back("1");
                } else if (codec == "libvorbis" && kbps <= 48) {
                    args.push_back("-ac");
                    args.push_back("1");
                }
            } else {
                // AAC / M4A / WAV
                if (kbps <= 48) {
                    args.push_back("-ac");
                    args.push_back("1");
                }
                if (req.detectedFormat == DetectedFormat::M4A || req.detectedFormat == DetectedFormat::WAV) {
                    args.push_back("-movflags");
                    args.push_back("+faststart");
                }
            }

            args.push_back(outFilePath);
            return args;
        };

        std::vector<std::string> args = buildAudioArgs(encodeKbps, tempFile.path().string());
        auto res = runner->run(ffmpeg, args, nullptr, cancel);
        if (res.cancelled) {
            result.success = false;
            result.errorMessage = "Cancelled.";
            return result;
        }
        if (res.exitCode != 0) {
            result.success = false;
            result.errorMessage = !res.stdErr.empty() ? res.stdErr : "Audio conversion failed.";
            return result;
        }

        // Post-encode ceiling check: If actualBytes > targetBytes and above floor, run one corrective pass
        if (req.targetBytes > 0 && !(cancel && cancel->load())) {
            std::error_code ec;
            auto actualBytes = std::filesystem::file_size(tempFile.path(), ec);
            if (!ec && actualBytes > static_cast<uintmax_t>(req.targetBytes) && encodeKbps > minAudioFloorKbps) {
                double scaleFactor = (static_cast<double>(req.targetBytes) / static_cast<double>(actualBytes)) * 0.94;
                int corrKbps = std::max(static_cast<int>(encodeKbps * scaleFactor), minAudioFloorKbps);
                if (req.detectedFormat == DetectedFormat::MP3) {
                    corrKbps = snapMp3Bitrate(corrKbps);
                }
                if (corrKbps < encodeKbps) {
                    SecureTempFile corrTemp(outDir, ext);
                    if (corrTemp.isValid()) {
                        std::vector<std::string> corrArgs = buildAudioArgs(corrKbps, corrTemp.path().string());
                        auto resCorr = runner->run(ffmpeg, corrArgs, nullptr, cancel);
                        if (resCorr.exitCode == 0) {
                            auto corrSize = std::filesystem::file_size(corrTemp.path(), ec);
                            if (!ec && corrSize > 0 && corrSize < actualBytes) {
                                std::filesystem::rename(corrTemp.path(), tempFile.path(), ec);
                                encodeKbps = corrKbps;
                            }
                        }
                    }
                }
            }
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
        auto res = runner->run(ffmpeg, args, nullptr, cancel);
        if (res.exitCode != 0) {
            result.success = false;
            result.errorMessage = !res.stdErr.empty() ? res.stdErr : "GIF conversion failed.";
            return result;
        }
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

    if (isWav) {
        result.outputFormat = "M4A";
        if (result.hitFloor || (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes)) {
            result.hitFloor = true;
            result.floorNote = "Converted WAV -> M4A; reached minimum audio bitrate floor (" + std::to_string(minAudioFloorKbps) + " kbps).";
        } else {
            result.floorNote = "Converted WAV -> M4A to meet target size.";
        }
    } else if (isAudio) {
        result.outputFormat = FormatDetector::formatName(req.detectedFormat);
        if (result.hitFloor || (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes)) {
            result.hitFloor = true;
            result.floorNote = "Reached minimum audio bitrate floor (" + std::to_string(minAudioFloorKbps) + " kbps) for intelligible playback.";
        }
    } else {
        result.outputFormat = FormatDetector::formatName(req.detectedFormat);
        if (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes) {
            result.hitFloor = true;
            result.floorNote = "Reached minimum bitrate floor for audio/video playback.";
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
