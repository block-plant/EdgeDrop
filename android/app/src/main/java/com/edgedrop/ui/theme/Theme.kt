package com.edgedrop.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

data class EdgeDropThemeTokens(
    val isLight: Boolean,
    val surface: Color,
    val surfaceElevated: Color,
    val pillSurface: Color,
    val borderHairline: Color,
    val activeBorder: Color,
    val textPrimary: Color,
    val textSecondary: Color,
    val textMuted: Color,
    val accent: Color,
    val accentSurface: Color,
    val success: Color,
    val successSurface: Color,
    val error: Color,
    val errorSurface: Color,
    val gripColor: Color,
    val cardRadius: Dp = 16.dp,
    val chipRadius: Dp = 8.dp,
    val buttonRadius: Dp = 8.dp,
    val pillRadius: Dp = 4.dp
)

val LocalEdgeDropTheme = staticCompositionLocalOf<EdgeDropThemeTokens> {
    error("No EdgeDropThemeTokens provided")
}

object EdgeDropTheme {
    val tokens: EdgeDropThemeTokens
        @Composable
        @ReadOnlyComposable
        get() = LocalEdgeDropTheme.current
}

@Composable
fun EdgeDropTheme(
    vibe: Vibe = Vibe.Adaptive,
    darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val isLight = when (vibe) {
        Vibe.Porcelain -> true
        Vibe.Obsidian, Vibe.Mist -> false
        Vibe.Adaptive -> !darkTheme
    }

    val tokens = when (vibe) {
        Vibe.Porcelain -> EdgeDropThemeTokens(
            isLight = true,
            surface = EdgeDropColors.PorcelainSurface,
            surfaceElevated = EdgeDropColors.PorcelainElevated,
            pillSurface = EdgeDropColors.PorcelainPill,
            borderHairline = EdgeDropColors.PorcelainBorderHairline,
            activeBorder = EdgeDropColors.PorcelainBorderActive,
            textPrimary = EdgeDropColors.PorcelainTextPrimary,
            textSecondary = EdgeDropColors.PorcelainTextSecondary,
            textMuted = EdgeDropColors.PorcelainTextMuted,
            accent = EdgeDropColors.PorcelainAccent,
            accentSurface = EdgeDropColors.PorcelainAccentSurface,
            success = EdgeDropColors.Success,
            successSurface = EdgeDropColors.SuccessSurfaceLight,
            error = EdgeDropColors.Error,
            errorSurface = EdgeDropColors.ErrorSurfaceLight,
            gripColor = EdgeDropColors.GripLight
        )
        Vibe.Obsidian -> EdgeDropThemeTokens(
            isLight = false,
            surface = EdgeDropColors.ObsidianSurface,
            surfaceElevated = EdgeDropColors.ObsidianElevated,
            pillSurface = EdgeDropColors.ObsidianPill,
            borderHairline = EdgeDropColors.ObsidianBorderHairline,
            activeBorder = EdgeDropColors.ObsidianBorderActive,
            textPrimary = EdgeDropColors.ObsidianTextPrimary,
            textSecondary = EdgeDropColors.ObsidianTextSecondary,
            textMuted = EdgeDropColors.ObsidianTextMuted,
            accent = EdgeDropColors.ObsidianAccent,
            accentSurface = EdgeDropColors.ObsidianAccentSurface,
            success = EdgeDropColors.Success,
            successSurface = EdgeDropColors.SuccessSurfaceDark,
            error = EdgeDropColors.Error,
            errorSurface = EdgeDropColors.ErrorSurfaceDark,
            gripColor = EdgeDropColors.GripDark
        )
        Vibe.Mist -> EdgeDropThemeTokens(
            isLight = false,
            surface = EdgeDropColors.MistSurface,
            surfaceElevated = EdgeDropColors.MistElevated,
            pillSurface = EdgeDropColors.MistPill,
            borderHairline = EdgeDropColors.MistBorderHairline,
            activeBorder = EdgeDropColors.MistBorderActive,
            textPrimary = EdgeDropColors.MistTextPrimary,
            textSecondary = EdgeDropColors.MistTextSecondary,
            textMuted = EdgeDropColors.MistTextMuted,
            accent = EdgeDropColors.MistAccent,
            accentSurface = EdgeDropColors.MistAccentSurface,
            success = EdgeDropColors.Success,
            successSurface = EdgeDropColors.SuccessSurfaceDark,
            error = EdgeDropColors.Error,
            errorSurface = EdgeDropColors.ErrorSurfaceDark,
            gripColor = EdgeDropColors.GripDark
        )
        Vibe.Adaptive -> {
            if (isLight) {
                EdgeDropThemeTokens(
                    isLight = true,
                    surface = EdgeDropColors.PorcelainSurface,
                    surfaceElevated = EdgeDropColors.PorcelainElevated,
                    pillSurface = EdgeDropColors.PorcelainPill,
                    borderHairline = EdgeDropColors.PorcelainBorderHairline,
                    activeBorder = EdgeDropColors.PorcelainBorderActive,
                    textPrimary = EdgeDropColors.PorcelainTextPrimary,
                    textSecondary = EdgeDropColors.PorcelainTextSecondary,
                    textMuted = EdgeDropColors.PorcelainTextMuted,
                    accent = EdgeDropColors.PorcelainAccent,
                    accentSurface = EdgeDropColors.PorcelainAccentSurface,
                    success = EdgeDropColors.Success,
                    successSurface = EdgeDropColors.SuccessSurfaceLight,
                    error = EdgeDropColors.Error,
                    errorSurface = EdgeDropColors.ErrorSurfaceLight,
                    gripColor = EdgeDropColors.GripLight
                )
            } else {
                EdgeDropThemeTokens(
                    isLight = false,
                    surface = EdgeDropColors.ObsidianSurface,
                    surfaceElevated = EdgeDropColors.ObsidianElevated,
                    pillSurface = EdgeDropColors.ObsidianPill,
                    borderHairline = EdgeDropColors.ObsidianBorderHairline,
                    activeBorder = EdgeDropColors.ObsidianBorderActive,
                    textPrimary = EdgeDropColors.ObsidianTextPrimary,
                    textSecondary = EdgeDropColors.ObsidianTextSecondary,
                    textMuted = EdgeDropColors.ObsidianTextMuted,
                    accent = EdgeDropColors.ObsidianAccent,
                    accentSurface = EdgeDropColors.ObsidianAccentSurface,
                    success = EdgeDropColors.Success,
                    successSurface = EdgeDropColors.SuccessSurfaceDark,
                    error = EdgeDropColors.Error,
                    errorSurface = EdgeDropColors.ErrorSurfaceDark,
                    gripColor = EdgeDropColors.GripDark
                )
            }
        }
    }

    CompositionLocalProvider(LocalEdgeDropTheme provides tokens) {
        content()
    }
}
