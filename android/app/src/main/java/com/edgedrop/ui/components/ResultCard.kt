package com.edgedrop.ui.components

import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.os.Build
import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.edgedrop.R
import com.edgedrop.core.CompressionResult
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.util.FileUtils
import java.io.File
import kotlin.math.roundToInt

@Composable
fun ResultCard(
    isProcessing: Boolean,
    progress: Double,
    result: CompressionResult?,
    onStop: () -> Unit,
    onNewFile: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens
    val context = LocalContext.current
    val view = LocalView.current

    if (isProcessing) {
        // ── 1. Processing State ───────────────────────────────────────────────
        Column(
            modifier = modifier
                .fillMaxWidth()
                .padding(vertical = 24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Box(
                modifier = Modifier.size(48.dp),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator(
                    progress = { progress.toFloat().coerceIn(0.06f, 1f) },
                    modifier = Modifier.size(44.dp),
                    color = theme.accent,
                    strokeWidth = 3.dp,
                    trackColor = theme.borderHairline
                )
            }

            Text(
                text = if (progress > 0) "Shrinking your file (${(progress * 100).roundToInt()}%)…" else "Shrinking your file…",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                color = theme.textPrimary
            )

            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(7.dp))
                    .background(theme.surfaceElevated)
                    .border(1.dp, theme.borderHairline, RoundedCornerShape(7.dp))
                    .clickable { onStop() }
                    .padding(horizontal = 14.dp, vertical = 5.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "Stop",
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Medium,
                    color = theme.textSecondary
                )
            }
        }
    } else if (result != null) {
        // ── 2. Completed Result State ─────────────────────────────────────────
        val outputFile = result.outputPath?.let { File(it) }

        Column(
            modifier = modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // Stats Row: Savings Badge & Size Transition
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Savings pill
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.successSurface)
                        .padding(horizontal = 8.dp, vertical = 3.dp)
                ) {
                    Text(
                        text = if (result.savingsPercentage > 0) "-${result.savingsPercentage.roundToInt()}%" else "Done",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        color = theme.success
                    )
                }

                // Size transition
                Text(
                    text = "${result.formattedOriginalSize()} → ${result.formattedOutputSize()}",
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                    fontFamily = FontFamily.Monospace,
                    color = theme.textPrimary
                )
            }

            // Spec summary badge if available
            if (result.outputWidth > 0 && result.outputHeight > 0) {
                Text(
                    text = result.specSummary(),
                    fontSize = 10.sp,
                    color = theme.textMuted
                )
            }

            // Drag / Share Drop Box
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(96.dp)
                    .clip(RoundedCornerShape(theme.chipRadius))
                    .background(theme.surfaceElevated)
                    .border(1.5.dp, theme.borderHairline, RoundedCornerShape(theme.chipRadius))
                    .clickable {
                        // Tap to share via Android Share Sheet
                        if (outputFile != null && outputFile.exists()) {
                            val shareUri = FileUtils.getShareUri(context, outputFile)
                            val intent = Intent(Intent.ACTION_SEND).apply {
                                type = FileUtils.getMimeType(outputFile)
                                putExtra(Intent.EXTRA_STREAM, shareUri)
                                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                            }
                            context.startActivity(Intent.createChooser(intent, "Share compressed file"))
                        }
                    },
                contentAlignment = Alignment.Center
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(32.dp)
                            .clip(CircleShape)
                            .background(theme.pillSurface)
                            .border(1.dp, theme.borderHairline, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        TintedIcon(
                            resId = R.drawable.ic_drop,
                            color = theme.textSecondary,
                            size = 15.dp
                        )
                    }

                    Text(
                        text = "Tap to share",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = theme.textPrimary
                    )

                    Text(
                        text = "Or drag out to another app in split screen",
                        fontSize = 10.sp,
                        color = theme.textMuted
                    )
                }
            }

            // Floor note notice if hit quality floor
            if (result.hitFloor && result.floorNote != null) {
                Text(
                    text = result.floorNote,
                    fontSize = 10.sp,
                    color = theme.textMuted,
                    modifier = Modifier.padding(horizontal = 4.dp)
                )
            }

            // Action Buttons Row (Copy, Show/Open, New)
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Copy
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(34.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.surfaceElevated)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                        .clickable {
                            if (outputFile != null && outputFile.exists()) {
                                FileUtils.copyFileToClipboard(context, outputFile)
                            }
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TintedIcon(resId = R.drawable.ic_copy, color = theme.textSecondary, size = 12.dp)
                        Text(text = "Copy", fontSize = 11.sp, fontWeight = FontWeight.Medium, color = theme.textPrimary)
                    }
                }

                // Show / Open
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(34.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.surfaceElevated)
                        .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                        .clickable {
                            if (outputFile != null && outputFile.exists()) {
                                val uri = FileUtils.getShareUri(context, outputFile)
                                val intent = Intent(Intent.ACTION_VIEW).apply {
                                    setDataAndType(uri, FileUtils.getMimeType(outputFile))
                                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                                }
                                try {
                                    context.startActivity(intent)
                                } catch (_: Exception) {}
                            }
                        },
                    contentAlignment = Alignment.Center
                ) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TintedIcon(resId = R.drawable.ic_folder, color = theme.textSecondary, size = 12.dp)
                        Text(text = "Open", fontSize = 11.sp, fontWeight = FontWeight.Medium, color = theme.textPrimary)
                    }
                }

                // New
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .height(34.dp)
                        .clip(RoundedCornerShape(6.dp))
                        .background(theme.accent)
                        .clickable { onNewFile() },
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "New",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = Color.White
                    )
                }
            }
        }
    }
}
