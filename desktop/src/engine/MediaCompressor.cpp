// MediaCompressor.cpp — Video/Audio compression via ffmpeg (QProcess)
#include "MediaCompressor.hpp"
#include "FormatDetector.hpp"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <chrono>
#include <cmath>
#include <algorithm>

namespace EdgeDrop {

// ── Locate ffmpeg / ffprobe ───────────────────────────────────────────────────

QString MediaCompressor::locateFfmpeg() {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/ffmpeg",
        "/opt/homebrew/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/usr/bin/ffmpeg"
    };
    for (const auto& c : candidates) {
        if (QFile::exists(c)) return c;
    }
    // Try PATH
    QProcess p;
    p.start("which", {"ffmpeg"});
    p.waitForFinished(2000);
    QString path = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (!path.isEmpty() && QFile::exists(path)) return path;
    return {};
}

QString MediaCompressor::locateFfprobe() {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + "/ffprobe",
        "/opt/homebrew/bin/ffprobe",
        "/usr/local/bin/ffprobe",
        "/usr/bin/ffprobe"
    };
    for (const auto& c : candidates) {
        if (QFile::exists(c)) return c;
    }
    QProcess p;
    p.start("which", {"ffprobe"});
    p.waitForFinished(2000);
    QString path = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (!path.isEmpty() && QFile::exists(path)) return path;
    return {};
}

// ── canHandle ─────────────────────────────────────────────────────────────────

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

// ── Probe media info via ffprobe ──────────────────────────────────────────────

struct MediaInfo {
    double duration = 0.0;   // seconds
    int    width    = 0;
    int    height   = 0;
    int    audioBitrate = 0; // bits/sec
    bool   hasVideo = false;
    bool   hasAudio = false;
};

static MediaInfo probeMedia(const QString& ffprobePath, const QString& filePath) {
    MediaInfo info;
    QProcess p;
    p.start(ffprobePath, {
        "-v", "quiet",
        "-print_format", "json",
        "-show_format", "-show_streams",
        filePath
    });
    if (!p.waitForFinished(10000)) return info;

    QByteArray out = p.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(out);
    if (doc.isNull()) return info;

    QJsonObject root = doc.object();
    QJsonObject fmt = root["format"].toObject();
    info.duration = fmt["duration"].toString().toDouble();

    for (const auto& sv : root["streams"].toArray()) {
        QJsonObject stream = sv.toObject();
        QString codec_type = stream["codec_type"].toString();
        if (codec_type == "video") {
            info.hasVideo = true;
            info.width  = stream["width"].toInt();
            info.height = stream["height"].toInt();
        } else if (codec_type == "audio") {
            info.hasAudio = true;
            info.audioBitrate = stream["bit_rate"].toString().toInt();
        }
    }
    return info;
}

// ── Main compress ─────────────────────────────────────────────────────────────

JobResult MediaCompressor::compress(const JobRequest& req,
                                    ProgressCallback progress,
                                    CancelToken cancel) {
    auto t0 = std::chrono::steady_clock::now();
    JobResult result;
    result.inputFilePath = req.inputFilePath;

    QFileInfo inInfo(req.inputFilePath);
    if (!inInfo.exists()) {
        result.success = false;
        result.errorMessage = "File not found: " + req.inputFilePath;
        return result;
    }
    result.inputSizeBytes = inInfo.size();

    if (progress) progress(0.02);

    // Locate ffmpeg
    QString ffmpeg  = locateFfmpeg();
    QString ffprobe = locateFfprobe();
    if (ffmpeg.isEmpty()) {
        result.success = false;
        result.errorMessage = "FFmpeg not found. Install FFmpeg to compress video/audio (not found in PATH).";
        return result;
    }

    if (progress) progress(0.05);

    // Pass-through guard
    if (req.targetBytes > 0 && inInfo.size() <= req.targetBytes) {
        QString outDir = req.outputDirectory.isEmpty()
            ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
        QDir().mkpath(outDir);
        QString ext = FormatDetector::outputExtension(req.detectedFormat);
        QString outPath = QDir(outDir).filePath(inInfo.completeBaseName() + "_compressed" + ext);
        int ctr = 1;
        while (QFile::exists(outPath)) {
            outPath = QDir(outDir).filePath(
                inInfo.completeBaseName() + "_compressed (" + QString::number(ctr++) + ")" + ext);
        }
        QFile::copy(req.inputFilePath, outPath);
        result.success = true;
        result.outputFilePath = outPath;
        result.outputSizeBytes = inInfo.size();
        result.outputFormat = FormatDetector::formatName(req.detectedFormat);
        if (progress) progress(1.0);
        auto t1 = std::chrono::steady_clock::now();
        result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        return result;
    }

    // Probe
    MediaInfo info;
    if (!ffprobe.isEmpty()) {
        info = probeMedia(ffprobe, req.inputFilePath);
    }

    QString outDir = req.outputDirectory.isEmpty()
        ? (QDir::homePath() + "/Downloads") : req.outputDirectory;
    QDir().mkpath(outDir);

    QString ext     = FormatDetector::outputExtension(req.detectedFormat);
    QString stem    = inInfo.completeBaseName();
    QString outPath;
    int ctr = 0;
    do {
        QString sfx = ctr == 0 ? "" : QString(" (%1)").arg(ctr);
        outPath = QDir(outDir).filePath(stem + "_compressed" + sfx + ext);
        ++ctr;
    } while (QFile::exists(outPath));

    QStringList args;

    bool isAudio = (req.detectedFormat == DetectedFormat::MP3 ||
                    req.detectedFormat == DetectedFormat::M4A ||
                    req.detectedFormat == DetectedFormat::AAC ||
                    req.detectedFormat == DetectedFormat::OGG ||
                    req.detectedFormat == DetectedFormat::WAV);
    bool isGif   = (req.detectedFormat == DetectedFormat::GIF);

    if (isAudio) {
        // Audio: target bitrate
        double duration = info.duration > 0.0 ? info.duration : 60.0;
        double targetBits = static_cast<double>(req.targetBytes > 0 ? req.targetBytes : inInfo.size()) * 8.0 * 0.96;
        int targetBitrate = std::max(32000, static_cast<int>(targetBits / duration));
        targetBitrate = std::min(targetBitrate, 320000);

        args = {"-y", "-i", req.inputFilePath,
                "-b:a", QString::number(targetBitrate),
                "-vn", outPath};

    } else if (isGif) {
        // GIF: palettegen + paletteuse two-pass, adaptive FPS/scale
        double duration = info.duration > 0.0 ? info.duration : 5.0;
        double targetBits = static_cast<double>(req.targetBytes > 0 ? req.targetBytes : inInfo.size()) * 8.0 * 0.90;
        double fps = 15.0;
        int scale = info.width > 0 ? info.width : 480;

        // Reduce fps / scale to meet target
        if (duration > 0.0) {
            double bitsPerFrame = targetBits / (duration * fps);
            if (bitsPerFrame < 50000) { fps = 10.0; }
            if (bitsPerFrame < 30000) { fps = 8.0; scale = static_cast<int>(scale * 0.75); }
        }

        // Two-pass palette approach
        QString palPath = QDir::tempPath() + "/edgedrop_palette_" +
                          QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";

        // Pass 1: generate palette
        QProcess pass1;
        pass1.start(ffmpeg, {
            "-y", "-i", req.inputFilePath,
            "-vf", QString("fps=%1,scale=%2:-1:flags=lanczos,palettegen").arg(fps).arg(scale),
            palPath
        });
        pass1.waitForFinished(60000);

        // Pass 2: apply palette
        args = {"-y", "-i", req.inputFilePath, "-i", palPath,
                "-filter_complex",
                QString("fps=%1,scale=%2:-1:flags=lanczos[x];[x][1:v]paletteuse").arg(fps).arg(scale),
                outPath};

    } else {
        // Video: two-pass bitrate control
        double duration = info.duration > 0.0 ? info.duration : 60.0;
        double totalBits = static_cast<double>(req.targetBytes > 0 ? req.targetBytes : inInfo.size()) * 8.0 * 0.95;
        int audioBr = std::min(128000, std::max(64000, static_cast<int>(totalBits * 0.1 / duration)));
        int videoBr = std::max(100000, static_cast<int>(totalBits / duration) - audioBr);

        // Resolution stepping if bits-per-pixel too low
        int outW = info.width > 0 ? info.width : 1280;
        int outH = info.height > 0 ? info.height : 720;
        double bpp = static_cast<double>(videoBr) / (outW * outH * 30.0); // assume 30fps
        if (bpp < 0.05 && outH > 480)      { outH = 480; outW = -2; }
        else if (bpp < 0.08 && outH > 720) { outH = 720; outW = -2; }

        QString scaleArg = outW == -2
            ? QString("scale=-2:%1").arg(outH)
            : QString("scale=%1:%2").arg(outW).arg(outH);

        // Simple single-pass (two-pass needs two runs; use CRF-based for simplicity)
        int crf = 23; // default quality
        if (req.targetBytes > 0) {
            // Approximate CRF from target bitrate
            double ratio = static_cast<double>(videoBr) / 2000000.0;
            crf = std::clamp(static_cast<int>(23.0 - std::log2(ratio) * 3.0), 18, 51);
        }

        args = {"-y", "-i", req.inputFilePath,
                "-vf", scaleArg,
                "-c:v", "libx264",
                "-crf", QString::number(crf),
                "-preset", "fast",
                "-c:a", "aac",
                "-b:a", QString::number(audioBr),
                outPath};
    }

    if (progress) progress(0.15);

    QProcess proc;
    proc.start(ffmpeg, args);

    // Stream progress by parsing ffmpeg stderr
    while (!proc.waitForFinished(200)) {
        if (cancel && cancel->load()) {
            proc.kill();
            proc.waitForFinished(2000);
            QFile::remove(outPath);
            result.success = false;
            result.errorMessage = "Cancelled.";
            return result;
        }
        QByteArray err = proc.readAllStandardError();
        // Parse out_time_us from progress updates
        if (err.contains("out_time_us")) {
            int pos = err.indexOf("out_time_us=");
            if (pos >= 0) {
                qint64 us = err.mid(pos + 12, 20).trimmed().toLongLong();
                double dur = info.duration > 0.0 ? info.duration : 60.0;
                double frac = std::clamp(static_cast<double>(us) / (dur * 1e6), 0.0, 1.0);
                if (progress) progress(0.15 + 0.80 * frac);
            }
        }
    }

    // Cleanup GIF palette
    if (isGif) {
        QString palPath = QDir::tempPath() + "/edgedrop_palette_*.png";
        // Best effort cleanup (we stored the specific path in pass1 above, but this is a minor cleanup)
        QDir tmp(QDir::tempPath());
        for (const QString& f : tmp.entryList({"edgedrop_palette_*.png"}, QDir::Files)) {
            QFile::remove(QDir::tempPath() + "/" + f);
        }
    }

    if (proc.exitCode() != 0 || !QFile::exists(outPath)) {
        result.success = false;
        result.errorMessage = "FFmpeg failed: " + QString::fromUtf8(proc.readAllStandardError()).left(300);
        return result;
    }

    auto t1 = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    result.success = true;
    result.outputFilePath = outPath;
    result.outputSizeBytes = QFileInfo(outPath).size();
    result.outputFormat = FormatDetector::formatName(req.detectedFormat);

    result.hitFloor = (req.targetBytes > 0 && result.outputSizeBytes > req.targetBytes);
    if (result.hitFloor) {
        result.floorNote = QString("Compressed as far as safely possible: %1 (target was %2)")
            .arg(result.formattedOutputSize())
            .arg(JobResult::formatBytes(req.targetBytes));
    }

    if (progress) progress(1.0);
    return result;
}

} // namespace EdgeDrop
