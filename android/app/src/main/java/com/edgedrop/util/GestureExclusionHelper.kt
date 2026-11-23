package com.edgedrop.util

import android.graphics.Rect
import android.os.Build
import android.view.View
import androidx.core.view.ViewCompat

object GestureExclusionHelper {

    fun updateExclusion(view: View) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            view.post {
                val rect = Rect(0, 0, view.width, view.height)
                ViewCompat.setSystemGestureExclusionRects(view, listOf(rect))
            }
        }
    }
}
