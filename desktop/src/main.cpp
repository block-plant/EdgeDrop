#include "app/AppController.hpp"
#include "engine/QtSubprocessRunner.hpp"
#include "edgedrop/compressor_router.hpp"
#include "platform/PlatformHelper.hpp"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <filesystem>

int main(int argc, char *argv[]) {
  // High DPI and platform attributes
  QApplication::setOrganizationName("EdgeDrop");
  QApplication::setApplicationName("EdgeDrop");
  QApplication::setApplicationVersion("3.0.0");

  QApplication app(argc, argv);

  // Single-instance guard via QLocalServer with authenticated token
  const QString serverName = QStringLiteral("EdgeDropSingleInstanceServer");
  const QByteArray wakeToken = "EDGEDROP_WAKE_V1";
  QLocalSocket existingInstanceSocket;
  existingInstanceSocket.connectToServer(serverName);
  if (existingInstanceSocket.waitForConnected(500)) {
    existingInstanceSocket.write(wakeToken);
    existingInstanceSocket.waitForBytesWritten(500);
    existingInstanceSocket.disconnectFromServer();
    qInfo() << "EdgeDrop: Another instance is already running. Signaled wake. Exiting.";
    return 0;
  }

  // Register hardened subprocess runner into core compressor router
  EdgeDrop::CompressorRouter::setSubprocessRunner(std::make_shared<EdgeDrop::QtSubprocessRunner>());

  // Set platform-native system font to prevent Qt font alias scanning overhead
  QFont sysFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
  if (!sysFont.family().isEmpty()) {
    QApplication::setFont(sysFont);
    QFont::insertSubstitution("Sans Serif", sysFont.family());
  }

  // Keep app running in tray when window hides
  app.setQuitOnLastWindowClosed(false);

  QQmlApplicationEngine engine;

  EdgeDrop::AppController controller;
  engine.rootContext()->setContextProperty("appController", &controller);
  engine.rootContext()->setContextProperty("settingsManager",
                                           controller.settingsManager());
  engine.rootContext()->setContextProperty("edgeWindowManager",
                                           controller.edgeWindowManager());
  engine.rootContext()->setContextProperty("wallpaperThemeEngine",
                                           controller.wallpaperThemeEngine());

  // Listen for second-instance invocations to toggle window (isolated to current user)
  QLocalServer localServer;
  localServer.setSocketOptions(QLocalServer::UserAccessOption);
  QLocalServer::removeServer(serverName);
  if (!localServer.listen(serverName)) {
    qWarning() << "EdgeDrop: Failed to listen on single instance server:"
               << localServer.errorString();
  }
  QObject::connect(&localServer, &QLocalServer::newConnection, [&controller, &localServer, wakeToken]() {
    QLocalSocket *client = localServer.nextPendingConnection();
    if (client) {
      if (client->waitForReadyRead(500)) {
        QByteArray data = client->read(64);
        if (data.startsWith(wakeToken)) {
          controller.edgeWindowManager()->setIsExpanded(true);
        }
      }
      client->close();
      client->deleteLater();
    }
  });

  auto createTrayIcon = []() -> QIcon {
    QPixmap pix(44, 44); // 22x22 @2x Retina
    pix.setDevicePixelRatio(2.0);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Minimal rounded square + right-edge pill glyph
    QPen pen(Qt::black, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(3.5, 4.5, 11.5, 13.0), 3.0, 3.0);

    // Solid right-edge pill
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawRoundedRect(QRectF(17.0, 6.0, 2.8, 10.0), 1.4, 1.4);
    p.end();

    QIcon icon(pix);
    icon.setIsMask(true); // macOS auto-tints white/black for Dark/Light menu bar
    return icon;
  };

  // Declare QMenu before QSystemTrayIcon for safe teardown order
  QMenu trayMenu;
  QAction *toggleAction = trayMenu.addAction("Toggle EdgeDrop");
  QObject::connect(toggleAction, &QAction::triggered, [&controller]() {
    bool curr = controller.edgeWindowManager()->isExpanded();
    controller.edgeWindowManager()->setIsExpanded(!curr);
  });

  trayMenu.addSeparator();
  QAction *quitAction = trayMenu.addAction("Quit EdgeDrop");
  quitAction->setShortcut(QKeySequence::Quit);
  QObject::connect(quitAction, &QAction::triggered, &app,
                   &QCoreApplication::quit);

  QSystemTrayIcon trayIcon;
  trayIcon.setIcon(createTrayIcon());
  trayIcon.setToolTip("EdgeDrop — Offline Same-Type Compressor");
  trayIcon.setContextMenu(&trayMenu);
  QObject::connect(&trayIcon, &QSystemTrayIcon::activated,
                   [&controller](QSystemTrayIcon::ActivationReason reason) {
                     if (reason == QSystemTrayIcon::Trigger) {
                       bool curr = controller.edgeWindowManager()->isExpanded();
                       controller.edgeWindowManager()->setIsExpanded(!curr);
                     }
                   });

  trayIcon.show();

  // Connect objectCreationFailed so QML load errors exit cleanly instead of hanging in CLI
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [](const QUrl &errUrl) {
        qCritical() << "EdgeDrop: Failed to load root QML from" << errUrl;
        QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [&controller, &trayIcon](QObject *obj, const QUrl &objUrl) {
        if (!obj) {
          qCritical() << "EdgeDrop: Root QML object creation failed for"
                      << objUrl;
          QCoreApplication::exit(-1);
          return;
        }
        if (auto *window = qobject_cast<QQuickWindow *>(obj)) {
          controller.edgeWindowManager()->setTargetWindow(window);
          window->show();
          EdgeDrop::PlatformHelper::configureNativeWindow(window, controller.edgeWindowManager(), &controller);

          QScreen *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
          QRect avail = screen ? screen->availableGeometry() : QRect();
          qInfo() << "EdgeDrop Startup: Screen availableGeometry:" << avail
                  << "| Window:" << window->geometry()
                  << "| SystemTray available:" << QSystemTrayIcon::isSystemTrayAvailable()
                  << "| TrayIcon visible:" << trayIcon.isVisible();

          // First-Launch Auto-Peek: smoothly expand edge card for 2.5s to show where the notch lives
          controller.edgeWindowManager()->setIsExpanded(true);
          QTimer::singleShot(2500, [&controller]() {
            if (!controller.settingsManager()->isPinned() &&
                !controller.isProcessing() &&
                controller.loadedFiles().isEmpty()) {
              controller.edgeWindowManager()->setIsExpanded(false);
            }
          });
        }
      },
      Qt::QueuedConnection);

  // Add import paths for EdgeDrop QML module and local components
  engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
  engine.addImportPath(QStringLiteral("qrc:/"));
  engine.addImportPath(QDir::currentPath());

  if (QFile::exists(QStringLiteral(":/qt/qml/EdgeDrop/desktop/qml/Main.qml"))) {
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/EdgeDrop/desktop/qml/Main.qml")));
  } else if (QFile::exists(QStringLiteral(":/EdgeDrop/desktop/qml/Main.qml"))) {
    engine.load(QUrl(QStringLiteral("qrc:/EdgeDrop/desktop/qml/Main.qml")));
  } else if (QFile::exists(QStringLiteral(":/qt/qml/EdgeDrop/qml/Main.qml"))) {
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/EdgeDrop/qml/Main.qml")));
  } else if (QFile::exists(QStringLiteral(":/EdgeDrop/qml/Main.qml"))) {
    engine.load(QUrl(QStringLiteral("qrc:/EdgeDrop/qml/Main.qml")));
  } else if (QFile::exists(QStringLiteral("desktop/qml/Main.qml"))) {
    engine.load(QUrl::fromLocalFile(QStringLiteral("desktop/qml/Main.qml")));
  } else {
    engine.load(QUrl::fromLocalFile(QStringLiteral("qml/Main.qml")));
  }

  // Teardown cleanup
  QObject::connect(&app, &QCoreApplication::aboutToQuit, [&controller]() {
    controller.cancelCurrent();
    EdgeDrop::PlatformHelper::cleanup();

    // Clean up any stray temporary files
    auto cleanupTmp = [](const std::filesystem::path& dir) {
      std::error_code ec;
      if (std::filesystem::exists(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
          if (entry.path().filename().string().find(".edgedrop.tmp") != std::string::npos) {
            std::filesystem::remove(entry.path(), ec);
          }
        }
      }
    };
    cleanupTmp(std::filesystem::temp_directory_path());
    if (const char* home = getenv("HOME")) {
      cleanupTmp(std::filesystem::path(home) / "Downloads");
    }
  });

  return app.exec();
}
