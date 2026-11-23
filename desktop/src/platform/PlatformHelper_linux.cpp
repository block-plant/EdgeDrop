#include "PlatformHelper.hpp"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QDrag>
#include <QPixmap>
#include <QFileInfo>
#include <QProcess>
#include <cstdlib>

namespace EdgeDrop {

void PlatformHelper::configureNativeWindow(QQuickWindow* window,
                                          EdgeWindowManager* /*windowManager*/,
                                          AppController* /*appController*/) {
    if (!window) return;
    // On Linux X11, Qt::Tool + Qt::WindowStaysOnTopHint + Qt::FramelessWindowHint
    // already sets _NET_WM_STATE_SKIP_TASKBAR and utility dock behavior.
}

void PlatformHelper::cleanup() {
}

void PlatformHelper::startFileDrag(const QString& filePath, QObject* dragSource) {
    QFileInfo info(filePath);
    if (!info.exists()) return;

    auto* drag = new QDrag(dragSource);
    auto* mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(filePath)});
    drag->setMimeData(mimeData);

    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    drag->setPixmap(pixmap);

    drag->exec(Qt::CopyAction | Qt::MoveAction);
}

void PlatformHelper::showInFolder(const QString& filePath) {
    QFileInfo info(filePath);
    QProcess::startDetached("xdg-open", {info.absolutePath()});
}

void PlatformHelper::copyFileToClipboard(const QString& filePath) {
    auto* mime = new QMimeData();
    mime->setUrls({QUrl::fromLocalFile(filePath)});
    QGuiApplication::clipboard()->setMimeData(mime);
}

bool PlatformHelper::isWayland() {
    const char* xdg = std::getenv("XDG_SESSION_TYPE");
    if (xdg && std::string(xdg) == "wayland") {
        return true;
    }
    const char* display = std::getenv("WAYLAND_DISPLAY");
    return display != nullptr;
}

void PlatformHelper::lockWindowSize(QQuickWindow* /*window*/, int /*width*/, int /*height*/) {
}

void PlatformHelper::flushWindowFrame(QQuickWindow* window) {
    if (!window) return;
    window->requestUpdate();
}

QImage PlatformHelper::getWallpaperEdgeThumbnail() {
    // Attempt GNOME gsettings wallpaper path extraction
    QProcess proc;
    proc.start("gsettings", {"get", "org.gnome.desktop.background", "picture-uri"});
    if (proc.waitForFinished(400)) {
        QString uri = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        uri.remove('\'');
        if (uri.startsWith("file://")) {
            QUrl url(uri);
            QString path = url.toLocalFile();
            if (!path.isEmpty() && QFile::exists(path)) {
                QImage img(path);
                if (!img.isNull()) {
                    return img.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
            }
        }
    }
    return QImage();
}

} // namespace EdgeDrop
