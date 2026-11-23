package com.edgedrop.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe

@Composable
fun SettingsPopover(
    currentVibe: Vibe,
    edgeSide: String,
    isOverlayEnabled: Boolean,
    onVibeSelected: (Vibe) -> Unit,
    onEdgeSideChanged: (String) -> Unit,
    onToggleOverlay: (Boolean) -> Unit,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens

    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(theme.cardRadius))
            .background(theme.surface)
            .border(1.dp, theme.borderHairline, RoundedCornerShape(theme.cardRadius))
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        // Title
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Preferences",
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold,
                color = theme.textPrimary
            )
            Text(
                text = "Done",
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                color = theme.accent,
                modifier = Modifier.clickable { onDismiss() }
            )
        }

        // Output destination
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(
                text = "Output location",
                fontSize = 11.sp,
                color = theme.textMuted
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(34.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(theme.surfaceElevated)
                    .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                    .padding(horizontal = 10.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                Text(
                    text = "Downloads/EdgeDrop",
                    fontSize = 12.sp,
                    color = theme.textPrimary
                )
            }
        }

        // Notch Edge Placement
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Screen edge",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = theme.textPrimary
            )

            Row(
                modifier = Modifier
                    .height(30.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(theme.surfaceElevated)
                    .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                    .padding(2.dp),
                horizontalArrangement = Arrangement.spacedBy(2.dp)
            ) {
                listOf("left" to "Left", "right" to "Right").forEach { (side, label) ->
                    val isSelected = edgeSide.equals(side, ignoreCase = true)
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(if (isSelected) theme.accent else Color.Transparent)
                            .clickable { onEdgeSideChanged(side) }
                            .padding(horizontal = 10.dp, vertical = 4.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = label,
                            fontSize = 11.sp,
                            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                            color = if (isSelected) Color.White else theme.textSecondary
                        )
                    }
                }
            }
        }

        // Floating Overlay Service Switch
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text(
                    text = "Floating edge notch",
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Medium,
                    color = theme.textPrimary
                )
                Text(
                    text = "Display over other apps",
                    fontSize = 10.sp,
                    color = theme.textMuted
                )
            }

            Switch(
                checked = isOverlayEnabled,
                onCheckedChange = { onToggleOverlay(it) },
                colors = SwitchDefaults.colors(
                    checkedThumbColor = Color.White,
                    checkedTrackColor = theme.accent,
                    uncheckedThumbColor = theme.textMuted,
                    uncheckedTrackColor = theme.surfaceElevated
                )
            )
        }

        // Vibe selection
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(
                text = "Theme vibe",
                fontSize = 11.sp,
                color = theme.textMuted
            )

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Vibe.entries.forEach { vibe ->
                    val isSelected = currentVibe == vibe
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .height(32.dp)
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (isSelected) theme.accentSurface else theme.surfaceElevated)
                            .border(
                                1.dp,
                                if (isSelected) theme.accent else theme.borderHairline,
                                RoundedCornerShape(6.dp)
                            )
                            .clickable { onVibeSelected(vibe) },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = vibe.displayName,
                            fontSize = 11.sp,
                            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                            color = if (isSelected) theme.accent else theme.textSecondary
                        )
                    }
                }
            }
        }
    }
}
