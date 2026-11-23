#include "WallpaperThemeEngine.hpp"
#include "SettingsManager.hpp"
#include "../platform/PlatformHelper.hpp"
#include <cmath>
#include <algorithm>
#include <QGuiApplication>
#include <QStyleHints>

namespace EdgeDrop {

WallpaperThemeEngine::WallpaperThemeEngine(SettingsManager* settings, QObject* parent)
    : QObject(parent),
      m_settings(settings) {
    if (m_settings) {
        m_lastEdge = m_settings->screenEdge().toLower();
        connect(m_settings, &SettingsManager::screenEdgeChanged, this, [this](const QString& edge) {
            updateForEdge(edge);
        });
        connect(m_settings, &SettingsManager::vibeChanged, this, [this](const QString& vibe) {
            if (vibe == "adaptive") {
                refreshWallpaper();
            }
        });
    }
    refreshWallpaper();
}

void WallpaperThemeEngine::updateForEdge(const QString& edge) {
    QString lower = edge.toLower();
    if (lower != "left" && lower != "right") lower = "right";
    if (m_lastEdge != lower) {
        m_lastEdge = lower;
        refreshWallpaper();
    }
}

void WallpaperThemeEngine::refreshWallpaper() {
    QImage thumb = PlatformHelper::getWallpaperEdgeThumbnail();
    computeAdaptiveColors(thumb, m_lastEdge);
}

void WallpaperThemeEngine::computeAdaptiveColors(const QImage& thumbnail, const QString& edge) {
    if (thumbnail.isNull() || thumbnail.width() < 16 || thumbnail.height() < 16) {
        applyFallbackColors();
        return;
    }

    // Sample the actual screen edge strip (left 25% or right 25% matching screenEdge)
    int startX = (edge == "left") ? 0 : static_cast<int>(thumbnail.width() * 0.75);
    int endX   = (edge == "left") ? static_cast<int>(thumbnail.width() * 0.25) : thumbnail.width();

    double sinSum = 0.0;
    double cosSum = 0.0;
    double satSum = 0.0;
    int satCount = 0;
    double lumSum = 0.0;
    int totalCount = 0;

    for (int y = 0; y < thumbnail.height(); ++y) {
        for (int x = startX; x < endX; ++x) {
            QColor c = thumbnail.pixelColor(x, y);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);

            // Perceived luminance (0.2126*R + 0.7152*G + 0.0722*B)
            double lum = 0.2126 * c.redF() + 0.7152 * c.greenF() + 0.0722 * c.blueF();
            lumSum += lum;
            totalCount++;

            // Ignore near-grey pixels when finding dominant Hue
            if (s > 0.12f && l > 0.08f && l < 0.92f) {
                double rad = h * 2.0 * M_PI;
                sinSum += std::sin(rad) * s;
                cosSum += std::cos(rad) * s;
                satSum += s;
                satCount++;
            }
        }
    }

    if (totalCount == 0) {
        applyFallbackColors();
        return;
    }

    double avgLum = lumSum / totalCount;
    double avgSat = (satCount > 0) ? (satSum / satCount) : 0.20;
    double dominantH = 210.0; // Clean blue fallback

    if (satCount > 0 && (std::abs(sinSum) > 0.001 || std::abs(cosSum) > 0.001)) {
        double angle = std::atan2(sinSum, cosSum);
        if (angle < 0.0) angle += 2.0 * M_PI;
        dominantH = angle * 180.0 / M_PI;
    }
    m_dominantHue = dominantH;

    // Follow the wallpaper's actual brightness: luminance >= 0.52 is Bright Wallpaper
    bool isBright = (avgLum >= 0.52);
    m_isDark = !isBright;

    float hueF = static_cast<float>(dominantH / 360.0);

    if (isBright) {
        // Bright Wallpaper (luminance >= 0.52):
        // cardSurface: Hue H, Saturation clamped 26%–42%, Lightness 91% (alpha: 0.94)
        float sCard = static_cast<float>(std::clamp(avgSat, 0.26, 0.42));
        m_cardSurface = QColor::fromHslF(hueF, sCard, 0.91f, 0.94f);

        // surfaceElevated: Hue H, Saturation 22%–36%, Lightness 85%
        float sElev = static_cast<float>(std::clamp(avgSat * 0.85, 0.22, 0.36));
        m_elevatedSurface = QColor::fromHslF(hueF, sElev, 0.85f, 1.0f);

        // pillSurface: Hue H, Saturation 24%–38%, Lightness 88%
        float sPill = static_cast<float>(std::clamp(avgSat * 0.90, 0.24, 0.38));
        m_pillSurface = QColor::fromHslF(hueF, sPill, 0.88f, 1.0f);

        // textPrimary: #14181F, textSecondary: #4B5563
        m_primaryText = QColor("#14181F");
        m_secondaryText = QColor("#4B5563");

        // accent: Hue H, Saturation 72%, Lightness 42% (guaranteed crisp contrast)
        m_accent = QColor::fromHslF(hueF, 0.72f, 0.42f, 1.0f);

        // borderHairline: rgba(0, 0, 0, 0.08)
        m_hairlineBorder = QColor(0, 0, 0, 20);
    } else {
        // Dark Wallpaper (luminance < 0.52):
        // cardSurface: Hue H, Saturation clamped 28%–45%, Lightness 15% (alpha: 0.94)
        float sCard = static_cast<float>(std::clamp(avgSat, 0.28, 0.45));
        m_cardSurface = QColor::fromHslF(hueF, sCard, 0.15f, 0.94f);

        // surfaceElevated: Hue H, Saturation 24%–38%, Lightness 21%
        float sElev = static_cast<float>(std::clamp(avgSat * 0.85, 0.24, 0.38));
        m_elevatedSurface = QColor::fromHslF(hueF, sElev, 0.21f, 1.0f);

        // pillSurface: Hue H, Saturation 26%–40%, Lightness 18%
        float sPill = static_cast<float>(std::clamp(avgSat * 0.90, 0.26, 0.40));
        m_pillSurface = QColor::fromHslF(hueF, sPill, 0.18f, 1.0f);

        // textPrimary: #F5F5F7, textSecondary: #A1A8B3
        m_primaryText = QColor("#F5F5F7");
        m_secondaryText = QColor("#A1A8B3");

        // accent: Hue H, Saturation 75%, Lightness 60%
        m_accent = QColor::fromHslF(hueF, 0.75f, 0.60f, 1.0f);

        // borderHairline: rgba(255, 255, 255, 0.12)
        m_hairlineBorder = QColor(255, 255, 255, 30);
    }

    emit colorsChanged();
}

void WallpaperThemeEngine::applyFallbackColors() {
    bool isDark = true;
    if (m_settings) {
        if (m_settings->themeMode() == "Light") isDark = false;
    } else if (QGuiApplication::styleHints()) {
        isDark = (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    }
    m_isDark = isDark;

    if (isDark) {
        m_cardSurface = QColor(22, 22, 24, 240);
        m_elevatedSurface = QColor("#202024");
        m_pillSurface = QColor("#1C1C20");
        m_hairlineBorder = QColor(255, 255, 255, 30);
        m_primaryText = QColor("#F5F5F7");
        m_secondaryText = QColor("#A1A8B3");
        m_accent = QColor("#3B82F6");
    } else {
        m_cardSurface = QColor(240, 244, 248, 240);
        m_elevatedSurface = QColor("#E2E8F0");
        m_pillSurface = QColor("#EDF2F7");
        m_hairlineBorder = QColor(0, 0, 0, 20);
        m_primaryText = QColor("#14181F");
        m_secondaryText = QColor("#4B5563");
        m_accent = QColor("#2563EB");
    }
    emit colorsChanged();
}

} // namespace EdgeDrop
