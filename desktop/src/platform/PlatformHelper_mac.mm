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

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>
#import <dispatch/dispatch.h>

static id g_mouseMonitor = nil;
static id g_spaceObserver = nil;
static id g_appObserver = nil;
#endif

namespace EdgeDrop {

void PlatformHelper::configureNativeWindow(QQuickWindow* window,
                                          EdgeWindowManager* windowManager,
                                          AppController* appController) {
    if (!window) return;

#ifdef __APPLE__
    NSView* view = reinterpret_cast<NSView*>(window->winId());
    if (!view) return;
    NSWindow* nsWindow = [view window];
    if (!nsWindow) return;

    // Never auto-hide when another app is focused
    [nsWindow setHidesOnDeactivate:NO];

    // Keep floating above all standard desktop windows
    [nsWindow setLevel:NSFloatingWindowLevel];

    // Join all spaces and participate as full-screen auxiliary
    [nsWindow setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces |
                                     NSWindowCollectionBehaviorFullScreenAuxiliary |
                                     NSWindowCollectionBehaviorStationary)];

    // Transparent window background for QML rounded card
    [nsWindow setOpaque:NO];
    [nsWindow setBackgroundColor:[NSColor clearColor]];
    [nsWindow setHasShadow:NO];

    // Strictly lock window size — strip resizable style mask to avoid resize cursor
    [nsWindow setStyleMask:([nsWindow styleMask] & ~NSWindowStyleMaskResizable)];

    // Install NSTrackingArea for seamless hover response
    NSView* contentView = [nsWindow contentView];
    if (contentView) {
        bool alreadyTracking = false;
        for (NSTrackingArea* area in [contentView trackingAreas]) {
            if ([area owner] == contentView) {
                alreadyTracking = true;
                break;
            }
        }
        if (!alreadyTracking) {
            NSTrackingArea* trackingArea = [[NSTrackingArea alloc]
                initWithRect:NSZeroRect
                     options:(NSTrackingMouseEnteredAndExited |
                              NSTrackingActiveAlways |
                              NSTrackingInVisibleRect)
                       owner:contentView
                    userInfo:nil];
            [contentView addTrackingArea:trackingArea];
        }
    }

    // Cleanup existing observers
    PlatformHelper::cleanup();

    if (windowManager) {
        NSWindow* targetWindow = nsWindow;
        __block NSPoint downLocation = NSZeroPoint;
        __block NSInteger dragStartChangeCount = 0;
        __block BOOL isSystemFileDrag = NO;

        auto safeRetract = [windowManager, appController]() {
            QMetaObject::invokeMethod(windowManager, [windowManager, appController]() {
                if (!windowManager->isExpanded()) return;
                if (windowManager->isRepositioning() || windowManager->isSnapping()) return;
                if (appController) {
                    if (appController->settingsManager() && appController->settingsManager()->isPinned()) return;
                    if (appController->isProcessing()) return;
                }
                windowManager->setIsExpanded(false);
            }, Qt::QueuedConnection);
        };

        // 1. Screen / Space Switch Observer
        g_spaceObserver = [[[NSWorkspace sharedWorkspace] notificationCenter]
            addObserverForName:NSWorkspaceActiveSpaceDidChangeNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* /*note*/) {
            safeRetract();
            if (appController) {
                QMetaObject::invokeMethod(appController, "refreshWallpaper", Qt::QueuedConnection);
            }
        }];

        // 2. App Switch Observer (Cmd+Tab, clicking Dock, etc.)
        g_appObserver = [[[NSWorkspace sharedWorkspace] notificationCenter]
            addObserverForName:NSWorkspaceDidActivateApplicationNotification
                        object:nil
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification* note) {
            NSRunningApplication* activatedApp = [note.userInfo objectForKey:NSWorkspaceApplicationKey];
            if (activatedApp && ![activatedApp isEqual:[NSRunningApplication currentApplication]]) {
                if (!isSystemFileDrag) {
                    safeRetract();
                }
            }
        }];

        // 3. Drag-Aware Global Monitor (Scroll, RightClick, MiddleClick, Non-File Drag Detection)
        NSEventMask mask = NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged |
                           NSEventMaskLeftMouseUp | NSEventMaskRightMouseDown |
                           NSEventMaskScrollWheel | NSEventMaskOtherMouseDown;

        g_mouseMonitor = [NSEvent addGlobalMonitorForEventsMatchingMask:mask
                                                                handler:^(NSEvent* event) {
            if (!targetWindow || windowManager->isRepositioning() || windowManager->isSnapping()) return;
            NSEventType type = [event type];
            NSPoint curLoc = [NSEvent mouseLocation];
            BOOL isInside = NSPointInRect(curLoc, [targetWindow frame]);

            if (type == NSEventTypeLeftMouseDown) {
                downLocation = curLoc;
                dragStartChangeCount = [[NSPasteboard pasteboardWithName:NSPasteboardNameDrag] changeCount];
                // Do NOT collapse on mouse down outside
            } else if (type == NSEventTypeLeftMouseDragged) {
                if (!isInside) {
                    NSPasteboard* dragPb = [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
                    NSInteger curChangeCount = [dragPb changeCount];
                    BOOL hasFileUrl = NO;
                    if (curChangeCount != dragStartChangeCount) {
                        NSArray* types = [dragPb types];
                        if ([types containsObject:NSPasteboardTypeFileURL]) {
                            hasFileUrl = YES;
                        }
                    }

                    if (hasFileUrl) {
                        isSystemFileDrag = YES;
                        QMetaObject::invokeMethod(windowManager, [windowManager]() {
                            windowManager->setIsExpanded(true);
                        }, Qt::QueuedConnection);
                    } else {
                        // Non-file drag (e.g. text selection or window moving)
                        double dragDist = hypot(curLoc.x - downLocation.x, curLoc.y - downLocation.y);
                        if (dragDist > 12.0) {
                            isSystemFileDrag = NO;
                            safeRetract();
                        }
                    }
                }
            } else if (type == NSEventTypeLeftMouseUp) {
                if (isSystemFileDrag) {
                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(300 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
                        isSystemFileDrag = NO;
                    });
                } else if (!isInside) {
                    double dist = hypot(curLoc.x - downLocation.x, curLoc.y - downLocation.y);
                    if (dist < 6.0) {
                        // Stationary dismiss click outside window
                        safeRetract();
                    }
                }
            } else if (type == NSEventTypeRightMouseDown ||
                       type == NSEventTypeOtherMouseDown ||
                       type == NSEventTypeScrollWheel) {
                if (!isInside && !isSystemFileDrag) {
                    safeRetract();
                }
            }
        }];

        QObject::connect(window, &QObject::destroyed, []() {
            PlatformHelper::cleanup();
        });
        if (qApp) {
            QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
                PlatformHelper::cleanup();
            });
        }
    }
#endif
}

void PlatformHelper::cleanup() {
#ifdef __APPLE__
    if (g_mouseMonitor) {
        [NSEvent removeMonitor:g_mouseMonitor];
        g_mouseMonitor = nil;
    }
    if (g_spaceObserver) {
        [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:g_spaceObserver];
        g_spaceObserver = nil;
    }
    if (g_appObserver) {
        [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:g_appObserver];
        g_appObserver = nil;
    }
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
#ifdef __APPLE__
    NSString* path = filePath.toNSString();
    [[NSWorkspace sharedWorkspace] selectFile:path inFileViewerRootedAtPath:@""];
#else
    QFileInfo info(filePath);
    QProcess::startDetached("open", {info.absolutePath()});
#endif
}

void PlatformHelper::copyFileToClipboard(const QString& filePath) {
    auto* mime = new QMimeData();
    mime->setUrls({QUrl::fromLocalFile(filePath)});
    QGuiApplication::clipboard()->setMimeData(mime);
}

void PlatformHelper::lockWindowSize(QQuickWindow* window, int width, int /*height*/) {
    if (!window) return;
#ifdef __APPLE__
    NSView* view = reinterpret_cast<NSView*>(window->winId());
    if (!view) return;
    NSWindow* nsWindow = [view window];
    if (!nsWindow) return;

    // Keep window non-resizable by user dragging, without locking min/max programmatic sizes
    [nsWindow setStyleMask:([nsWindow styleMask] & ~NSWindowStyleMaskResizable)];
    [nsWindow setOpaque:NO];
    [nsWindow setBackgroundColor:[NSColor clearColor]];
    if (width <= 20) {
        [nsWindow setHasShadow:NO];
        [nsWindow invalidateShadow];
    }
#endif
}

void PlatformHelper::flushWindowFrame(QQuickWindow* window) {
    if (!window) return;
    window->requestUpdate();
#ifdef __APPLE__
    NSView* view = reinterpret_cast<NSView*>(window->winId());
    if (!view) return;
    NSWindow* nsWindow = [view window];
    if (!nsWindow) return;

    [nsWindow setOpaque:NO];
    [nsWindow setBackgroundColor:[NSColor clearColor]];
    [nsWindow setHasShadow:NO];
    [nsWindow invalidateShadow];
    [nsWindow displayIfNeeded];
#endif
}

QImage PlatformHelper::getWallpaperEdgeThumbnail() {
#ifdef __APPLE__
    @autoreleasepool {
        NSScreen* screen = [NSScreen mainScreen];
        if (!screen) return QImage();

        NSURL* url = [[NSWorkspace sharedWorkspace] desktopImageURLForScreen:screen];
        if (!url) return QImage();

        // Handle both direct image files and directory URLs (e.g. dynamic desktop wallpaper folder)
        NSNumber* isDir = nil;
        if ([url getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil] && [isDir boolValue]) {
            NSFileManager* fm = [NSFileManager defaultManager];
            NSArray<NSURL*>* contents = [fm contentsOfDirectoryAtURL:url
                                          includingPropertiesForKeys:nil
                                                             options:NSDirectoryEnumerationSkipsHiddenFiles
                                                               error:nil];
            for (NSURL* item in contents) {
                NSString* ext = [[item pathExtension] lowercaseString];
                if ([ext isEqualToString:@"jpg"] || [ext isEqualToString:@"jpeg"] ||
                    [ext isEqualToString:@"png"] || [ext isEqualToString:@"heic"] ||
                    [ext isEqualToString:@"tiff"]) {
                    url = item;
                    break;
                }
            }
        }

        CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
        CGImageRef cgImage = NULL;
        if (src) {
            cgImage = CGImageSourceCreateImageAtIndex(src, 0, NULL);
            CFRelease(src);
        }
        if (!cgImage) {
            NSImage* nsImg = [[NSImage alloc] initWithContentsOfURL:url];
            if (nsImg) {
                cgImage = [nsImg CGImageForProposedRect:NULL context:nil hints:nil];
            }
        }
        if (!cgImage) return QImage();

        const size_t width = 64;
        const size_t height = 64;
        const size_t bytesPerRow = width * 4;
        std::vector<unsigned char> rawData(bytesPerRow * height, 0);
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            rawData.data(),
            width,
            height,
            8,
            bytesPerRow,
            colorSpace,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big)
        );
        CGColorSpaceRelease(colorSpace);

        if (!ctx) {
            CGImageRelease(cgImage);
            return QImage();
        }

        CGContextSetInterpolationQuality(ctx, kCGInterpolationHigh);
        CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cgImage);
        CGContextRelease(ctx);
        CGImageRelease(cgImage);

        QImage qimg(rawData.data(), static_cast<int>(width), static_cast<int>(height), static_cast<int>(bytesPerRow), QImage::Format_RGBA8888);
        return qimg.copy();
    }
#else
    return QImage();
#endif
}

} // namespace EdgeDrop
