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
    Q_PROPERTY(int cardWidth READ cardWidth WRITE setCardWidth NOTIFY cardWidthChanged)
    Q_PROPERTY(int cardHeight READ cardHeight WRITE setCardHeight NOTIFY cardHeightChanged)
    Q_PROPERTY(bool isRepositioning READ isRepositioning NOTIFY isRepositioningChanged)
    Q_PROPERTY(bool isSnapping READ isSnapping NOTIFY isSnappingChanged)
    Q_PROPERTY(bool isSnappingToExpanded READ isSnappingToExpanded NOTIFY isSnappingChanged)
    Q_PROPERTY(bool isResizing READ isResizing NOTIFY isResizingChanged)
    Q_PROPERTY(int customCardWidth READ customCardWidth WRITE setCustomCardWidth NOTIFY customCardSizeChanged)
    Q_PROPERTY(int customCardHeight READ customCardHeight WRITE setCustomCardHeight NOTIFY customCardSizeChanged)
    Q_PROPERTY(int activeViewAutoHeight READ activeViewAutoHeight WRITE setActiveViewAutoHeight NOTIFY activeViewAutoHeightChanged)
    Q_PROPERTY(int handleNone READ handleNone CONSTANT)
    Q_PROPERTY(int handleLeft READ handleLeft CONSTANT)
    Q_PROPERTY(int handleRight READ handleRight CONSTANT)
    Q_PROPERTY(int handleBottom READ handleBottom CONSTANT)
    Q_PROPERTY(int handleBottomRight READ handleBottomRight CONSTANT)
    Q_PROPERTY(int handleBottomLeft READ handleBottomLeft CONSTANT)

public:
    enum ResizeHandle {
        HandleNone = 0,
        HandleLeft = 1,
        HandleRight = 2,
        HandleBottom = 4,
        HandleBottomRight = 6, // Right | Bottom
        HandleBottomLeft = 5   // Left | Bottom
    };
    Q_ENUM(ResizeHandle)

    explicit EdgeWindowManager(QObject* parent = nullptr);

    void setTargetWindow(QQuickWindow* window);
    void setSettingsManager(SettingsManager* sm);

    [[nodiscard]] int handleNone() const { return HandleNone; }
    [[nodiscard]] int handleLeft() const { return HandleLeft; }
    [[nodiscard]] int handleRight() const { return HandleRight; }
    [[nodiscard]] int handleBottom() const { return HandleBottom; }
    [[nodiscard]] int handleBottomRight() const { return HandleBottomRight; }
    [[nodiscard]] int handleBottomLeft() const { return HandleBottomLeft; }

    [[nodiscard]] bool isExpanded() const { return m_isExpanded; }
    Q_INVOKABLE void setIsExpanded(bool expanded);

    [[nodiscard]] QString screenEdge() const { return m_screenEdge; }
    Q_INVOKABLE void setScreenEdge(const QString& edge);

    [[nodiscard]] double edgeOffsetPercent() const { return m_edgeOffsetPercent; }
    Q_INVOKABLE void setEdgeOffsetPercent(double percent);

    [[nodiscard]] double edgeYRatio() const { return m_edgeOffsetPercent; }
    Q_INVOKABLE void setEdgeYRatio(double ratio) { setEdgeOffsetPercent(ratio); }

    [[nodiscard]] int cardWidth() const { return m_cardWidth; }
    Q_INVOKABLE void setCardWidth(int width);

    [[nodiscard]] int cardHeight() const { return m_cardHeight; }
    Q_INVOKABLE void setCardHeight(int height);

    [[nodiscard]] bool isRepositioning() const { return m_isRepositioning; }
    [[nodiscard]] bool isSnapping() const { return m_isSnapping; }
    [[nodiscard]] bool isSnappingToExpanded() const { return m_isSnapping && m_isExpanded; }
    [[nodiscard]] bool isResizing() const { return m_isResizing; }

    [[nodiscard]] int customCardWidth() const { return m_customWidth; }
    Q_INVOKABLE void setCustomCardWidth(int width);

    [[nodiscard]] int customCardHeight() const { return m_customHeight; }
    Q_INVOKABLE void setCustomCardHeight(int height);

    [[nodiscard]] int activeViewAutoHeight() const { return m_activeViewAutoHeight; }
    Q_INVOKABLE void setActiveViewAutoHeight(int height);

    [[nodiscard]] int windowX() const { return m_currentGeo.x(); }
    [[nodiscard]] int windowY() const { return m_currentGeo.y(); }
    [[nodiscard]] int windowWidth() const { return m_currentGeo.width(); }
    [[nodiscard]] int windowHeight() const { return m_currentGeo.height(); }

    Q_INVOKABLE void startRepositioning(int globalMouseX, int globalMouseY);
    Q_INVOKABLE void updateRepositioning(int globalMouseX, int globalMouseY);
    Q_INVOKABLE void endRepositioning(int globalMouseX = 0, int globalMouseY = 0);

    Q_INVOKABLE void beginDrag(int globalMouseX, int globalMouseY) { startRepositioning(globalMouseX, globalMouseY); }
    Q_INVOKABLE void endDrag(int globalMouseX = 0, int globalMouseY = 0) { endRepositioning(globalMouseX, globalMouseY); }

    Q_INVOKABLE void startCardResize(int handleMask);
    Q_INVOKABLE void updateCardResize();
    Q_INVOKABLE void endCardResize();
    Q_INVOKABLE void resetCustomSize();

    Q_INVOKABLE void recalculateGeometry();
    Q_INVOKABLE void updatePillDragOffset(int deltaY);
    Q_INVOKABLE void resizeCard(int deltaW, int deltaH);

signals:
    void isExpandedChanged(bool expanded);
    void screenEdgeChanged(const QString& edge);
    void edgeOffsetPercentChanged(double percent);
    void edgeYRatioChanged(double ratio);
    void cardWidthChanged(int width);
    void cardHeightChanged(int height);
    void isRepositioningChanged(bool repositioning);
    void isSnappingChanged(bool isSnapping);
    void isResizingChanged(bool isResizing);
    void customCardSizeChanged();
    void activeViewAutoHeightChanged(int height);
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
    bool m_isResizing = false;
    int m_activeResizeHandle = HandleNone;
    int m_customWidth = 0;
    int m_customHeight = 0;
    int m_activeViewAutoHeight = 246;
    QPoint m_resizeStartMouse{0, 0};
    int m_resizeStartWidth = 316;
    int m_resizeStartHeight = 246;
    QString m_screenEdge = "right";
    double m_edgeOffsetPercent = 0.40;
    int m_cardHeight = 246;

    QPoint m_dragStartMouse{0, 0};
    QPoint m_dragStartWindowPos{0, 0};

    static constexpr int kPillWidth = 14;
    static constexpr int kPillHeight = 68;
    static constexpr int kDefaultCardWidth = 316;
    int m_cardWidth = 316;

    QRect m_currentGeo{0, 0, kPillWidth, kPillHeight};
    QVariantAnimation* m_snapAnimation = nullptr;
};

} // namespace EdgeDrop
