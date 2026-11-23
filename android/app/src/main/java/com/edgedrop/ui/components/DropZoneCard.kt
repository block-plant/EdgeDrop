package com.edgedrop.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.edgedrop.R
import com.edgedrop.ui.theme.EdgeDropTheme
import java.io.File

@Composable
fun DropZoneCard(
    onChooseFile: () -> Unit,
    onPasteFromClipboard: () -> Unit,
    recentFiles: List<String>,
    onSelectRecentFile: (File) -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens

    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        // ── Main Dashed Drop Container ────────────────────────────────────────
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(148.dp)
                .clip(RoundedCornerShape(theme.chipRadius))
                .background(theme.surfaceElevated)
                .border(
                    width = 1.5.dp,
                    color = theme.borderHairline,
                    shape = RoundedCornerShape(theme.chipRadius)
                )
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null
                ) { onChooseFile() },
            contentAlignment = Alignment.Center
        ) {
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Centered 36x36dp icon badge with 18dp cloud/drop icon
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .background(theme.pillSurface)
                        .border(1.dp, theme.borderHairline, CircleShape),
                    contentAlignment = Alignment.Center
                ) {
                    TintedIcon(
                        resId = R.drawable.ic_drop,
                        color = theme.textSecondary,
                        size = 18.dp
                    )
                }

                // Everyday human labels
                Text(
                    text = "Pick any file to shrink",
                    fontSize = 15.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = theme.textPrimary
                )

                Text(
                    text = "Photos · PDF · Docs · Video · Audio",
                    fontSize = 11.sp,
                    color = theme.textSecondary
                )

                // Action links
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "Choose file",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        color = theme.accent,
                        textDecoration = TextDecoration.Underline,
                        modifier = Modifier.clickable { onChooseFile() }
                    )

                    Text(
                        text = "·",
                        fontSize = 12.sp,
                        color = theme.textMuted
                    )

                    Text(
                        text = "Paste",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Medium,
                        color = theme.accent,
                        textDecoration = TextDecoration.Underline,
                        modifier = Modifier.clickable { onPasteFromClipboard() }
                    )
                }
            }
        }

        // ── Recent Files Chips ────────────────────────────────────────────────
        if (recentFiles.isNotEmpty()) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(
                    text = "Recent files",
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Medium,
                    color = theme.textMuted,
                    modifier = Modifier.padding(horizontal = 2.dp)
                )

                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    items(recentFiles) { path ->
                        val file = File(path)
                        if (file.exists()) {
                            Row(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(6.dp))
                                    .background(theme.surfaceElevated)
                                    .border(1.dp, theme.borderHairline, RoundedCornerShape(6.dp))
                                    .clickable { onSelectRecentFile(file) }
                                    .padding(horizontal = 8.dp, vertical = 4.dp),
                                horizontalArrangement = Arrangement.spacedBy(4.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                TintedIcon(
                                    resId = R.drawable.ic_document,
                                    color = theme.textSecondary,
                                    size = 11.dp
                                )
                                Text(
                                    text = file.name,
                                    fontSize = 11.sp,
                                    color = theme.textPrimary,
                                    maxLines = 1,
                                    overflow = TextOverflow.Ellipsis
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}
