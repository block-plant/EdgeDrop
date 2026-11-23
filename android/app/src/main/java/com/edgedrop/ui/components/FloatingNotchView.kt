package com.edgedrop.ui.components

import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import com.edgedrop.ui.theme.EdgeDropTheme

@Composable
fun FloatingNotchView(
    isRightEdge: Boolean,
    isDragging: Boolean,
    onTap: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens

    val scale by animateFloatAsState(
        targetValue = if (isDragging) 1.05f else 1.0f,
        label = "notch_scale"
    )

    // Transparent touch container: 32dp width x 68dp height
    Box(
        modifier = modifier
            .width(32.dp)
            .height(68.dp)
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = null
            ) { onTap() },
        contentAlignment = if (isRightEdge) Alignment.CenterEnd else Alignment.CenterStart
    ) {
        // Visual slim pill: 8dp width x 52dp height, full pill radius 4dp
        Box(
            modifier = Modifier
                .width(8.dp)
                .height(52.dp)
                .scale(scale)
                .clip(RoundedCornerShape(theme.pillRadius))
                .background(theme.pillSurface)
                .border(
                    width = 1.dp,
                    color = theme.borderHairline,
                    shape = RoundedCornerShape(theme.pillRadius)
                ),
            contentAlignment = Alignment.Center
        ) {
            // Subtle translucent grab indicator: 3dp width x 20dp height, radius 1.5dp
            Box(
                modifier = Modifier
                    .width(3.dp)
                    .height(20.dp)
                    .clip(RoundedCornerShape(1.5.dp))
                    .background(theme.gripColor)
            )
        }
    }
}
