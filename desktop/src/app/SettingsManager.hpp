#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>

namespace EdgeDrop {

class SettingsManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString outputDirectory READ outputDirectory WRITE setOutputDirectory NOTIFY outputDirectoryChanged)
    Q_PROPERTY(QString screenEdge READ screenEdge WRITE setScreenEdge NOTIFY screenEdgeChanged)
    Q_PROPERTY(double edgeOffsetPercent READ edgeOffsetPercent WRITE setEdgeOffsetPercent NOTIFY edgeOffsetPercentChanged)
    Q_PROPERTY(double edgeYRatio READ edgeYRatio WRITE setEdgeYRatio NOTIFY edgeYRatioChanged)
    Q_PROPERTY(int autoHideDelayMs READ autoHideDelayMs WRITE setAutoHideDelayMs NOTIFY autoHideDelayMsChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(QString vibe READ vibe WRITE setVibe NOTIFY vibeChanged)
    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY launchAtLoginChanged)
    Q_PROPERTY(bool isPinned READ isPinned WRITE setIsPinned NOTIFY isPinnedChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles WRITE setRecentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(int customCardWidth READ customCardWidth WRITE setCustomCardWidth NOTIFY customCardSizeChanged)
    Q_PROPERTY(int customCardHeight READ customCardHeight WRITE setCustomCardHeight NOTIFY customCardSizeChanged)

public:
    explicit SettingsManager(QObject* parent = nullptr);

    [[nodiscard]] QString outputDirectory() const { return m_outputDirectory; }
    void setOutputDirectory(const QString& dir);

    [[nodiscard]] QString screenEdge() const { return m_screenEdge; }
    void setScreenEdge(const QString& edge);

    [[nodiscard]] double edgeOffsetPercent() const { return m_edgeOffsetPercent; }
    void setEdgeOffsetPercent(double percent);

    [[nodiscard]] double edgeYRatio() const { return m_edgeOffsetPercent; }
    void setEdgeYRatio(double ratio) { setEdgeOffsetPercent(ratio); }

    [[nodiscard]] int autoHideDelayMs() const { return m_autoHideDelayMs; }
    void setAutoHideDelayMs(int ms);

    [[nodiscard]] QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString& mode);

    [[nodiscard]] QString vibe() const { return m_vibe; }
    void setVibe(const QString& vibe);

    [[nodiscard]] bool launchAtLogin() const { return m_launchAtLogin; }
    void setLaunchAtLogin(bool enable);

    [[nodiscard]] bool isPinned() const { return m_isPinned; }
    void setIsPinned(bool pinned);

    [[nodiscard]] QStringList recentFiles() const { return m_recentFiles; }
    void setRecentFiles(const QStringList& files);
    Q_INVOKABLE void addRecentFile(const QString& filePath);

    [[nodiscard]] int customCardWidth() const { return m_customCardWidth; }
    void setCustomCardWidth(int width);

    [[nodiscard]] int customCardHeight() const { return m_customCardHeight; }
    void setCustomCardHeight(int height);

    Q_INVOKABLE void resetCustomCardSize();

    Q_INVOKABLE void resetSettings();

signals:
    void outputDirectoryChanged(const QString& dir);
    void screenEdgeChanged(const QString& edge);
    void edgeOffsetPercentChanged(double percent);
    void edgeYRatioChanged(double ratio);
    void autoHideDelayMsChanged(int ms);
    void themeModeChanged(const QString& mode);
    void vibeChanged(const QString& vibe);
    void launchAtLoginChanged(bool enable);
    void isPinnedChanged(bool pinned);
    void recentFilesChanged(const QStringList& files);
    void customCardSizeChanged();

private:
    void load();
    void save();

    QSettings m_settings;
    QString m_outputDirectory;
    QString m_screenEdge;
    double m_edgeOffsetPercent = 0.40;
    int m_autoHideDelayMs = 2500;
    QString m_themeMode;
    QString m_vibe = "adaptive";
    bool m_launchAtLogin = false;
    bool m_isPinned = false;
    QStringList m_recentFiles;
    int m_customCardWidth = 0;
    int m_customCardHeight = 0;
};

} // namespace EdgeDrop
