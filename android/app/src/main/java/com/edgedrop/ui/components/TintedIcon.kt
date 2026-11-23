package com.edgedrop.ui.components

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

@Composable
fun TintedIcon(
    @DrawableRes resId: Int,
    color: Color,
    modifier: Modifier = Modifier,
    size: Dp = 16.dp,
    contentDescription: String? = null
) {
    Image(
        painter = painterResource(id = resId),
        contentDescription = contentDescription,
        colorFilter = ColorFilter.tint(color),
        modifier = modifier.size(size)
    )
}
