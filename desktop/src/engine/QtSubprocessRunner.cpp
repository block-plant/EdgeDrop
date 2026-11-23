#include "QtSubprocessRunner.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <chrono>
#include <filesystem>
#include <algorithm>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

namespace EdgeDrop {

static std::vector<fs::path> getTrustedDirectories() {
    std::vector<fs::path> dirs;

#ifdef _WIN32
    dirs.push_back(fs::path("C:/Program Files"));
    dirs.push_back(fs::path("C:/Program Files (x86)"));
    dirs.push_back(fs::path("C:/Windows/System32"));
    dirs.push_back(fs::path("C:/Windows"));
#else
    dirs.push_back(fs::path("/opt/homebrew/bin"));
    dirs.push_back(fs::path("/usr/local/bin"));
    dirs.push_back(fs::path("/usr/bin"));
    dirs.push_back(fs::path("/bin"));
#endif

    QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        dirs.push_back(fs::path(appDir.toStdString()));
#ifdef __APPLE__
        dirs.push_back(fs::path(appDir.toStdString()) / "../Resources/bin");
        dirs.push_back(fs::path(appDir.toStdString()) / "bin");
#endif
    }

    return dirs;
}

bool QtSubprocessRunner::isTrustedExecutable(const std::string& rawPath, std::string& resolvedPath) {
    if (rawPath.empty()) return false;

    fs::path p(rawPath);
    std::vector<fs::path> trustedDirs = getTrustedDirectories();

    fs::path candidate;
    if (p.has_parent_path()) {
        // Explicit path provided
        std::error_code ec;
        candidate = fs::weakly_canonical(p, ec);
        if (ec) return false;
    } else {
        // Program name only — search exclusively in trusted directories
        for (const auto& dir : trustedDirs) {
            fs::path testPath = dir / p;
#ifdef _WIN32
            if (!testPath.has_extension()) {
                testPath.replace_extension(".exe");
            }
#endif
            std::error_code ec;
            if (fs::exists(testPath, ec) && !ec) {
                candidate = fs::canonical(testPath, ec);
                if (!ec) break;
            }
        }
        if (candidate.empty()) return false;
    }

    // Verify candidate is within an allowed trusted directory
    std::error_code ec;
    candidate = fs::canonical(candidate, ec);
    if (ec) return false;

    bool inTrustedDir = false;
    for (const auto& dir : trustedDirs) {
        fs::path canonicalDir = fs::weakly_canonical(dir, ec);
        if (ec) continue;

        auto mismatch = std::mismatch(canonicalDir.begin(), canonicalDir.end(), candidate.begin());
        if (mismatch.first == canonicalDir.end()) {
            inTrustedDir = true;
            break;
        }
    }

    if (!inTrustedDir) return false;

    // Check file attributes
    if (!fs::is_regular_file(candidate, ec) || ec) return false;

#ifndef _WIN32
    // Check executable permission and ensure NOT world-writable
    std::string candStr = candidate.string();
    if (access(candStr.c_str(), X_OK) != 0) return false;

    auto perms = fs::status(candidate, ec).permissions();
    if (ec) return false;
    if ((perms & fs::perms::others_write) != fs::perms::none) {
        return false; // Reject world-writable executable
    }
#endif

    resolvedPath = candidate.string();
    return true;
}

SubprocessResult QtSubprocessRunner::run(const std::string& program,
                                        const std::vector<std::string>& args,
                                        ProgressCallback progress,
                                        CancelToken cancel) {
    SubprocessResult result;
    std::string resolvedProgram;

    if (!isTrustedExecutable(program, resolvedProgram)) {
        result.exitCode = -1;
        result.stdErr = "Execution blocked by EdgeDrop security policy: binary not found in trusted system path.";
        return result;
    }

    // Scrub process environment of dynamic linker and tool-override variables
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    static const QStringList dangerousVars = {
        QStringLiteral("LD_PRELOAD"),
        QStringLiteral("LD_LIBRARY_PATH"),
        QStringLiteral("LD_AUDIT"),
        QStringLiteral("LD_ORIGIN_PATH"),
        QStringLiteral("DYLD_INSERT_LIBRARIES"),
        QStringLiteral("DYLD_LIBRARY_PATH"),
        QStringLiteral("DYLD_FRAMEWORK_PATH"),
        QStringLiteral("GS_OPTIONS"),
        QStringLiteral("GS_LIB"),
        QStringLiteral("GS_DEVICE"),
        QStringLiteral("GS_FONTPATH"),
        QStringLiteral("FFMPEG_DATADIR"),
        QStringLiteral("PYTHONPATH"),
        QStringLiteral("RUBYLIB"),
        QStringLiteral("PERL5LIB")
    };
    for (const auto& var : dangerousVars) {
        env.remove(var);
    }

    // Determine watchdog timeout
    int timeoutMs = 120000; // 120 seconds default (PDF / general)
    fs::path progPath(resolvedProgram);
    std::string baseName = progPath.stem().string();
    std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::tolower);
    if (baseName == "ffmpeg" || baseName == "ffprobe") {
        timeoutMs = 300000; // 300 seconds (5 min) for media
    }

    QStringList qArgs;
    qArgs.reserve(static_cast<qsizetype>(args.size()));
    for (const auto& a : args) {
        qArgs.append(QString::fromStdString(a));
    }

    QProcess process;
    process.setProcessEnvironment(env);
    process.start(QString::fromStdString(resolvedProgram), qArgs);

    if (!process.waitForStarted(5000)) {
        result.exitCode = -1;
        result.stdErr = "Failed to launch process: " + process.errorString().toStdString();
        return result;
    }

    auto startTime = std::chrono::steady_clock::now();
    while (!process.waitForFinished(100)) {
        if (cancel && cancel->load()) {
            process.terminate();
            if (!process.waitForFinished(1000)) {
                process.kill();
            }
            result.cancelled = true;
            result.exitCode = -1;
            result.stdOut = process.readAllStandardOutput().toStdString();
            result.stdErr = process.readAllStandardError().toStdString();
            return result;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > timeoutMs) {
            process.kill();
            process.waitForFinished(1000);
            result.timedOut = true;
            result.exitCode = -1;
            result.stdOut = process.readAllStandardOutput().toStdString();
            result.stdErr = "Execution timed out by watchdog policy.\n" + process.readAllStandardError().toStdString();
            return result;
        }
    }

    result.exitCode = process.exitCode();
    result.stdOut = process.readAllStandardOutput().toStdString();
    result.stdErr = process.readAllStandardError().toStdString();
    return result;
}

} // namespace EdgeDrop
