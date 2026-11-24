#include "EdgeWindowManager.hpp"
#include "SettingsManager.hpp"
#include "../platform/PlatformHelper.hpp"
#include <QGuiApplication>
#include <QCursor>
#include <QDebug>
#include <algorithm>
#include <cmath>

namespace EdgeDrop {

EdgeWindowManager::EdgeWindowManager(QObject* parent)
    : QObject(parent) {
    auto connectScreen = [this](QScreen* s) {
        if (!s) return;
        connect(s, &QScreen::geometryChanged, this, &EdgeWindowManager::onScreenChanged, Qt::UniqueConnection);
        connect(s, &QScreen::availableGeometryChanged, this, &EdgeWindowManager::onScreenChanged, Qt::UniqueConnection);
        connect(s, &QScreen::logicalDotsPerInchChanged, this, &EdgeWindowManager::onScreenChanged, Qt::UniqueConnection);
    };

    for (QScreen* s : QGuiApplication::screens()) {
        connectScreen(s);
    }

    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, connectScreen](QScreen* s) {
        connectScreen(s);
        onScreenChanged();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &EdgeWindowManager::onScreenChanged);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &EdgeWindowManager::onScreenChanged);
}

void EdgeWindowManager::setTargetWindow(QQuickWindow* window) {
    if (m_window != window) {
        m_window = window;
        if (m_window) {
            connect(m_window, &QQuickWindow::screenChanged, this, [this](QScreen* s) {
                if (s) {
                    connect(s, &QScreen::geometryChanged, this, &EdgeWindowManager::onScreenChanged, Qt::UniqueConnection);
                    connect(s, &QScreen::availableGeometryChanged, this, &EdgeWindowManager::onScreenChanged, Qt::UniqueConnection);
                }
                onScreenChanged();
            });
            connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
                if (visible) {
                    recalculateGeometry();
                }
            });
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::setSettingsManager(SettingsManager* sm) {
    m_settingsManager = sm;
    if (m_settingsManager) {
        m_screenEdge = m_settingsManager->screenEdge().toLower();
        if (m_screenEdge != "left" && m_screenEdge != "right" && m_screenEdge != "top") m_screenEdge = "right";
        m_edgeOffsetPercent = m_settingsManager->edgeOffsetPercent();
        m_customWidth = m_settingsManager->customCardWidth();
        m_customHeight = m_settingsManager->customCardHeight();

        if (PlatformHelper::isWayland()) {
            // Under Wayland, client-side window positioning is restricted by the compositor.
            // Start expanded as a floating utility card so EdgeDrop is immediately accessible.
            m_isExpanded = true;
        }

        // Safe bounds clamp on launch against monitor geometry
        QScreen* s = activeScreen();
        if (s) {
            QRect av = s->availableGeometry();
            int maxSafeW = static_cast<int>(av.width() * 0.90);
            int maxSafeH = static_cast<int>(av.height() * 0.85);
            if (m_customWidth > maxSafeW) m_customWidth = maxSafeW;
            if (m_customHeight > maxSafeH) m_customHeight = maxSafeH;
        }

        recalculateGeometry();
    }
}

QScreen* EdgeWindowManager::activeScreen() const {
    // 1. When actively dragging/repositioning, follow the monitor under current cursor
    if (m_isRepositioning) {
        QPoint cursorPos = QCursor::pos();
        for (QScreen* s : QGuiApplication::screens()) {
            if (s && s->geometry().contains(cursorPos)) {
                return s;
            }
        }
    }

    // 2. If window has a valid screen currently attached and in screen list
    if (m_window && m_window->screen()) {
        if (QGuiApplication::screens().contains(m_window->screen())) {
            return m_window->screen();
        }
    }

    // 3. Fallback to monitor under cursor or primary monitor
    QPoint cursorPos = QCursor::pos();
    for (QScreen* s : QGuiApplication::screens()) {
        if (s && s->geometry().contains(cursorPos)) {
            return s;
        }
    }
    return QGuiApplication::primaryScreen();
}

void EdgeWindowManager::onScreenChanged() {
    recalculateGeometry();
}

void EdgeWindowManager::setIsExpanded(bool expanded) {
    if (m_isRepositioning || m_isSnapping) return;
    if (PlatformHelper::isWayland() && !expanded) {
        // Under Wayland, client-side positioning is restricted by compositors.
        // Keep the floating utility window accessible rather than collapsing into an invisible sliver.
        return;
    }
    if (m_isExpanded != expanded) {
        if (m_snapAnimation && m_snapAnimation->state() == QAbstractAnimation::Running) {
            m_snapAnimation->stop();
        }
        m_isExpanded = expanded;
        emit isExpandedChanged(expanded);
        recalculateGeometry();
        if (!expanded && m_window) {
            m_window->requestUpdate();
            PlatformHelper::flushWindowFrame(m_window);
        }
    }
}

void EdgeWindowManager::setScreenEdge(const QString& edge) {
    QString lower = edge.toLower();
    if (lower != "left" && lower != "right" && lower != "top") lower = "right";
    if (m_screenEdge != lower) {
        m_screenEdge = lower;
        if (m_settingsManager) {
            m_settingsManager->setScreenEdge(lower);
        }
        emit screenEdgeChanged(lower);
        recalculateGeometry();
    }
}

void EdgeWindowManager::setEdgeOffsetPercent(double percent) {
    percent = std::clamp(percent, 0.05, 0.95);
    if (std::abs(m_edgeOffsetPercent - percent) > 0.001) {
        m_edgeOffsetPercent = percent;
        if (m_settingsManager) {
            m_settingsManager->setEdgeOffsetPercent(percent);
        }
        emit edgeOffsetPercentChanged(percent);
        emit edgeYRatioChanged(percent);
        recalculateGeometry();
    }
}

void EdgeWindowManager::setCardWidth(int width) {
    int clamped = std::clamp(width, 280, 720);
    if (m_cardWidth != clamped) {
        m_cardWidth = clamped;
        m_customWidth = (clamped > kDefaultCardWidth) ? clamped : 0;
        if (m_settingsManager) {
            m_settingsManager->setCustomCardWidth(m_customWidth);
        }
        emit cardWidthChanged(clamped);
        emit customCardSizeChanged();
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::setCardHeight(int height) {
    int clamped = std::clamp(height, 180, 900);
    if (m_cardHeight != clamped) {
        m_cardHeight = clamped;
        emit cardHeightChanged(clamped);
        setActiveViewAutoHeight(clamped);
    }
}

void EdgeWindowManager::setActiveViewAutoHeight(int height) {
    int clamped = std::clamp(height, 180, 900);
    if (m_activeViewAutoHeight != clamped) {
        m_activeViewAutoHeight = clamped;
        emit activeViewAutoHeightChanged(clamped);
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::setCustomCardWidth(int width) {
    if (m_customWidth != width) {
        m_customWidth = width;
        if (m_settingsManager) {
            m_settingsManager->setCustomCardWidth(width);
        }
        emit customCardSizeChanged();
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::setCustomCardHeight(int height) {
    if (m_customHeight != height) {
        m_customHeight = height;
        if (m_settingsManager) {
            m_settingsManager->setCustomCardHeight(height);
        }
        emit customCardSizeChanged();
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::resetCustomSize() {
    m_customWidth = 0;
    m_customHeight = 0;
    if (m_settingsManager) {
        m_settingsManager->setCustomCardWidth(0);
        m_settingsManager->setCustomCardHeight(0);
    }
    emit customCardSizeChanged();
    recalculateGeometry();
}

void EdgeWindowManager::startCardResize(int handleMask) {
    if (!m_isExpanded) return;
    if (m_snapAnimation && m_snapAnimation->state() == QAbstractAnimation::Running) {
        m_snapAnimation->stop();
    }
    m_isResizing = true;
    m_activeResizeHandle = handleMask;
    m_resizeStartMouse = QCursor::pos();
    m_resizeStartWidth = m_currentGeo.width();
    m_resizeStartHeight = m_currentGeo.height();
    emit isResizingChanged(true);
}

void EdgeWindowManager::updateCardResize() {
    if (!m_isResizing) return;
    QScreen* screen = activeScreen();
    if (!screen) return;
    QRect avail = screen->availableGeometry();

    QPoint currentMouse = QCursor::pos();
    int deltaX = currentMouse.x() - m_resizeStartMouse.x();
    int deltaY = currentMouse.y() - m_resizeStartMouse.y();

    int newW = m_resizeStartWidth;
    int newH = m_resizeStartHeight;

    if (m_screenEdge == "right") {
        // When docked on the right screen edge, moving mouse left (deltaX < 0) expands the card into the screen
        if ((m_activeResizeHandle & HandleLeft) || (m_activeResizeHandle & HandleRight)) {
            newW = m_resizeStartWidth - deltaX;
        }
    } else if (m_screenEdge == "left") {
        // When docked on the left screen edge, moving mouse right (deltaX > 0) expands the card into the screen
        if ((m_activeResizeHandle & HandleLeft) || (m_activeResizeHandle & HandleRight)) {
            newW = m_resizeStartWidth + deltaX;
        }
    } else {
        if (m_activeResizeHandle & HandleRight) {
            newW = m_resizeStartWidth + deltaX;
        } else if (m_activeResizeHandle & HandleLeft) {
            newW = m_resizeStartWidth - deltaX;
        }
    }

    if (m_activeResizeHandle & HandleBottom) {
        newH = m_resizeStartHeight + deltaY;
    }

    int maxSafeW = static_cast<int>(avail.width() * 0.90);
    int maxSafeH = static_cast<int>(avail.height() * 0.85);

    int minSafeW = kDefaultCardWidth; // 316
    int minSafeH = m_activeViewAutoHeight > 0 ? m_activeViewAutoHeight : 246;

    newW = std::clamp(newW, minSafeW, maxSafeW);
    newH = std::clamp(newH, minSafeH, maxSafeH);

    m_customWidth = (newW > kDefaultCardWidth) ? newW : 0;
    m_customHeight = (newH > minSafeH) ? newH : 0;

    recalculateGeometry();
}

void EdgeWindowManager::endCardResize() {
    if (!m_isResizing) return;
    m_isResizing = false;
    m_activeResizeHandle = HandleNone;
    emit isResizingChanged(false);

    if (m_settingsManager) {
        m_settingsManager->setCustomCardWidth(m_customWidth);
        m_settingsManager->setCustomCardHeight(m_customHeight);
    }
    emit customCardSizeChanged();
}

void EdgeWindowManager::resizeCard(int deltaW, int deltaH) {
    if (!m_isExpanded) return;
    QScreen* s = activeScreen();
    QRect av = s ? s->availableGeometry() : QRect(0, 0, 1920, 1080);
    int maxSafeW = static_cast<int>(av.width() * 0.90);
    int maxSafeH = static_cast<int>(av.height() * 0.85);

    int autoH = m_activeViewAutoHeight > 0 ? m_activeViewAutoHeight : 246;
    int curW = (m_customWidth > kDefaultCardWidth) ? m_customWidth : kDefaultCardWidth;
    int curH = (m_customHeight > autoH) ? m_customHeight : autoH;

    int newW = std::clamp(curW + deltaW, kDefaultCardWidth, maxSafeW);
    int newH = std::clamp(curH + deltaH, autoH, maxSafeH);

    m_customWidth = (newW > kDefaultCardWidth) ? newW : 0;
    m_customHeight = (newH > autoH) ? newH : 0;

    if (m_settingsManager) {
        m_settingsManager->setCustomCardWidth(m_customWidth);
        m_settingsManager->setCustomCardHeight(m_customHeight);
    }
    emit customCardSizeChanged();
    recalculateGeometry();
}

void EdgeWindowManager::startRepositioning(int globalMouseX, int globalMouseY) {
    if (m_snapAnimation && m_snapAnimation->state() == QAbstractAnimation::Running) {
        m_snapAnimation->stop();
    }
    if (m_isSnapping) {
        m_isSnapping = false;
        emit isSnappingChanged(false);
    }
    m_isRepositioning = true;
    m_dragStartMouse = QPoint(globalMouseX, globalMouseY);
    m_dragStartWindowPos = m_currentGeo.topLeft();
    emit isRepositioningChanged(true);
}

void EdgeWindowManager::updateRepositioning(int globalMouseX, int globalMouseY) {
    if (!m_isRepositioning) return;

    QScreen* screen = activeScreen();
    if (m_window && screen && m_window->screen() != screen) {
        m_window->setScreen(screen);
    }

    QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int deltaX = globalMouseX - m_dragStartMouse.x();
    int deltaY = globalMouseY - m_dragStartMouse.y();

    int newX = m_dragStartWindowPos.x() + deltaX;
    int newY = m_dragStartWindowPos.y() + deltaY;

    // Clamp Y so the window stays strictly within available screen area
    int targetH = m_currentGeo.height();
    newY = std::clamp(newY, avail.top(), avail.bottom() - targetH + 1);

    m_currentGeo.moveTo(newX, newY);
    applyWindowGeometry();
    emit geometryChanged();
}

void EdgeWindowManager::endRepositioning(int /*globalMouseX*/, int /*globalMouseY*/) {
    if (!m_isRepositioning) return;
    m_isRepositioning = false;
    emit isRepositioningChanged(false);

    if (m_snapAnimation && m_snapAnimation->state() == QAbstractAnimation::Running) {
        m_snapAnimation->stop();
    }

    QScreen* screen = activeScreen();
    if (!screen) {
        recalculateGeometry();
        return;
    }

    QRect avail = screen->availableGeometry();

    // Compare window horizontal center to screen horizontal center
    int winCenterX = m_currentGeo.x() + (m_currentGeo.width() / 2);
    int screenMidX = avail.left() + (avail.width() / 2);
    QString newEdge = (winCenterX < screenMidX) ? "left" : "right";

    // Capture the exact fixed (w, h) for current state before starting snap
    int maxSafeW = static_cast<int>(avail.width() * 0.90);
    int maxSafeH = static_cast<int>(avail.height() * 0.85);
    int autoH = m_activeViewAutoHeight > 0 ? m_activeViewAutoHeight : m_cardHeight;
    if (autoH <= 0) autoH = 246;

    int effectiveW = kDefaultCardWidth;
    if (m_customWidth > kDefaultCardWidth) {
        effectiveW = std::clamp(m_customWidth, kDefaultCardWidth, maxSafeW);
    }
    int effectiveH = autoH;
    if (m_customHeight > effectiveH) {
        effectiveH = std::clamp(m_customHeight, autoH, maxSafeH);
    }

    int exactW = m_isExpanded ? effectiveW : kPillWidth;
    int exactH = m_isExpanded ? effectiveH : kPillHeight;

    int targetX = (newEdge == "left") ? avail.left() : (avail.right() - exactW + 1);
    int maxTravelY = avail.height() - exactH;
    int targetY = std::clamp(m_currentGeo.y(), avail.top(), avail.bottom() - exactH + 1);

    double newRatio = 0.40;
    if (maxTravelY > 0) {
        newRatio = static_cast<double>(targetY - avail.top()) / maxTravelY;
    }
    newRatio = std::clamp(newRatio, 0.05, 0.95);

    m_screenEdge = newEdge;
    m_edgeOffsetPercent = newRatio;

    if (m_settingsManager) {
        m_settingsManager->setScreenEdge(newEdge);
        m_settingsManager->setEdgeOffsetPercent(newRatio);
    }

    emit screenEdgeChanged(newEdge);
    emit edgeOffsetPercentChanged(newRatio);
    emit edgeYRatioChanged(newRatio);

    // Lock the window to that exact (w, h) first
    m_currentGeo.setSize(QSize(exactW, exactH));
    applyWindowGeometry();

    QPoint startPos = m_currentGeo.topLeft();
    QPoint targetPos(targetX, targetY);

    if (startPos == targetPos) {
        m_currentGeo = QRect(targetX, targetY, exactW, exactH);
        applyWindowGeometry();
        emit geometryChanged();
        if (m_window) {
            m_window->requestUpdate();
            PlatformHelper::flushWindowFrame(m_window);
        }
        return;
    }

    m_isSnapping = true;
    emit isSnappingChanged(true);

    if (!m_snapAnimation) {
        m_snapAnimation = new QVariantAnimation(this);
        connect(m_snapAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            QPoint pos = value.toPoint();
            m_currentGeo.moveTo(pos.x(), pos.y());
            if (m_window) {
                m_window->setPosition(pos.x(), pos.y());
            }
            emit geometryChanged();
        });
        connect(m_snapAnimation, &QVariantAnimation::finished, this, [this]() {
            m_isSnapping = false;
            emit isSnappingChanged(false);
            recalculateGeometry();
        });
    }

    m_snapAnimation->stop();
    m_snapAnimation->setDuration(220);
    m_snapAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_snapAnimation->setStartValue(startPos);
    m_snapAnimation->setEndValue(targetPos);
    m_snapAnimation->start();
}

void EdgeWindowManager::updatePillDragOffset(int deltaY) {
    QScreen* screen = activeScreen();
    if (!screen) return;

    QRect avail = screen->availableGeometry();
    if (avail.height() <= 0) return;

    double newPercent = m_edgeOffsetPercent + static_cast<double>(deltaY) / avail.height();
    setEdgeOffsetPercent(newPercent);
}

void EdgeWindowManager::recalculateGeometry() {
    if (m_snapAnimation && m_snapAnimation->state() == QAbstractAnimation::Running) {
        m_snapAnimation->stop();
    }
    if (m_isSnapping) {
        m_isSnapping = false;
        emit isSnappingChanged(false);
    }

    QScreen* screen = activeScreen();
    if (!screen) return;

    QRect avail = screen->availableGeometry();
    int maxSafeW = static_cast<int>(avail.width() * 0.90);
    int maxSafeH = static_cast<int>(avail.height() * 0.85);

    int autoH = m_activeViewAutoHeight > 0 ? m_activeViewAutoHeight : m_cardHeight;
    if (autoH <= 0) autoH = 246;

    int effectiveW = kDefaultCardWidth;
    if (m_customWidth > kDefaultCardWidth) {
        effectiveW = std::clamp(m_customWidth, kDefaultCardWidth, maxSafeW);
    }

    int effectiveH = autoH;
    if (m_customHeight > effectiveH) {
        effectiveH = std::clamp(m_customHeight, autoH, maxSafeH);
    }

    int targetW = m_isExpanded ? effectiveW : kPillWidth;
    int targetH = m_isExpanded ? effectiveH : kPillHeight;

    int posX = 0;
    int posY = 0;

    if (m_screenEdge.toLower() == "top") {
        posX = avail.left() + (avail.width() - targetW) / 2;
        posY = avail.top();
    } else if (m_screenEdge.toLower() == "left") {
        posX = avail.left();
        int maxTravelY = avail.height() - targetH;
        posY = avail.top() + static_cast<int>(m_edgeOffsetPercent * maxTravelY);
        posY = std::clamp(posY, avail.top(), avail.bottom() - targetH + 1);
    } else {
        posX = avail.right() - targetW + 1;
        int maxTravelY = avail.height() - targetH;
        posY = avail.top() + static_cast<int>(m_edgeOffsetPercent * maxTravelY);
        posY = std::clamp(posY, avail.top(), avail.bottom() - targetH + 1);
    }

    m_currentGeo = QRect(posX, posY, targetW, targetH);
    applyWindowGeometry();
    emit geometryChanged();

    if (!m_isExpanded && m_window) {
        m_window->requestUpdate();
        PlatformHelper::flushWindowFrame(m_window);
    }
}

void EdgeWindowManager::applyWindowGeometry() {
    if (!m_window) return;
    PlatformHelper::lockWindowSize(m_window, m_currentGeo.width(), m_currentGeo.height());
    m_window->setGeometry(m_currentGeo);
    if (!m_isExpanded) {
        m_window->requestUpdate();
        PlatformHelper::flushWindowFrame(m_window);
    }
}

} // namespace EdgeDrop
