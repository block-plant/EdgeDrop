package com.edgedrop.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.edgedrop.R
import com.edgedrop.ui.theme.EdgeDropColors
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe

@Composable
fun TopBar(
    currentVibe: Vibe,
    isPinned: Boolean,
    onVibeSelected: (Vibe) -> Unit,
    onTogglePin: () -> Unit,
    onOpenSettings: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens

    Row(
        modifier = modifier
            .fillMaxWidth()
            .height(36.dp)
            .padding(horizontal = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        // ── 1. Vibe Selector Dots ─────────────────────────────────────────────
        Row(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            val vibes = listOf(
                Vibe.Adaptive to (if (theme.isLight) EdgeDropColors.PorcelainElevated else EdgeDropColors.ObsidianElevated),
                Vibe.Obsidian to EdgeDropColors.ObsidianSurface,
                Vibe.Porcelain to EdgeDropColors.PorcelainSurface,
                Vibe.Mist to Color(0xFF60A5FA)
            )

            vibes.forEach { (vibe, dotColor) ->
                val isSelected = currentVibe == vibe
                Box(
                    modifier = Modifier
                        .size(14.dp)
                        .clip(CircleShape)
                        .background(if (isSelected) theme.accentSurface else Color.Transparent)
                        .border(
                            width = if (isSelected) 1.5.dp else 1.dp,
                            color = if (isSelected) theme.accent else theme.borderHairline,
                            shape = CircleShape
                        )
                        .clickable(
                            interactionSource = remember { MutableInteractionSource() },
                            indication = null
                        ) { onVibeSelected(vibe) },
                    contentAlignment = Alignment.Center
                ) {
                    Box(
                        modifier = Modifier
                            .size(7.dp)
                            .clip(CircleShape)
                            .background(dotColor)
                    )
                }
            }
        }

        // ── 2. Center Drag / Collapse Handle ──────────────────────────────────
        Box(
            modifier = Modifier
                .width(28.dp)
                .height(4.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(theme.gripColor)
        )

        // ── 3. Action Icons ───────────────────────────────────────────────────
        Row(
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Pin Toggle
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(if (isPinned) theme.accentSurface else Color.Transparent)
                    .clickable { onTogglePin() },
                contentAlignment = Alignment.Center
            ) {
                TintedIcon(
                    resId = R.drawable.ic_pin,
                    color = if (isPinned) theme.accent else theme.textSecondary,
                    size = 14.dp
                )
            }

            // Settings
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .clickable { onOpenSettings() },
                contentAlignment = Alignment.Center
            ) {
                TintedIcon(
                    resId = R.drawable.ic_settings,
                    color = theme.textSecondary,
                    size = 14.dp
                )
            }

            // Close
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .clickable { onClose() },
                contentAlignment = Alignment.Center
            ) {
                TintedIcon(
                    resId = R.drawable.ic_close,
                    color = theme.textSecondary,
                    size = 13.dp
                )
            }
        }
    }
}
