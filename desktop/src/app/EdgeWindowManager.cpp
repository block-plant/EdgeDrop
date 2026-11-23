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
    connect(qGuiApp, &QGuiApplication::screenAdded, this, &EdgeWindowManager::onScreenChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &EdgeWindowManager::onScreenChanged);
}

void EdgeWindowManager::setTargetWindow(QQuickWindow* window) {
    if (m_window != window) {
        m_window = window;
        if (m_window) {
            connect(m_window, &QQuickWindow::screenChanged, this, &EdgeWindowManager::onScreenChanged);
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
        if (m_screenEdge != "left" && m_screenEdge != "right") m_screenEdge = "right";
        m_edgeOffsetPercent = m_settingsManager->edgeOffsetPercent();
        recalculateGeometry();
    }
}

QScreen* EdgeWindowManager::activeScreen() const {
    if (m_window && m_window->screen()) {
        return m_window->screen();
    }
    // Determine screen containing current cursor position
    QPoint cursorPos = QCursor::pos();
    for (QScreen* s : QGuiApplication::screens()) {
        if (s->geometry().contains(cursorPos)) {
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
    if (lower != "left" && lower != "right") lower = "right";
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
    int clamped = std::clamp(width, 280, 520);
    if (m_cardWidth != clamped) {
        m_cardWidth = clamped;
        emit cardWidthChanged(clamped);
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::setCardHeight(int height) {
    int clamped = std::clamp(height, 180, 700);
    if (m_cardHeight != clamped) {
        m_cardHeight = clamped;
        emit cardHeightChanged(clamped);
        if (m_isExpanded) {
            recalculateGeometry();
        }
    }
}

void EdgeWindowManager::resizeCard(int deltaW, int deltaH) {
    if (!m_isExpanded) return;
    int newW = std::clamp(m_cardWidth + deltaW, 280, 520);
    int newH = std::clamp(m_cardHeight + deltaH, 180, 700);
    bool changed = false;
    if (newW != m_cardWidth) {
        m_cardWidth = newW;
        emit cardWidthChanged(m_cardWidth);
        changed = true;
    }
    if (newH != m_cardHeight) {
        m_cardHeight = newH;
        emit cardHeightChanged(m_cardHeight);
        changed = true;
    }
    if (changed) {
        recalculateGeometry();
    }
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
    int exactW = m_isExpanded ? m_cardWidth : kPillWidth;
    int exactH = m_isExpanded ? std::min(m_cardHeight, static_cast<int>(avail.height() * 0.85)) : kPillHeight;

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

            QScreen* s = activeScreen();
            QRect av = s ? s->availableGeometry() : QRect(0, 0, 1920, 1080);
            int ew = m_isExpanded ? m_cardWidth : kPillWidth;
            int eh = m_isExpanded ? std::min(m_cardHeight, static_cast<int>(av.height() * 0.85)) : kPillHeight;
            int tx = (m_screenEdge == "left") ? av.left() : (av.right() - ew + 1);
            int mty = av.height() - eh;
            int ty = av.top() + static_cast<int>(m_edgeOffsetPercent * mty);
            ty = std::clamp(ty, av.top(), av.bottom() - eh + 1);

            m_currentGeo = QRect(tx, ty, ew, eh);
            applyWindowGeometry();
            emit geometryChanged();

            if (m_window) {
                m_window->requestUpdate();
                PlatformHelper::flushWindowFrame(m_window);
            }
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
    int targetW = m_isExpanded ? m_cardWidth : kPillWidth;
    int targetH = m_isExpanded ? std::min(m_cardHeight, static_cast<int>(avail.height() * 0.85)) : kPillHeight;

    int posX = (m_screenEdge.toLower() == "left") ? avail.left() : (avail.right() - targetW + 1);
    int maxTravelY = avail.height() - targetH;
    int posY = avail.top() + static_cast<int>(m_edgeOffsetPercent * maxTravelY);
    posY = std::clamp(posY, avail.top(), avail.bottom() - targetH + 1);

    m_currentGeo = QRect(posX, posY, targetW, targetH);
    qInfo() << "EdgeWindowManager: recalculated geometry:" << m_currentGeo << "isExpanded:" << m_isExpanded << "edge:" << m_screenEdge;
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
