#include "PlatformHelper.hpp"
#include "app/EdgeWindowManager.hpp"
#include "app/AppController.hpp"
#include "app/SettingsManager.hpp"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QDrag>
#include <QPixmap>
#include <QFileInfo>
#include <QProcess>
#include <QDir>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HWINEVENTHOOK g_winEventHook = nullptr;
static EdgeDrop::EdgeWindowManager* g_winWindowManager = nullptr;
static EdgeDrop::AppController* g_winAppController = nullptr;
static HWND g_winHwnd = nullptr;

static void CALLBACK WinEventProc(HWINEVENTHOOK /*hWinEventHook*/,
                                  DWORD event,
                                  HWND hwnd,
                                  LONG /*idObject*/,
                                  LONG /*idChild*/,
                                  DWORD /*dwEventThread*/,
                                  DWORD /*dwmsEventTime*/) {
    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (hwnd != g_winHwnd && g_winWindowManager) {
            QMetaObject::invokeMethod(g_winWindowManager, []() {
                if (!g_winWindowManager || !g_winWindowManager->isExpanded()) return;
                if (g_winAppController) {
                    if (g_winAppController->settingsManager() && g_winAppController->settingsManager()->isPinned()) return;
                    if (g_winAppController->isProcessing()) return;
                }
                g_winWindowManager->setIsExpanded(false);
            }, Qt::QueuedConnection);
        }
    }
}
#endif

namespace EdgeDrop {

void PlatformHelper::configureNativeWindow(QQuickWindow* window,
                                          EdgeWindowManager* windowManager,
                                          AppController* appController) {
    if (!window) return;

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd) {
        g_winHwnd = hwnd;
        g_winWindowManager = windowManager;
        g_winAppController = appController;

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        // WS_EX_TOOLWINDOW hides from Taskbar & Alt+Tab, TOPMOST floats above, NOACTIVATE avoids focus theft
        exStyle |= WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

        // Strip WS_THICKFRAME and WS_MAXIMIZEBOX to strictly prevent resizing
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        if (!g_winEventHook) {
            g_winEventHook = SetWinEventHook(
                EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                NULL, WinEventProc, 0, 0,
                WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
            );
        }
    }
#endif
}

void PlatformHelper::cleanup() {
#ifdef _WIN32
    if (g_winEventHook) {
        UnhookWinEvent(g_winEventHook);
        g_winEventHook = nullptr;
    }
    g_winHwnd = nullptr;
    g_winWindowManager = nullptr;
    g_winAppController = nullptr;
#endif
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
    QStringList args;
    args << "/select," << QDir::toNativeSeparators(filePath);
    QProcess::startDetached("explorer.exe", args);
}

void PlatformHelper::copyFileToClipboard(const QString& filePath) {
    auto* mime = new QMimeData();
    mime->setUrls({QUrl::fromLocalFile(filePath)});
    QGuiApplication::clipboard()->setMimeData(mime);
}

void PlatformHelper::lockWindowSize(QQuickWindow* window, int width, int height) {
    if (!window) return;
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd) {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
    }
#else
    Q_UNUSED(width);
    Q_UNUSED(height);
#endif
}

void PlatformHelper::flushWindowFrame(QQuickWindow* window) {
    if (!window) return;
    window->requestUpdate();
}

QImage PlatformHelper::getWallpaperEdgeThumbnail() {
#ifdef _WIN32
    WCHAR wallpaperPath[MAX_PATH] = {0};
    if (SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, wallpaperPath, 0)) {
        QString path = QString::fromWCharArray(wallpaperPath);
        if (!path.isEmpty() && QFile::exists(path)) {
            QImage img(path);
            if (!img.isNull()) {
                return img.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
#endif
    return QImage();
}

} // namespace EdgeDrop
