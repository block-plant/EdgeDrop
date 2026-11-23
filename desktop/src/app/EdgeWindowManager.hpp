#pragma once

#include <QObject>
#include <QScreen>
#include <QRect>
#include <QPoint>
#include <QQuickWindow>
#include <QVariantAnimation>
#include <QEasingCurve>

namespace EdgeDrop {

class SettingsManager;

class EdgeWindowManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isExpanded READ isExpanded WRITE setIsExpanded NOTIFY isExpandedChanged)
    Q_PROPERTY(QString screenEdge READ screenEdge WRITE setScreenEdge NOTIFY screenEdgeChanged)
    Q_PROPERTY(double edgeOffsetPercent READ edgeOffsetPercent WRITE setEdgeOffsetPercent NOTIFY edgeOffsetPercentChanged)
    Q_PROPERTY(double edgeYRatio READ edgeYRatio WRITE setEdgeYRatio NOTIFY edgeYRatioChanged)
    Q_PROPERTY(int windowX READ windowX NOTIFY geometryChanged)
    Q_PROPERTY(int windowY READ windowY NOTIFY geometryChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY geometryChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY geometryChanged)
    Q_PROPERTY(int cardHeight READ cardHeight WRITE setCardHeight NOTIFY cardHeightChanged)
    Q_PROPERTY(bool isRepositioning READ isRepositioning NOTIFY isRepositioningChanged)
    Q_PROPERTY(bool isSnapping READ isSnapping NOTIFY isSnappingChanged)
    Q_PROPERTY(bool isSnappingToExpanded READ isSnappingToExpanded NOTIFY isSnappingChanged)

public:
    explicit EdgeWindowManager(QObject* parent = nullptr);

    void setTargetWindow(QQuickWindow* window);
    void setSettingsManager(SettingsManager* sm);

    [[nodiscard]] bool isExpanded() const { return m_isExpanded; }
    void setIsExpanded(bool expanded);

    [[nodiscard]] QString screenEdge() const { return m_screenEdge; }
    void setScreenEdge(const QString& edge);

    [[nodiscard]] double edgeOffsetPercent() const { return m_edgeOffsetPercent; }
    void setEdgeOffsetPercent(double percent);

    [[nodiscard]] double edgeYRatio() const { return m_edgeOffsetPercent; }
    void setEdgeYRatio(double ratio) { setEdgeOffsetPercent(ratio); }

    [[nodiscard]] int cardHeight() const { return m_cardHeight; }
    void setCardHeight(int height);

    [[nodiscard]] bool isRepositioning() const { return m_isRepositioning; }
    [[nodiscard]] bool isSnapping() const { return m_isSnapping; }
    [[nodiscard]] bool isSnappingToExpanded() const { return m_isSnapping && m_isExpanded; }

    [[nodiscard]] int windowX() const { return m_currentGeo.x(); }
    [[nodiscard]] int windowY() const { return m_currentGeo.y(); }
    [[nodiscard]] int windowWidth() const { return m_currentGeo.width(); }
    [[nodiscard]] int windowHeight() const { return m_currentGeo.height(); }

    Q_INVOKABLE void startRepositioning(int globalMouseX, int globalMouseY);
    Q_INVOKABLE void updateRepositioning(int globalMouseX, int globalMouseY);
    Q_INVOKABLE void endRepositioning(int globalMouseX = 0, int globalMouseY = 0);

    Q_INVOKABLE void beginDrag(int globalMouseX, int globalMouseY) { startRepositioning(globalMouseX, globalMouseY); }
    Q_INVOKABLE void endDrag(int globalMouseX = 0, int globalMouseY = 0) { endRepositioning(globalMouseX, globalMouseY); }

    Q_INVOKABLE void recalculateGeometry();
    Q_INVOKABLE void updatePillDragOffset(int deltaY);

signals:
    void isExpandedChanged(bool expanded);
    void screenEdgeChanged(const QString& edge);
    void edgeOffsetPercentChanged(double percent);
    void edgeYRatioChanged(double ratio);
    void cardHeightChanged(int height);
    void isRepositioningChanged(bool repositioning);
    void isSnappingChanged(bool isSnapping);
    void geometryChanged();

private slots:
    void onScreenChanged();

private:
    QScreen* activeScreen() const;
    void applyWindowGeometry();

    QQuickWindow* m_window = nullptr;
    SettingsManager* m_settingsManager = nullptr;
    bool m_isExpanded = false;
    bool m_isRepositioning = false;
    bool m_isSnapping = false;
    QString m_screenEdge = "right";
    double m_edgeOffsetPercent = 0.40;
    int m_cardHeight = 232;

    QPoint m_dragStartMouse{0, 0};
    QPoint m_dragStartWindowPos{0, 0};

    static constexpr int kPillWidth = 14;
    static constexpr int kPillHeight = 68;
    static constexpr int kCardWidth = 316;

    QRect m_currentGeo{0, 0, kPillWidth, kPillHeight};
    QVariantAnimation* m_snapAnimation = nullptr;
};

} // namespace EdgeDrop
