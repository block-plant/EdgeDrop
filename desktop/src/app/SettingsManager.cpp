#include "SettingsManager.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QFile>

namespace EdgeDrop {

SettingsManager::SettingsManager(QObject* parent)
    : QObject(parent),
      m_settings("EdgeDrop", "EdgeDrop") {
    load();
}

void SettingsManager::load() {
    QString defaultDownloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    m_outputDirectory = m_settings.value("outputDirectory", defaultDownloads).toString();
    m_screenEdge = m_settings.value("screenEdge", "right").toString().toLower();
    if (m_screenEdge != "left" && m_screenEdge != "right") m_screenEdge = "right";
    m_edgeOffsetPercent = m_settings.value("edgeOffsetPercent", 0.40).toDouble();
    m_autoHideDelayMs = m_settings.value("autoHideDelayMs", 2500).toInt();
    m_themeMode = m_settings.value("themeMode", "System").toString();
    m_vibe = m_settings.value("vibe", "adaptive").toString().toLower();
    m_launchAtLogin = m_settings.value("launchAtLogin", false).toBool();
    m_isPinned = m_settings.value("isPinned", false).toBool();
    m_recentFiles = m_settings.value("recentFiles").toStringList();
    // Prune missing files
    QStringList valid;
    for (const auto& f : m_recentFiles) {
        if (QFile::exists(f)) valid.append(f);
    }
    m_recentFiles = valid;
}

void SettingsManager::save() {
    m_settings.setValue("outputDirectory", m_outputDirectory);
    m_settings.setValue("screenEdge", m_screenEdge);
    m_settings.setValue("edgeOffsetPercent", m_edgeOffsetPercent);
    m_settings.setValue("autoHideDelayMs", m_autoHideDelayMs);
    m_settings.setValue("themeMode", m_themeMode);
    m_settings.setValue("vibe", m_vibe);
    m_settings.setValue("launchAtLogin", m_launchAtLogin);
    m_settings.setValue("isPinned", m_isPinned);
    m_settings.setValue("recentFiles", m_recentFiles);
}

void SettingsManager::setOutputDirectory(const QString& dir) {
    if (m_outputDirectory != dir) {
        m_outputDirectory = dir;
        m_settings.setValue("outputDirectory", dir);
        emit outputDirectoryChanged(dir);
    }
}

void SettingsManager::setScreenEdge(const QString& edge) {
    QString lower = edge.toLower();
    if (lower != "left" && lower != "right") lower = "right";
    if (m_screenEdge != lower) {
        m_screenEdge = lower;
        m_settings.setValue("screenEdge", lower);
        emit screenEdgeChanged(lower);
    }
}

void SettingsManager::setEdgeOffsetPercent(double percent) {
    percent = std::clamp(percent, 0.05, 0.95);
    if (std::abs(m_edgeOffsetPercent - percent) > 0.001) {
        m_edgeOffsetPercent = percent;
        m_settings.setValue("edgeOffsetPercent", percent);
        emit edgeOffsetPercentChanged(percent);
        emit edgeYRatioChanged(percent);
    }
}

void SettingsManager::setVibe(const QString& vibe) {
    QString lower = vibe.toLower();
    if (m_vibe != lower) {
        m_vibe = lower;
        m_settings.setValue("vibe", lower);
        emit vibeChanged(lower);
    }
}

void SettingsManager::setAutoHideDelayMs(int ms) {
    if (m_autoHideDelayMs != ms) {
        m_autoHideDelayMs = ms;
        m_settings.setValue("autoHideDelayMs", ms);
        emit autoHideDelayMsChanged(ms);
    }
}

void SettingsManager::setThemeMode(const QString& mode) {
    if (m_themeMode != mode) {
        m_themeMode = mode;
        m_settings.setValue("themeMode", mode);
        emit themeModeChanged(mode);
    }
}

void SettingsManager::setLaunchAtLogin(bool enable) {
    if (m_launchAtLogin != enable) {
        m_launchAtLogin = enable;
        m_settings.setValue("launchAtLogin", enable);
        emit launchAtLoginChanged(enable);
    }
}

void SettingsManager::setIsPinned(bool pinned) {
    if (m_isPinned != pinned) {
        m_isPinned = pinned;
        m_settings.setValue("isPinned", pinned);
        emit isPinnedChanged(pinned);
    }
}

void SettingsManager::setRecentFiles(const QStringList& files) {
    if (m_recentFiles != files) {
        m_recentFiles = files;
        m_settings.setValue("recentFiles", m_recentFiles);
        emit recentFilesChanged(m_recentFiles);
    }
}

void SettingsManager::addRecentFile(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) return;
    m_recentFiles.removeAll(filePath);
    m_recentFiles.prepend(filePath);
    while (m_recentFiles.size() > 5) {
        m_recentFiles.removeLast();
    }
    m_settings.setValue("recentFiles", m_recentFiles);
    emit recentFilesChanged(m_recentFiles);
}

void SettingsManager::resetSettings() {
    m_settings.clear();
    load();
    emit outputDirectoryChanged(m_outputDirectory);
    emit screenEdgeChanged(m_screenEdge);
    emit edgeOffsetPercentChanged(m_edgeOffsetPercent);
    emit edgeYRatioChanged(m_edgeOffsetPercent);
    emit autoHideDelayMsChanged(m_autoHideDelayMs);
    emit themeModeChanged(m_themeMode);
    emit vibeChanged(m_vibe);
    emit launchAtLoginChanged(m_launchAtLogin);
    emit isPinnedChanged(m_isPinned);
    emit recentFilesChanged(m_recentFiles);
}

} // namespace EdgeDrop
