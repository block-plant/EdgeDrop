pragma Singleton
import QtQuick

QtObject {
    id: root

    // Typography token
    readonly property string monoFont: Qt.platform.os === "osx" ? "Menlo" : (Qt.platform.os === "windows" ? "Consolas" : "monospace")

    // Active vibe token from SettingsManager ("adaptive", "obsidian", "porcelain", "mist")
    readonly property string activeVibe: (typeof settingsManager !== "undefined" && settingsManager && settingsManager.vibe) ? settingsManager.vibe : "adaptive"

    // Derive isLightSurface strictly
    readonly property bool isLightSurface: {
        if (activeVibe === "porcelain") return true;
        if (activeVibe === "obsidian" || activeVibe === "mist") return false;
        if (typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) {
            return wallpaperThemeEngine.isLightWallpaper;
        }
        return false;
    }

    readonly property bool isDarkMode: !isLightSurface

    // Contrast-Locked Text Tokens
    readonly property color textPrimary: isLightSurface ? "#111318" : "#F5F7FA"
    readonly property color textSecondary: isLightSurface ? "#4A5160" : "#A6AEBC"
    readonly property color textMuted: isLightSurface ? "#6B7280" : "#7A8291"
    readonly property color textOnAccent: "#FFFFFF"

    // Compatibility aliases
    readonly property color primaryText: textPrimary
    readonly property color secondaryText: textSecondary
    readonly property color tertiaryText: textMuted

    // Surface & Border Tokens
    readonly property color surfaceElevated: {
        if (isLightSurface) {
            return (activeVibe === "porcelain") ? "#EEEDE8" : ((typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.elevatedSurface : "#EEEDE8");
        } else {
            if (activeVibe === "obsidian") return "#22242A";
            if (activeVibe === "mist") return "rgba(255, 255, 255, 0.15)";
            return (typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.elevatedSurface : "#22242A";
        }
    }
    readonly property color elevatedSurface: surfaceElevated

    // Vibe-Matched Hover Tokens
    readonly property color surfaceHover: isLightSurface ? Qt.rgba(0, 0, 0, 0.06) : Qt.rgba(1, 1, 1, 0.08)
    readonly property color elevatedHover: {
        if (isLightSurface) {
            return Qt.darker(surfaceElevated, 1.05);
        } else {
            if (activeVibe === "mist") return Qt.rgba(1, 1, 1, 0.22);
            return Qt.lighter(surfaceElevated, 1.12);
        }
    }
    readonly property color borderHairline: isLightSurface ? "#1A000000" : "#1FFFFFFF"
    readonly property color hairlineBorder: borderHairline
    readonly property color activeBorder: isLightSurface ? "#2E000000" : "#33FFFFFF"

    readonly property color pillFill: isLightSurface ? "#FFFFFF" : "#2E313B"

    readonly property color cardSurface: {
        if (isLightSurface) {
            return (activeVibe === "porcelain") ? "#FAF9F6" : ((typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.cardSurface : "#FAF9F6");
        } else {
            if (activeVibe === "obsidian") return "#161618";
            if (activeVibe === "mist") return "rgba(22, 24, 30, 0.91)";
            return (typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.cardSurface : "#161618";
        }
    }

    readonly property color pillSurface: {
        if (isLightSurface) {
            return (activeVibe === "porcelain") ? "#F5F4EF" : ((typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.pillSurface : "#F5F4EF");
        } else {
            if (activeVibe === "obsidian") return "#1C1C20";
            if (activeVibe === "mist") return "rgba(30, 32, 40, 0.91)";
            return (typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.pillSurface : "#1C1C20";
        }
    }

    // Accent & Status Tokens
    readonly property color accent: {
        if (isLightSurface) {
            return (activeVibe === "porcelain") ? "#18181B" : ((typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.accent : "#18181B");
        } else {
            if (activeVibe === "obsidian") return "#3B82F6";
            if (activeVibe === "mist") return "#60A5FA";
            return (typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine) ? wallpaperThemeEngine.accent : "#3B82F6";
        }
    }

    readonly property color accentHover: isLightSurface ? Qt.darker(accent, 1.08) : Qt.lighter(accent, 1.10)
    readonly property color accentSurface: isLightSurface ? "#0F000000" : "#1AFFFFFF"
    readonly property color accentBorder: accent

    readonly property color success: "#10B981"
    readonly property color successSurface: isLightSurface ? "#E6F4EA" : "#0D3320"
    readonly property color warning: "#F59E0B"
    readonly property color danger: "#EF4444"
    readonly property color error: danger
    readonly property color errorSurface: isLightSurface ? "#FEE2E2" : "#3B1818"

    // Spacing (Airbnb/Apple 8pt rhythm)
    readonly property int spaceXs: 4
    readonly property int spaceSm: 8
    readonly property int spaceMd: 12
    readonly property int spaceLg: 14
    readonly property int spaceXl: 20

    // Concentric Geometry
    readonly property int radiusOuter: 20
    readonly property int radiusInner: 12
    readonly property int radiusSmall: 8
    readonly property int radiusPill: 999

    // Animation Timings (Hardware accelerated)
    readonly property int animExpandMs: 240
    readonly property int animCollapseMs: 190
    readonly property int animSnapMs: 260
    readonly property int animColorMs: 220
    readonly property int animHoverMs: 120
    readonly property int animPulseMs: 120
    readonly property int animTabSwitchMs: 180
    readonly property int animMorphMs: 200
}
