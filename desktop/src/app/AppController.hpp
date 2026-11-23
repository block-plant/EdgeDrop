#pragma once

#include "SettingsManager.hpp"
#include "EdgeWindowManager.hpp"
#include "WallpaperThemeEngine.hpp"
#include "../engine/JobRunner.hpp"
#include <edgedrop/format_detector.hpp>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QUrl>
#include <QTimer>

namespace EdgeDrop {

class AppController : public QObject {
    Q_OBJECT

    // ── File state ───────────────────────────────────────────────────────────
    Q_PROPERTY(QStringList  loadedFiles         READ loadedFiles        NOTIFY loadedFilesChanged)
    Q_PROPERTY(QVariantMap  currentFileInfo      READ currentFileInfo    NOTIFY currentFileInfoChanged)
    Q_PROPERTY(bool         isProcessing         READ isProcessing       NOTIFY isProcessingChanged)
    Q_PROPERTY(QVariantMap  currentResult        READ currentResult      NOTIFY currentResultChanged)
    Q_PROPERTY(QVariantList batchResults         READ batchResults       NOTIFY batchResultsChanged)
    Q_PROPERTY(int          batchProgressVal     READ batchProgressVal   NOTIFY batchProgressChanged)
    Q_PROPERTY(int          batchTotalVal        READ batchTotalVal      NOTIFY batchProgressChanged)

    // ── v2 Target ────────────────────────────────────────────────────────────
    Q_PROPERTY(qint64 targetBytes READ targetBytes WRITE setTargetBytes NOTIFY targetBytesChanged)

    // ── Compression progress (0.0–1.0) ───────────────────────────────────────
    Q_PROPERTY(double compressionProgress READ compressionProgress NOTIFY compressionProgressChanged)

    // ── Format label (e.g. "JPG · 3.1 MB") ───────────────────────────────────
    Q_PROPERTY(QString detectedFormatLabel READ detectedFormatLabel NOTIFY currentFileInfoChanged)

    // ── Floor guard ───────────────────────────────────────────────────────────
    Q_PROPERTY(bool    resultHitFloor READ resultHitFloor NOTIFY currentResultChanged)
    Q_PROPERTY(QString floorNote      READ floorNote      NOTIFY currentResultChanged)

    // ── Recent files ──────────────────────────────────────────────────────────
    Q_PROPERTY(QVariantList recentFiles READ recentFiles NOTIFY recentFilesChanged)

    // ── Sub-managers ─────────────────────────────────────────────────────────
    Q_PROPERTY(SettingsManager*    settingsManager    READ settingsManager    CONSTANT)
    Q_PROPERTY(EdgeWindowManager*  edgeWindowManager  READ edgeWindowManager  CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] QStringList  loadedFiles()       const { return m_loadedFiles; }
    [[nodiscard]] QVariantMap  currentFileInfo()   const { return m_currentFileInfo; }
    [[nodiscard]] bool         isProcessing()      const { return m_jobRunner.isRunning(); }
    [[nodiscard]] QVariantMap  currentResult()     const { return m_currentResult; }
    [[nodiscard]] QVariantList batchResults()      const { return m_batchResults; }
    [[nodiscard]] int          batchProgressVal()  const { return m_batchProgress; }
    [[nodiscard]] int          batchTotalVal()     const { return m_batchTotal; }

    [[nodiscard]] qint64 targetBytes()             const { return m_targetBytes; }
    void setTargetBytes(qint64 bytes);

    [[nodiscard]] double compressionProgress()     const { return m_compressionProgress; }
    [[nodiscard]] QString detectedFormatLabel()    const;
    [[nodiscard]] bool    resultHitFloor()         const { return m_resultHitFloor; }
    [[nodiscard]] QString floorNote()              const { return m_floorNote; }
    [[nodiscard]] QVariantList recentFiles()       const { return m_recentFiles; }

    [[nodiscard]] SettingsManager*   settingsManager()   { return &m_settingsManager; }
    [[nodiscard]] EdgeWindowManager* edgeWindowManager() { return &m_edgeWindowManager; }
    [[nodiscard]] WallpaperThemeEngine* wallpaperThemeEngine() { return &m_wallpaperThemeEngine; }

    // ── QML Invokables ────────────────────────────────────────────────────────
    Q_INVOKABLE void handleDroppedUrls(const QList<QUrl>& urls);
    Q_INVOKABLE void loadFiles(const QList<QUrl>& urls) { handleDroppedUrls(urls); }
    Q_INVOKABLE void handleClipboardPaste();
    Q_INVOKABLE void processCurrent();
    Q_INVOKABLE void cancelCurrent();
    Q_INVOKABLE void clearLoadedFiles();
    Q_INVOKABLE void removeFileAt(int index);
    Q_INVOKABLE void clearFiles();
    Q_INVOKABLE void resetToHome();
    Q_INVOKABLE void startDragOut();
    Q_INVOKABLE void startDragFile(const QString& filePath);
    Q_INVOKABLE void showResultInFolder();
    Q_INVOKABLE void copyResultToClipboard();
    Q_INVOKABLE void chooseFile();
    Q_INVOKABLE void chooseOutputFolder();
    Q_INVOKABLE void refreshWallpaper() { m_wallpaperThemeEngine.refreshWallpaper(); }

    // Convenience wrappers
    Q_INVOKABLE void setTargetKb(int kb) { setTargetBytes(static_cast<qint64>(kb) * 1024); }
    Q_INVOKABLE void setTargetMb(double mb) { setTargetBytes(static_cast<qint64>(mb * 1024.0 * 1024.0)); }

signals:
    void loadedFilesChanged();
    void currentFileInfoChanged();
    void isProcessingChanged();
    void currentResultChanged();
    void batchResultsChanged();
    void batchProgressChanged();
    void targetBytesChanged();
    void compressionProgressChanged();
    void recentFilesChanged();
    void resetToHomeRequested();
    void errorOccurred(const QString& message);

private slots:
    void onJobFinished(const EdgeDrop::JobResult& result);
    void onJobProgress(double progress);
    void onBatchProgress(int completed, int total);
    void onBatchFinished(const QList<EdgeDrop::JobResult>& results);

private:
    void analyzeInputFile(const QString& filePath);
    void addRecentFile(const QString& outputPath);

    SettingsManager    m_settingsManager;
    EdgeWindowManager  m_edgeWindowManager;
    WallpaperThemeEngine m_wallpaperThemeEngine;
    JobRunner          m_jobRunner;

    QStringList  m_loadedFiles;
    QVariantMap  m_currentFileInfo;
    QVariantMap  m_currentResult;
    QVariantList m_batchResults;
    QVariantList m_recentFiles;   // last 3 successful output paths
    int          m_batchProgress = 0;
    int          m_batchTotal    = 0;

    qint64 m_targetBytes         = 0;
    double m_compressionProgress = 0.0;
    bool   m_resultHitFloor      = false;
    QString m_floorNote;

    DetectedFormat m_detectedFormat = DetectedFormat::Unknown;
};

} // namespace EdgeDrop
