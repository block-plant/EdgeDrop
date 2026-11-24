#include "QtSubprocessRunner.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>
#include <filesystem>

#if defined(Q_OS_WIN) || defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
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
  dirs.push_back(fs::path("C:/ffmpeg/bin"));
  const char *localAppData = getenv("LOCALAPPDATA");
  if (localAppData) {
    dirs.push_back(fs::path(localAppData) / "Microsoft/WinGet/Links");
    dirs.push_back(fs::path(localAppData) / "Microsoft/WinGet/Packages");
  }
#else
  dirs.push_back(fs::path("/opt/homebrew"));
  dirs.push_back(fs::path("/opt/homebrew/bin"));
  dirs.push_back(fs::path("/opt/homebrew/Cellar"));
  dirs.push_back(fs::path("/usr/local"));
  dirs.push_back(fs::path("/usr/local/bin"));
  dirs.push_back(fs::path("/usr/local/Cellar"));
  dirs.push_back(fs::path("/usr/bin"));
  dirs.push_back(fs::path("/usr"));
  dirs.push_back(fs::path("/bin"));
  dirs.push_back(fs::path("/snap/bin"));
#endif

  QString appDir = QCoreApplication::applicationDirPath();
  if (!appDir.isEmpty()) {
    dirs.push_back(fs::path(appDir.toStdString()));
#ifdef __APPLE__
    dirs.push_back(fs::path(appDir.toStdString()) / "../Resources/bin");
    dirs.push_back(fs::path(appDir.toStdString()) / "bin");
    dirs.push_back(fs::path(appDir.toStdString()) / "../Resources");
#endif
  }

  return dirs;
}

bool QtSubprocessRunner::isTrustedExecutable(const std::string &rawPath,
                                             std::string &resolvedPath) {
  if (rawPath.empty())
    return false;

  fs::path p(rawPath);
  std::vector<fs::path> trustedDirs = getTrustedDirectories();

  fs::path candidate;
  std::error_code ec;

  if (p.has_parent_path()) {
    if (fs::exists(p, ec) && !ec) {
      candidate = p;
    } else {
      return false;
    }
  } else {
    // Search in trustedDirs
    for (const auto &dir : trustedDirs) {
      fs::path testPath = dir / p;
#ifdef _WIN32
      if (!testPath.has_extension()) {
        testPath.replace_extension(".exe");
      }
#endif
      if (fs::exists(testPath, ec) && !ec) {
        candidate = testPath;
        break;
      }
    }
    if (candidate.empty()) {
      QString found =
          QStandardPaths::findExecutable(QString::fromStdString(rawPath));
      if (!found.isEmpty()) {
        candidate = fs::path(found.toStdString());
      } else {
        return false;
      }
    }
  }

  // Resolve symlinks to canonical path (e.g. Homebrew symlink -> Cellar path)
  fs::path canonicalCandidate = fs::canonical(candidate, ec);
  if (ec) {
    canonicalCandidate = fs::weakly_canonical(candidate, ec);
  }

  auto isSubdirOf = [](const fs::path &base, const fs::path &sub) {
    std::error_code e;
    fs::path cBase = fs::weakly_canonical(base, e);
    if (e)
      return false;
    fs::path cSub = fs::weakly_canonical(sub, e);
    if (e)
      return false;
    auto mismatch = std::mismatch(cBase.begin(), cBase.end(), cSub.begin());
    return (mismatch.first == cBase.end());
  };

  bool inTrustedDir = false;
  for (const auto &dir : trustedDirs) {
    if (isSubdirOf(dir, candidate) ||
        (!canonicalCandidate.empty() && isSubdirOf(dir, canonicalCandidate))) {
      inTrustedDir = true;
      break;
    }
  }

  if (!inTrustedDir)
    return false;

  fs::path targetToCheck =
      !canonicalCandidate.empty() ? canonicalCandidate : candidate;
  if (!fs::is_regular_file(targetToCheck, ec) || ec)
    return false;

#ifndef _WIN32
  std::string candStr = targetToCheck.string();
  if (access(candStr.c_str(), X_OK) != 0)
    return false;

  auto perms = fs::status(targetToCheck, ec).permissions();
  if (ec)
    return false;
  if ((perms & fs::perms::others_write) != fs::perms::none) {
    return false; // Reject world-writable executable
  }
#endif

  resolvedPath = targetToCheck.string();
  return true;
}

SubprocessResult QtSubprocessRunner::run(const std::string &program,
                                         const std::vector<std::string> &args,
                                         ProgressCallback progress,
                                         CancelToken cancel) {
  SubprocessResult result;
  std::string resolvedProgram;

  if (!isTrustedExecutable(program, resolvedProgram)) {
    result.exitCode = -1;
    result.stdErr = "Execution blocked by EdgeDrop security policy: binary not "
                    "found in trusted system path.";
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
      QStringLiteral("PERL5LIB")};
  for (const auto &var : dangerousVars) {
    env.remove(var);
  }

#ifndef _WIN32
  QString currentPath = env.value(QStringLiteral("PATH"));
  QString standardPaths = QStringLiteral(
      "/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/snap/bin");
  if (!currentPath.isEmpty()) {
    env.insert(QStringLiteral("PATH"),
               standardPaths + QStringLiteral(":") + currentPath);
  } else {
    env.insert(QStringLiteral("PATH"), standardPaths);
  }
#endif

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
  for (const auto &a : args) {
    qArgs.append(QString::fromStdString(a));
  }

  QProcess process;
  process.setProcessEnvironment(env);
#if defined(Q_OS_WIN) || defined(_WIN32)
  process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
    args->flags |= CREATE_NO_WINDOW;
  });
#endif
  process.start(QString::fromStdString(resolvedProgram), qArgs);

  if (!process.waitForStarted(5000)) {
    result.exitCode = -1;
    result.stdErr =
        "Failed to launch process: " + process.errorString().toStdString();
    return result;
  }

  QByteArray accumulatedOut;
  QByteArray accumulatedErr;

  auto startTime = std::chrono::steady_clock::now();
  while (!process.waitForFinished(100)) {
    QByteArray chunkOut = process.readAllStandardOutput();
    if (!chunkOut.isEmpty()) accumulatedOut.append(chunkOut);
    QByteArray chunkErr = process.readAllStandardError();
    if (!chunkErr.isEmpty()) accumulatedErr.append(chunkErr);

    if (cancel && cancel->load()) {
      process.terminate();
      if (!process.waitForFinished(1000)) {
        process.kill();
      }
      result.cancelled = true;
      result.exitCode = -1;
      accumulatedOut.append(process.readAllStandardOutput());
      accumulatedErr.append(process.readAllStandardError());
      result.stdOut = accumulatedOut.toStdString();
      result.stdErr = accumulatedErr.toStdString();
      return result;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - startTime)
                       .count();
    if (elapsed > timeoutMs) {
      process.kill();
      process.waitForFinished(1000);
      result.timedOut = true;
      result.exitCode = -1;
      accumulatedOut.append(process.readAllStandardOutput());
      accumulatedErr.append(process.readAllStandardError());
      result.stdOut = accumulatedOut.toStdString();
      result.stdErr = "Execution timed out by watchdog policy.\n" +
                      accumulatedErr.toStdString();
      return result;
    }
  }

  accumulatedOut.append(process.readAllStandardOutput());
  accumulatedErr.append(process.readAllStandardError());
  result.exitCode = process.exitCode();
  result.stdOut = accumulatedOut.toStdString();
  result.stdErr = accumulatedErr.toStdString();
  return result;
}

} // namespace EdgeDrop
