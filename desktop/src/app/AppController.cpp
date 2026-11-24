#include "AppController.hpp"
#include "../platform/PlatformHelper.hpp"
#include <edgedrop/format_detector.hpp>
#include <edgedrop/media_compressor.hpp>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QFileDialog>
#include <QDateTime>

namespace EdgeDrop {

AppController::AppController(QObject* parent)
    : QObject(parent),
      m_wallpaperThemeEngine(&m_settingsManager, this) {
    m_edgeWindowManager.setSettingsManager(&m_settingsManager);
    connect(&m_edgeWindowManager, &EdgeWindowManager::screenEdgeChanged,
            &m_wallpaperThemeEngine, &WallpaperThemeEngine::updateForEdge);

    connect(&m_jobRunner, &JobRunner::jobStarted,     this, &AppController::isProcessingChanged);
    connect(&m_jobRunner, &JobRunner::jobFinished,   this, &AppController::onJobFinished);
    connect(&m_jobRunner, &JobRunner::jobProgress,   this, &AppController::onJobProgress);
    connect(&m_jobRunner, &JobRunner::batchProgress, this, &AppController::onBatchProgress);
    connect(&m_jobRunner, &JobRunner::batchFinished, this, &AppController::onBatchFinished);

    for (const QString& p : m_settingsManager.recentFiles()) {
        if (QFile::exists(p)) {
            QVariantMap m;
            m["path"] = p;
            m["fileName"] = QFileInfo(p).fileName();
            m_recentFiles.append(m);
            if (m_recentFiles.size() >= 3) break;
        }
    }
    checkFfmpegAvailability();
}

bool AppController::isCurrentFileMedia() const {
    MediaCompressor compressor;
    return compressor.canHandle(m_detectedFormat);
}

QString AppController::ffmpegInstallCommand() const {
#if defined(Q_OS_MACOS) || defined(Q_OS_APPLE)
    return "brew install ffmpeg";
#elif defined(Q_OS_WIN)
    return "winget install Gyan.FFmpeg";
#else
    return "sudo apt install ffmpeg";
#endif
}

void AppController::checkFfmpegAvailability() {
    bool available = !MediaCompressor::locateFfmpeg().empty();
    if (m_hasFfmpeg != available) {
        m_hasFfmpeg = available;
        emit ffmpegAvailabilityChanged();
    }
}

void AppController::copyToClipboard(const QString& text) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard) {
        clipboard->setText(text);
    }
}

// ── Target bytes ──────────────────────────────────────────────────────────────

void AppController::setTargetBytes(qint64 bytes) {
    if (m_targetBytes != bytes) {
        m_targetBytes = bytes;
        emit targetBytesChanged();
    }
}

// ── detectedFormatLabel ───────────────────────────────────────────────────────

QString AppController::detectedFormatLabel() const {
    return m_currentFileInfo.value("detectedFormatLabel").toString();
}

// ── analyzeInputFile ──────────────────────────────────────────────────────────

void AppController::analyzeInputFile(const QString& filePath) {
    QFileInfo info(filePath);
    if (!info.exists()) return;

    m_detectedFormat = FormatDetector::detect(filePath.toStdString());

    QVariantMap fileMap;
    fileMap["filePath"]  = filePath;
    fileMap["fileName"]  = info.fileName();
    fileMap["sizeBytes"] = info.size();
    fileMap["formattedSize"] = QString::fromStdString(JobResult::formatBytes(info.size()));
    fileMap["suffix"]    = info.suffix().toUpper();
    fileMap["detectedFormat"] = static_cast<int>(m_detectedFormat);
    fileMap["detectedFormatLabel"] = QString::fromStdString(FormatDetector::formatLabel(m_detectedFormat, filePath.toStdString()));

    // For images: read dimensions
    if (m_detectedFormat == DetectedFormat::JPEG ||
        m_detectedFormat == DetectedFormat::PNG  ||
        m_detectedFormat == DetectedFormat::WebP ||
        m_detectedFormat == DetectedFormat::TIFF ||
        m_detectedFormat == DetectedFormat::BMP  ||
        m_detectedFormat == DetectedFormat::GIF) {
        QImageReader reader(filePath);
        if (reader.canRead()) {
            QSize sz = reader.size();
            fileMap["imageWidth"]  = sz.width();
            fileMap["imageHeight"] = sz.height();
            fileMap["dimensions"]  = QString("%1×%2 px").arg(sz.width()).arg(sz.height());
        }
    }

    m_currentFileInfo = fileMap;
    checkFfmpegAvailability();
    emit currentFileInfoChanged();
}

// ── handleDroppedUrls ─────────────────────────────────────────────────────────

void AppController::handleDroppedUrls(const QList<QUrl>& urls) {
    static const QStringList supported = {
        "jpg","jpeg","jfif","png","webp","heic","heif","tiff","tif","bmp","gif",
        "pdf",
        "docx","pptx","xlsx",
        "mp4","mov","mkv","webm",
        "mp3","m4a","aac","ogg","wav"
    };

    QStringList validFiles;
    for (const auto& url : urls) {
        QString path;
        if (url.isLocalFile()) {
            path = url.toLocalFile();
        } else if (url.scheme() == "file") {
            path = QUrl::fromPercentEncoding(url.path().toUtf8());
#if defined(Q_OS_WIN)
            if (path.startsWith('/') && path.length() > 2 && path[2] == ':') {
                path = path.mid(1);
            }
#endif
        } else {
            path = url.toLocalFile();
            if (path.isEmpty()) {
                path = QUrl::fromPercentEncoding(url.path().toUtf8());
            }
            if (path.isEmpty()) {
                path = url.toString();
            }
        }

        path = QDir::fromNativeSeparators(path);
        QFileInfo fi(path);
        if (!fi.exists()) {
            QString decoded = QUrl::fromPercentEncoding(path.toUtf8());
            if (QFile::exists(decoded)) {
                path = decoded;
                fi.setFile(path);
            }
        }

        if (fi.exists() && fi.isFile() && supported.contains(fi.suffix().toLower())) {
            validFiles.append(fi.absoluteFilePath());
        }
    }

    if (validFiles.isEmpty()) {
        emit errorOccurred(QStringLiteral("Unsupported file type. Try: Photos (JPG, PNG, WebP, HEIC), PDF, Docs (DOCX, PPTX, XLSX), or Media (MP4, MP3, WAV)."));
        return;
    }

    m_loadedFiles = validFiles;
    emit loadedFilesChanged();

    m_currentResult.clear();
    m_batchResults.clear();
    m_resultHitFloor = false;
    m_floorNote.clear();
    m_compressionProgress = 0.0;
    emit currentResultChanged();
    emit batchResultsChanged();
    emit compressionProgressChanged();

    m_edgeWindowManager.setIsExpanded(true);
    analyzeInputFile(validFiles.first());
}

// ── handleClipboardPaste ──────────────────────────────────────────────────────

void AppController::handleClipboardPaste() {
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData*  mime      = clipboard->mimeData();

    if (mime->hasUrls()) {
        handleDroppedUrls(mime->urls());
        return;
    }

    if (mime->hasImage()) {
        QImage img = qvariant_cast<QImage>(mime->imageData());
        if (!img.isNull()) {
            QString tempDir  = QDir::tempPath() + "/edgedrop_paste";
            QDir().mkpath(tempDir);
            QString tempFile = QString("%1/screenshot_%2.png")
                .arg(tempDir)
                .arg(QDateTime::currentMSecsSinceEpoch());
            if (img.save(tempFile, "PNG")) {
                handleDroppedUrls({QUrl::fromLocalFile(tempFile)});
            }
        }
    }
}

// ── processCurrent ────────────────────────────────────────────────────────────

void AppController::processCurrent() {
    if (m_loadedFiles.isEmpty()) return;

    QString outDir = m_settingsManager.outputDirectory();

    auto buildReq = [&](const QString& path) {
        JobRequest req;
        req.inputFilePath   = path.toStdString();
        req.outputDirectory = outDir.toStdString();
        req.targetBytes     = m_targetBytes;
        req.detectedFormat  = FormatDetector::detect(path.toStdString());
        return req;
    };

    m_compressionProgress = 0.0;
    emit compressionProgressChanged();

    if (m_loadedFiles.size() == 1) {
        m_jobRunner.runJob(buildReq(m_loadedFiles.first()));
    } else {
        QList<JobRequest> batch;
        for (const auto& path : m_loadedFiles) batch.append(buildReq(path));
        m_jobRunner.runBatch(batch);
    }
    emit isProcessingChanged();
}

// ── cancelCurrent ─────────────────────────────────────────────────────────────

void AppController::cancelCurrent() {
    m_jobRunner.cancelAll();
    emit isProcessingChanged();
}

// ── clearLoadedFiles / removeFileAt / clearFiles / resetToHome ───────────────

void AppController::removeFileAt(int index) {
    if (index < 0 || index >= m_loadedFiles.size()) return;
    m_loadedFiles.removeAt(index);
    emit loadedFilesChanged();

    if (m_loadedFiles.isEmpty()) {
        clearFiles();
    } else {
        analyzeInputFile(m_loadedFiles.first());
    }
}

void AppController::clearFiles() {
    clearLoadedFiles();
}

void AppController::resetToHome() {
    cancelCurrent();
    clearLoadedFiles();
    emit resetToHomeRequested();
}

void AppController::clearLoadedFiles() {
    m_loadedFiles.clear();
    m_currentFileInfo.clear();
    m_currentResult.clear();
    m_batchResults.clear();
    m_resultHitFloor = false;
    m_floorNote.clear();
    m_compressionProgress = 0.0;
    m_detectedFormat = DetectedFormat::Unknown;
    emit loadedFilesChanged();
    emit currentFileInfoChanged();
    emit currentResultChanged();
    emit batchResultsChanged();
    emit compressionProgressChanged();
}

// ── startDragOut / showResultInFolder / copyResultToClipboard ─────────────────

void AppController::startDragOut() {
    QString outPath = m_currentResult.value("outputFilePath").toString();
    if (!outPath.isEmpty()) PlatformHelper::startFileDrag(outPath, this);
}

void AppController::startDragFile(const QString& filePath) {
    if (!filePath.isEmpty() && QFile::exists(filePath)) {
        PlatformHelper::startFileDrag(filePath, this);
    }
}

void AppController::showResultInFolder() {
    QString outPath = m_currentResult.value("outputFilePath").toString();
    if (!outPath.isEmpty()) PlatformHelper::showInFolder(outPath);
}

void AppController::copyResultToClipboard() {
    QString outPath = m_currentResult.value("outputFilePath").toString();
    if (!outPath.isEmpty()) PlatformHelper::copyFileToClipboard(outPath);
}

// ── chooseFile / chooseOutputFolder ───────────────────────────────────────────

void AppController::chooseFile() {
    QString path = QFileDialog::getOpenFileName(
        nullptr,
        "Select File to Compress",
        QDir::homePath(),
        "All Supported Files (*.jpg *.jpeg *.png *.webp *.heic *.heif *.tiff *.tif *.bmp *.gif "
        "*.pdf *.docx *.pptx *.xlsx "
        "*.mp4 *.mov *.mkv *.webm "
        "*.mp3 *.m4a *.aac *.ogg *.wav)"
    );
    if (!path.isEmpty()) handleDroppedUrls({QUrl::fromLocalFile(path)});
}

void AppController::chooseOutputFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        nullptr, "Choose Output Folder", m_settingsManager.outputDirectory());
    if (!dir.isEmpty()) m_settingsManager.setOutputDirectory(dir);
}

// ── addRecentFile ─────────────────────────────────────────────────────────────

void AppController::addRecentFile(const QString& outputPath) {
    if (outputPath.isEmpty() || !QFile::exists(outputPath)) return;
    m_settingsManager.addRecentFile(outputPath);

    m_recentFiles.clear();
    for (const QString& p : m_settingsManager.recentFiles()) {
        if (QFile::exists(p)) {
            QVariantMap m;
            m["path"] = p;
            m["fileName"] = QFileInfo(p).fileName();
            m_recentFiles.append(m);
            if (m_recentFiles.size() >= 3) break;
        }
    }
    emit recentFilesChanged();
}

// ── onJobProgress ─────────────────────────────────────────────────────────────

void AppController::onJobProgress(double progress) {
    if (m_compressionProgress != progress) {
        m_compressionProgress = progress;
        emit compressionProgressChanged();
    }
}

// ── Human Copy Translation Helpers (Section 1) ───────────────────────────────

static QString humanizeFloorNote(const std::string& rawNote, bool hitFloor) {
    QString note = QString::fromStdString(rawNote);
    if (note.contains("smaller than target", Qt::CaseInsensitive) ||
        note.contains("pass-through", Qt::CaseInsensitive) ||
        note.contains("already smaller", Qt::CaseInsensitive)) {
        return QStringLiteral("Already smaller than your target — saved as-is.");
    }
    if (note.contains("ghostscript", Qt::CaseInsensitive)) {
        return QStringLiteral("Saved the best size available. Install Ghostscript for even smaller PDFs.");
    }
    if (note.contains("converted wav", Qt::CaseInsensitive)) {
        return note;
    }
    if (note.contains("audio bitrate floor", Qt::CaseInsensitive) ||
        note.contains("audio playback", Qt::CaseInsensitive) ||
        note.contains("intelligible playback", Qt::CaseInsensitive)) {
        return QStringLiteral("Made it as small as possible while keeping audio clear.");
    }
    if (hitFloor || note.contains("floor", Qt::CaseInsensitive) || note.contains("legibility", Qt::CaseInsensitive)) {
        return QStringLiteral("Made it as small as possible while keeping it clear to read.");
    }
    if (!note.trimmed().isEmpty()) {
        return note;
    }
    return hitFloor ? QStringLiteral("Made it as small as possible while keeping it clear to read.") : QString();
}

static QString humanizeErrorMessage(const std::string& rawError) {
    QString err = QString::fromStdString(rawError);
    if (err.contains("password", Qt::CaseInsensitive) || err.contains("encrypt", Qt::CaseInsensitive)) {
        return QStringLiteral("This PDF is locked with a password. Unlock it first to try again.");
    }
    if (err.contains("ffmpeg", Qt::CaseInsensitive) && !err.contains("pass", Qt::CaseInsensitive)) {
        return QStringLiteral("Install FFmpeg to shrink videos and audio.");
    }
    if (err.contains("ghostscript", Qt::CaseInsensitive)) {
        return QStringLiteral("Install Ghostscript to shrink PDF files.");
    }
    if (err.contains("unsupported", Qt::CaseInsensitive) || err.contains("unknown format", Qt::CaseInsensitive)) {
        return QStringLiteral("This file type isn't supported yet.");
    }
    if (err.contains("duration", Qt::CaseInsensitive) || err.contains("probe failed", Qt::CaseInsensitive)) {
        return QStringLiteral("Could not read media duration. Check that the file is a valid video or audio file.");
    }
    if (err.contains("corrupt", Qt::CaseInsensitive) || err.contains("empty", Qt::CaseInsensitive) ||
        err.contains("could not open", Qt::CaseInsensitive) || err.contains("cannot open", Qt::CaseInsensitive) ||
        err.contains("failed to read", Qt::CaseInsensitive)) {
        return QStringLiteral("Couldn't read this file. Check that it isn't damaged or empty.");
    }
    return err.isEmpty() ? QStringLiteral("Couldn't process this file. Please try again.") : err;
}

// ── onJobFinished ─────────────────────────────────────────────────────────────

void AppController::onJobFinished(const EdgeDrop::JobResult& result) {
    m_compressionProgress = 1.0;
    emit compressionProgressChanged();
    emit isProcessingChanged();

    if (!result.success) {
        QString inPath = QString::fromStdString(result.inputFilePath.string());
        qWarning() << "Compression job failed for:" << inPath << "Raw error:" << QString::fromStdString(result.errorMessage);
        QVariantMap errMap;
        errMap["success"] = false;
        errMap["errorMessage"] = humanizeErrorMessage(result.errorMessage);
        errMap["inputFilePath"] = inPath;
        errMap["inputFileName"] = QFileInfo(inPath).fileName();
        m_currentResult = errMap;
        emit currentResultChanged();
        emit errorOccurred(humanizeErrorMessage(result.errorMessage));
        return;
    }

    m_resultHitFloor = result.hitFloor;
    m_floorNote      = humanizeFloorNote(result.floorNote, result.hitFloor);

    QString inPath  = QString::fromStdString(result.inputFilePath.string());
    QString outPath = QString::fromStdString(result.outputFilePath.string());

    QVariantMap resMap;
    resMap["success"]           = result.success;
    resMap["inputFilePath"]     = inPath;
    resMap["outputFilePath"]    = outPath;
    resMap["outputFileName"]    = QFileInfo(outPath).fileName();
    resMap["formattedInputSize"]  = QString::fromStdString(result.formattedInputSize());
    resMap["formattedOutputSize"] = QString::fromStdString(result.formattedOutputSize());
    resMap["savingsPercentage"] = QString::number(result.savingsPercentage(), 'f', 1);
    resMap["specSummary"]       = QString::fromStdString(result.specSummary());
    resMap["durationMs"]        = static_cast<qint64>(result.durationMs);
    resMap["hitFloor"]          = result.hitFloor;
    resMap["floorNote"]         = m_floorNote;

    m_currentResult = resMap;
    emit currentResultChanged();

    addRecentFile(outPath);
}

// ── onBatchProgress ───────────────────────────────────────────────────────────

void AppController::onBatchProgress(int completed, int total) {
    m_batchProgress = completed;
    m_batchTotal    = total;
    emit batchProgressChanged();
}

// ── onBatchFinished ───────────────────────────────────────────────────────────

void AppController::onBatchFinished(const QList<EdgeDrop::JobResult>& results) {
    emit isProcessingChanged();
    QVariantList list;
    for (const auto& res : results) {
        QString inPath  = QString::fromStdString(res.inputFilePath.string());
        QString outPath = QString::fromStdString(res.outputFilePath.string());
        QVariantMap m;
        m["success"]       = res.success;
        m["inputFileName"] = QFileInfo(inPath).fileName();
        m["outputFilePath"] = outPath;
        m["formattedSize"] = QString::fromStdString(res.formattedOutputSize());
        m["errorMessage"]  = humanizeErrorMessage(res.errorMessage);
        m["hitFloor"]      = res.hitFloor;
        list.append(m);
        if (res.success) addRecentFile(outPath);
    }
    m_batchResults = list;
    emit batchResultsChanged();
}

} // namespace EdgeDrop
