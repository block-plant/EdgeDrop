package com.edgedrop.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.edgedrop.R
import com.edgedrop.core.CompressionResult
import com.edgedrop.core.DetectedFormat
import com.edgedrop.core.Preset
import com.edgedrop.core.PresetRegistry
import com.edgedrop.ui.theme.EdgeDropTheme
import java.io.File

data class QuickChip(val label: String, val value: Int, val unit: String)

@Composable
fun ConfigPanel(
    selectedFile: File,
    detectedFormat: DetectedFormat,
    targetNumber: Int,
    targetUnit: String,
    onTargetNumberChanged: (Int) -> Unit,
    onTargetUnitChanged: (String) -> Unit,
    onRemoveFile: () -> Unit,
    onCompress: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens
    val focusManager = LocalFocusManager.current

    val originalBytes = selectedFile.length()
    val quickChips = remember(originalBytes) {
        if (originalBytes < 2 * 1024 * 1024) {
            listOf(
                QuickChip("50 KB", 50, "KB"),
                QuickChip("100 KB", 100, "KB"),
                QuickChip("200 KB", 200, "KB"),
                QuickChip("500 KB", 500, "KB")
            )
        } else if (originalBytes <= 15 * 1024 * 1024) {
            listOf(
                QuickChip("200 KB", 200, "KB"),
                QuickChip("500 KB", 500, "KB"),
                QuickChip("1 MB", 1, "MB"),
                QuickChip("2 MB", 2, "MB")
            )
        } else {
            listOf(
                QuickChip("5 MB", 5, "MB"),
                QuickChip("10 MB", 10, "MB"),
                QuickChip("25 MB", 25, "MB"),
                QuickChip("50 MB", 50, "MB")
            )
        }
    }

    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // ── 1. Uploaded File Strip ────────────────────────────────────────────
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .height(40.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(theme.surfaceElevated)
                .border(1.dp, theme.borderHairline, RoundedCornerShape(10.dp))
                .padding(horizontal = 8.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Row(
                modifier = Modifier.weight(1f),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Icon badge
                Box(
                    modifier = Modifier
                        .size(24.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.accentSurface)
                        .border(1.dp, theme.accent, RoundedCornerShape(6.dp)),
                    contentAlignment = Alignment.Center
                ) {
                    TintedIcon(
                        resId = when (detectedFormat) {
                            DetectedFormat.JPEG, DetectedFormat.PNG, DetectedFormat.WebP, DetectedFormat.HEIC -> R.drawable.ic_photo
                            DetectedFormat.PDF -> R.drawable.ic_document
                            else -> R.drawable.ic_document
                        },
                        color = theme.accent,
                        size = 13.dp
                    )
                }

                // File name & original size
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = selectedFile.name,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        color = theme.textPrimary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = CompressionResult.formatBytes(originalBytes),
                        fontSize = 10.sp,
                        color = theme.textSecondary
                    )
                }

                // Format badge
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(theme.pillSurface)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(4.dp))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = detectedFormat.displayName,
                        fontSize = 9.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = theme.textSecondary
                    )
                }
            }

            // Remove button
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .clickable { onRemoveFile() },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "×",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = theme.textMuted
                )
            }
        }

        // ── 2. Target Size & Segmented KB/MB Row ──────────────────────────────
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = "Target size",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = theme.textPrimary
            )

            Row(
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Numeric input box
                Box(
                    modifier = Modifier
                        .width(72.dp)
                        .height(30.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.surfaceElevated)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                        .padding(horizontal = 8.dp),
                    contentAlignment = Alignment.CenterStart
                ) {
                    BasicTextField(
                        value = targetNumber.toString(),
                        onValueChange = { str ->
                            val num = str.filter { it.isDigit() }.toIntOrNull() ?: 1
                            onTargetNumberChanged(num.coerceAtLeast(1))
                        },
                        textStyle = TextStyle(
                            color = theme.textPrimary,
                            fontSize = 13.sp,
                            fontWeight = FontWeight.SemiBold,
                            fontFamily = FontFamily.Monospace
                        ),
                        cursorBrush = SolidColor(theme.accent),
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Number,
                            imeAction = ImeAction.Done
                        ),
                        keyboardActions = KeyboardActions(onDone = { focusManager.clearFocus() }),
                        singleLine = true
                    )
                }

                // Segmented Switch (KB / MB)
                Row(
                    modifier = Modifier
                        .height(30.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.surfaceElevated)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                        .padding(2.dp),
                    horizontalArrangement = Arrangement.spacedBy(2.dp)
                ) {
                    listOf("KB", "MB").forEach { unit ->
                        val isSelected = targetUnit == unit
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(4.dp))
                                .background(if (isSelected) theme.accent else Color.Transparent)
                                .clickable { onTargetUnitChanged(unit) }
                                .padding(horizontal = 8.dp, vertical = 4.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = unit,
                                fontSize = 11.sp,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                                color = if (isSelected) Color.White else theme.textSecondary
                            )
                        }
                    }
                }
            }
        }

        // ── 3. Quick Chips Row ────────────────────────────────────────────────
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            items(quickChips) { chip ->
                val isSelected = targetNumber == chip.value && targetUnit == chip.unit
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(if (isSelected) theme.accentSurface else theme.surfaceElevated)
                        .border(
                            1.dp,
                            if (isSelected) theme.accent else theme.borderHairline,
                            RoundedCornerShape(6.dp)
                        )
                        .clickable {
                            onTargetNumberChanged(chip.value)
                            onTargetUnitChanged(chip.unit)
                        }
                        .padding(horizontal = 10.dp, vertical = 5.dp)
                ) {
                    Text(
                        text = chip.label,
                        fontSize = 11.sp,
                        fontWeight = if (isSelected) FontWeight.SemiBold else FontWeight.Normal,
                        color = if (isSelected) theme.accent else theme.textSecondary
                    )
                }
            }
        }

        // ── 4. Preset Templates Row ───────────────────────────────────────────
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(6.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            items(PresetRegistry.presets) { preset ->
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.pillSurface)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                        .clickable {
                            onTargetNumberChanged(preset.targetKb)
                            onTargetUnitChanged("KB")
                        }
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = preset.name,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Medium,
                        color = theme.textMuted
                    )
                }
            }
        }

        // ── 5. Primary CTA ────────────────────────────────────────────────────
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(38.dp)
                .clip(RoundedCornerShape(theme.buttonRadius))
                .background(theme.accent)
                .clickable { onCompress() },
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = "Shrink file",
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
                color = Color.White
            )
        }
    }
}
