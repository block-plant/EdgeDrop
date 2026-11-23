package com.edgedrop.service

import android.content.Context
import android.graphics.PixelFormat
import android.os.Build
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import com.edgedrop.ui.EdgeDropCard
import com.edgedrop.ui.components.FloatingNotchView
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe
import com.edgedrop.util.GestureExclusionHelper
import com.edgedrop.util.PreferencesManager
import kotlin.math.roundToInt

class OverlayViewManager(private val context: Context) : LifecycleOwner, SavedStateRegistryOwner {

    private val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
    private val prefs = PreferencesManager(context)

    private val lifecycleRegistry = LifecycleRegistry(this)
    private val savedStateRegistryController = SavedStateRegistryController.create(this)

    override val lifecycle: Lifecycle get() = lifecycleRegistry
    override val savedStateRegistry: SavedStateRegistry get() = savedStateRegistryController.savedStateRegistry

    private var composeView: ComposeView? = null
    private var isExpanded = false
    private var isVisible = true

    init {
        savedStateRegistryController.performRestore(null)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
    }

    fun show() {
        if (composeView != null) return

        val view = ComposeView(context).apply {
            setViewTreeLifecycleOwner(this@OverlayViewManager)
            setViewTreeSavedStateRegistryOwner(this@OverlayViewManager)
        }
        composeView = view

        updateContent(isDragging = false)
        val params = createNotchLayoutParams()

        setupTouchListener(view, params)
        windowManager.addView(view, params)
        GestureExclusionHelper.updateExclusion(view)
    }

    fun hide() {
        composeView?.let {
            try {
                windowManager.removeView(it)
            } catch (_: Exception) {}
            composeView = null
        }
    }

    fun toggleVisibility(): Boolean {
        isVisible = !isVisible
        if (isVisible) {
            show()
        } else {
            hide()
        }
        return isVisible
    }

    private fun updateContent(isDragging: Boolean) {
        val view = composeView ?: return
        val currentVibe = Vibe.fromId(prefs.vibe)
        val isRight = prefs.edgeSide != "left"

        view.setContent {
            EdgeDropTheme(vibe = currentVibe) {
                if (isExpanded) {
                    EdgeDropCard(
                        onClose = { collapse() }
                    )
                } else {
                    FloatingNotchView(
                        isRightEdge = isRight,
                        isDragging = isDragging,
                        onTap = { expand() }
                    )
                }
            }
        }
    }

    private fun expand() {
        val view = composeView ?: return
        isExpanded = true
        updateContent(isDragging = false)

        val displayMetrics = context.resources.displayMetrics
        val screenW = displayMetrics.widthPixels
        val cardW = (screenW * 0.92f).coerceAtMost(380f * displayMetrics.density).roundToInt()

        val params = WindowManager.LayoutParams(
            cardW,
            WindowManager.LayoutParams.WRAP_CONTENT,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            else
                WindowManager.LayoutParams.TYPE_PHONE,
            WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN, // Clear FLAG_NOT_FOCUSABLE for soft keyboard
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.CENTER
        }

        try {
            windowManager.updateViewLayout(view, params)
        } catch (_: Exception) {}
    }

    private fun collapse() {
        val view = composeView ?: return
        isExpanded = false
        updateContent(isDragging = false)

        val params = createNotchLayoutParams()
        try {
            windowManager.updateViewLayout(view, params)
            GestureExclusionHelper.updateExclusion(view)
        } catch (_: Exception) {}
    }

    private fun createNotchLayoutParams(): WindowManager.LayoutParams {
        val displayMetrics = context.resources.displayMetrics
        val density = displayMetrics.density
        val screenH = displayMetrics.heightPixels

        val notchWidth = (32 * density).roundToInt()
        val notchHeight = (68 * density).roundToInt()

        val isRight = prefs.edgeSide != "left"
        val yPos = (screenH * prefs.edgeYRatio).roundToInt() - (notchHeight / 2)

        return WindowManager.LayoutParams(
            notchWidth,
            notchHeight,
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            else
                WindowManager.LayoutParams.TYPE_PHONE,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.TOP or (if (isRight) Gravity.END else Gravity.START)
            x = 0
            y = yPos
        }
    }

    private fun setupTouchListener(view: View, params: WindowManager.LayoutParams) {
        var initialX = 0
        var initialY = 0
        var initialTouchX = 0f
        var initialTouchY = 0f
        var isDragging = false

        view.setOnTouchListener { _, event ->
            if (isExpanded) return@setOnTouchListener false

            val displayMetrics = context.resources.displayMetrics
            val screenW = displayMetrics.widthPixels
            val screenH = displayMetrics.heightPixels

            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    initialX = params.x
                    initialY = params.y
                    initialTouchX = event.rawX
                    initialTouchY = event.rawY
                    isDragging = false
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = event.rawX - initialTouchX
                    val dy = event.rawY - initialTouchY

                    if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
                        isDragging = true
                        updateContent(isDragging = true)

                        // Vertical position clamped to 18%–65% of screen height
                        val newY = (initialY + dy).roundToInt()
                        val minY = (screenH * 0.18f).roundToInt()
                        val maxY = (screenH * 0.65f).roundToInt()
                        params.y = newY.coerceIn(minY, maxY)

                        // Update ratio
                        prefs.edgeYRatio = params.y.toFloat() / screenH.toFloat()

                        // Horizontal snap crossing check
                        val isCurrentlyRight = prefs.edgeSide != "left"
                        if (isCurrentlyRight && event.rawX < (screenW * 0.40f)) {
                            prefs.edgeSide = "left"
                            params.gravity = Gravity.TOP or Gravity.START
                        } else if (!isCurrentlyRight && event.rawX > (screenW * 0.60f)) {
                            prefs.edgeSide = "right"
                            params.gravity = Gravity.TOP or Gravity.END
                        }

                        windowManager.updateViewLayout(view, params)
                        GestureExclusionHelper.updateExclusion(view)
                    }
                    true
                }
                MotionEvent.ACTION_UP -> {
                    if (!isDragging) {
                        expand()
                    } else {
                        updateContent(isDragging = false)
                        params.x = 0
                        windowManager.updateViewLayout(view, params)
                        GestureExclusionHelper.updateExclusion(view)
                    }
                    true
                }
                else -> false
            }
        }
    }

    fun onDestroy() {
        hide()
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_DESTROY)
    }
}
