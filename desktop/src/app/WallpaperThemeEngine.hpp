#pragma once

#include <QObject>
#include <QColor>
#include <QImage>
#include <QString>

namespace EdgeDrop {

class SettingsManager;

class WallpaperThemeEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor cardSurface READ cardSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor elevatedSurface READ elevatedSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor pillSurface READ pillSurface NOTIFY colorsChanged)
    Q_PROPERTY(QColor hairlineBorder READ hairlineBorder NOTIFY colorsChanged)
    Q_PROPERTY(QColor primaryText READ primaryText NOTIFY colorsChanged)
    Q_PROPERTY(QColor secondaryText READ secondaryText NOTIFY colorsChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY colorsChanged)
    Q_PROPERTY(bool isDark READ isDark NOTIFY colorsChanged)
    Q_PROPERTY(bool isLightWallpaper READ isLightWallpaper NOTIFY colorsChanged)
    Q_PROPERTY(double dominantHue READ dominantHue NOTIFY colorsChanged)

public:
    explicit WallpaperThemeEngine(SettingsManager* settings = nullptr, QObject* parent = nullptr);

    [[nodiscard]] QColor cardSurface() const { return m_cardSurface; }
    [[nodiscard]] QColor elevatedSurface() const { return m_elevatedSurface; }
    [[nodiscard]] QColor pillSurface() const { return m_pillSurface; }
    [[nodiscard]] QColor hairlineBorder() const { return m_hairlineBorder; }
    [[nodiscard]] QColor primaryText() const { return m_primaryText; }
    [[nodiscard]] QColor secondaryText() const { return m_secondaryText; }
    [[nodiscard]] QColor accent() const { return m_accent; }
    [[nodiscard]] bool isDark() const { return m_isDark; }
    [[nodiscard]] bool isLightWallpaper() const { return !m_isDark; }
    [[nodiscard]] double dominantHue() const { return m_dominantHue; }

    Q_INVOKABLE void refreshWallpaper();
    void updateForEdge(const QString& edge);

signals:
    void colorsChanged();

private:
    void computeAdaptiveColors(const QImage& thumbnail, const QString& edge);
    void applyFallbackColors();

    SettingsManager* m_settings = nullptr;
    QString m_lastEdge = "right";

    QColor m_cardSurface{"#161618"};
    QColor m_elevatedSurface{"#202024"};
    QColor m_pillSurface{"#1C1C20"};
    QColor m_hairlineBorder{255, 255, 255, 20};
    QColor m_primaryText{"#F5F5F7"};
    QColor m_secondaryText{"#A1A1A6"};
    QColor m_accent{"#3B82F6"};
    bool m_isDark = true;
    double m_dominantHue = 210.0;
};

} // namespace EdgeDrop
