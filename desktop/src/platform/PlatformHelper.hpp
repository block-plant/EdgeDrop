#pragma once

#include <QString>
#include <QQuickWindow>

namespace EdgeDrop {

class EdgeWindowManager;
class AppController;

class PlatformHelper {
public:
    // Configures window behavior (CanJoinAllSpaces on macOS, WS_EX_TOOLWINDOW on Windows, etc.)
    // and installs the global click-outside-to-close event monitor
    static void configureNativeWindow(QQuickWindow* window,
                                      EdgeWindowManager* windowManager = nullptr,
                                      AppController* appController = nullptr);

    // Cleans up native monitors and handlers
    static void cleanup();

    // Initiates native OS file drag carrying local file URL
    static void startFileDrag(const QString& filePath, QObject* dragSource = nullptr);

    // Reveals file in native file manager (Finder / Explorer / Nautilus)
    static void showInFolder(const QString& filePath);

    // Copies file to OS clipboard
    static void copyFileToClipboard(const QString& filePath);

    // Checks if current session is running under Wayland
    static bool isWayland();

    // Strictly locks native window size preventing resizing, stretching, or resize cursors
    static void lockWindowSize(QQuickWindow* window, int width, int height);

    // Flushes window frame and invalidates shadows to prevent stale Metal frames
    static void flushWindowFrame(QQuickWindow* window);

    // Extracts 64x64 zero-permission desktop wallpaper thumbnail
    static QImage getWallpaperEdgeThumbnail();
};

} // namespace EdgeDrop
