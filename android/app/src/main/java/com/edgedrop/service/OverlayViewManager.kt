package com.edgedrop.service

import android.content.Context
import android.content.res.Configuration
import android.graphics.PixelFormat
import android.os.Build
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.view.WindowManager
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.ViewModelStore
import androidx.lifecycle.ViewModelStoreOwner
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.lifecycle.setViewTreeViewModelStoreOwner
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
import java.io.File
import kotlin.math.hypot
import kotlin.math.roundToInt

class OverlayViewManager(private val context: Context) :
    LifecycleOwner, ViewModelStoreOwner, SavedStateRegistryOwner {

    private val windowManager = context.getSystemService(Context.WINDOW_SERVICE) as WindowManager
    private val prefs = PreferencesManager(context)

    private val lifecycleRegistry = LifecycleRegistry(this)
    private val savedStateRegistryController = SavedStateRegistryController.create(this)
    override val viewModelStore = ViewModelStore()

    override val lifecycle: Lifecycle get() = lifecycleRegistry
    override val savedStateRegistry: SavedStateRegistry get() = savedStateRegistryController.savedStateRegistry

    private var composeView: ComposeView? = null
    var isExpanded = false
        private set
    var isVisible = false
        private set

    private var incomingFile: File? = null

    init {
        savedStateRegistryController.performRestore(null)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)

        // Set up callback listener from FilePickerProxyActivity
        FilePickerProxyActivity.onFileSelectedListener = { file ->
            setIncomingFileAndExpand(file)
        }
        FilePickerProxyActivity.onPickerCancelledListener = {
            if (isExpanded) {
                // Keep expanded so user can choose again or close
                updateContent()
            }
        }
    }

    fun show() {
        if (composeView != null) {
            isVisible = true
            return
        }

        try {
            val view = ComposeView(context).apply {
                setViewTreeLifecycleOwner(this@OverlayViewManager)
                setViewTreeViewModelStoreOwner(this@OverlayViewManager)
                setViewTreeSavedStateRegistryOwner(this@OverlayViewManager)
            }
            composeView = view

            updateContent(isDragging = false)
            val params = createNotchLayoutParams()

            setupTouchListener(view)
            setupKeyListener(view)

            windowManager.addView(view, params)
            isVisible = true
            GestureExclusionHelper.updateExclusion(view)
        } catch (e: Exception) {
            e.printStackTrace()
            composeView = null
            isVisible = false
        }
    }

    fun hide() {
        composeView?.let { view ->
            try {
                if (view.isAttachedToWindow) {
                    windowManager.removeView(view)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
            composeView = null
            isVisible = false
            isExpanded = false
        }
    }

    fun toggleVisibility(): Boolean {
        return if (isVisible && composeView != null) {
            hide()
            false
        } else {
            show()
            true
        }
    }

    fun recenter() {
        prefs.edgeYRatio = 0.40f
        if (isExpanded) {
            collapse()
        } else {
            composeView?.let { view ->
                val params = createNotchLayoutParams()
                try {
                    if (view.isAttachedToWindow) {
                        windowManager.updateViewLayout(view, params)
                        GestureExclusionHelper.updateExclusion(view)
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }
    }

    fun setIncomingFileAndExpand(file: File) {
        incomingFile = file
        expand(file)
    }

    fun expand(file: File? = null) {
        if (file != null) {
            incomingFile = file
        }
        val view = composeView ?: return
        isExpanded = true
        updateContent(isDragging = false)

        val params = createExpandedLayoutParams()
        try {
            if (view.isAttachedToWindow) {
                windowManager.updateViewLayout(view, params)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun collapse() {
        val view = composeView ?: return
        isExpanded = false
        updateContent(isDragging = false)

        val params = createNotchLayoutParams()
        try {
            if (view.isAttachedToWindow) {
                windowManager.updateViewLayout(view, params)
                GestureExclusionHelper.updateExclusion(view)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    fun onConfigurationChanged(newConfig: Configuration) {
        composeView?.let { view ->
            if (view.isAttachedToWindow) {
                val params = if (isExpanded) createExpandedLayoutParams() else createNotchLayoutParams()
                try {
                    windowManager.updateViewLayout(view, params)
                    if (!isExpanded) {
                        GestureExclusionHelper.updateExclusion(view)
                    }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        }
    }

    private fun updateContent(isDragging: Boolean = false) {
        val view = composeView ?: return
        val currentVibe = Vibe.fromId(prefs.vibe)
        val isRight = prefs.edgeSide != "left"

        view.setContent {
            EdgeDropTheme(vibe = currentVibe) {
                if (isExpanded) {
                    EdgeDropCard(
                        initialFile = incomingFile,
                        onClose = { collapse() },
                        onLaunchFilePicker = {
                            FilePickerProxyActivity.launch(context)
                        }
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

    private fun createNotchLayoutParams(): WindowManager.LayoutParams {
        val displayMetrics = context.resources.displayMetrics
        val density = displayMetrics.density
        val screenH = displayMetrics.heightPixels

        val notchWidth = (32 * density).roundToInt()
        val notchHeight = (68 * density).roundToInt()

        val isRight = prefs.edgeSide != "left"
        val yPos = (screenH * prefs.edgeYRatio).roundToInt() - (notchHeight / 2)

        val windowType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }

        val flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH

        return WindowManager.LayoutParams(
            notchWidth,
            notchHeight,
            windowType,
            flags,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.TOP or (if (isRight) Gravity.END else Gravity.START)
            x = 0
            val minY = (screenH * 0.12f).roundToInt()
            val maxY = (screenH * 0.85f).roundToInt() - notchHeight
            y = yPos.coerceIn(minY, maxY)

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
    }

    private fun createExpandedLayoutParams(): WindowManager.LayoutParams {
        val displayMetrics = context.resources.displayMetrics
        val screenW = displayMetrics.widthPixels
        val density = displayMetrics.density
        val cardW = (screenW * 0.94f).coerceAtMost(380f * density).roundToInt()

        val windowType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            @Suppress("DEPRECATION")
            WindowManager.LayoutParams.TYPE_PHONE
        }

        // When expanded, we clear FLAG_NOT_FOCUSABLE so text inputs accept soft keyboard input cleanly
        val flags = WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN or
                WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH

        return WindowManager.LayoutParams(
            cardW,
            WindowManager.LayoutParams.WRAP_CONTENT,
            windowType,
            flags,
            PixelFormat.TRANSLUCENT
        ).apply {
            gravity = Gravity.CENTER
            softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
    }

    private fun setupTouchListener(view: View) {
        val touchSlop = ViewConfiguration.get(context).scaledTouchSlop
        var initialX = 0
        var initialY = 0
        var initialTouchX = 0f
        var initialTouchY = 0f
        var isDragging = false

        view.setOnTouchListener { v, event ->
            if (isExpanded) {
                // If user touches outside the expanded card area, collapse cleanly
                if (event.action == MotionEvent.ACTION_OUTSIDE) {
                    collapse()
                    return@setOnTouchListener true
                }
                return@setOnTouchListener false
            }

            val displayMetrics = context.resources.displayMetrics
            val screenW = displayMetrics.widthPixels
            val screenH = displayMetrics.heightPixels
            val currentParams = v.layoutParams as? WindowManager.LayoutParams ?: return@setOnTouchListener false

            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    initialX = currentParams.x
                    initialY = currentParams.y
                    initialTouchX = event.rawX
                    initialTouchY = event.rawY
                    isDragging = false
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    val dx = event.rawX - initialTouchX
                    val dy = event.rawY - initialTouchY

                    if (hypot(dx, dy) > touchSlop) {
                        if (!isDragging) {
                            isDragging = true
                            updateContent(isDragging = true)
                        }

                        // Clamp Y position cleanly away from status bar and navigation bar
                        val newY = (initialY + dy).roundToInt()
                        val minY = (screenH * 0.12f).roundToInt()
                        val maxY = (screenH * 0.85f).roundToInt() - currentParams.height
                        currentParams.y = newY.coerceIn(minY, maxY)

                        prefs.edgeYRatio = currentParams.y.toFloat() / screenH.toFloat()

                        // Horizontal snap across screen midpoint (35% / 65%)
                        val isCurrentlyRight = prefs.edgeSide != "left"
                        if (isCurrentlyRight && event.rawX < (screenW * 0.35f)) {
                            prefs.edgeSide = "left"
                            currentParams.gravity = Gravity.TOP or Gravity.START
                        } else if (!isCurrentlyRight && event.rawX > (screenW * 0.65f)) {
                            prefs.edgeSide = "right"
                            currentParams.gravity = Gravity.TOP or Gravity.END
                        }

                        try {
                            if (v.isAttachedToWindow) {
                                windowManager.updateViewLayout(v, currentParams)
                                GestureExclusionHelper.updateExclusion(v)
                            }
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                    }
                    true
                }
                MotionEvent.ACTION_UP -> {
                    if (!isDragging) {
                        expand()
                    } else {
                        isDragging = false
                        updateContent(isDragging = false)
                        currentParams.x = 0
                        try {
                            if (v.isAttachedToWindow) {
                                windowManager.updateViewLayout(v, currentParams)
                                GestureExclusionHelper.updateExclusion(v)
                            }
                        } catch (e: Exception) {
                            e.printStackTrace()
                        }
                    }
                    true
                }
                else -> false
            }
        }
    }

    private fun setupKeyListener(view: View) {
        view.isFocusableInTouchMode = true
        view.setOnKeyListener { _, keyCode, event ->
            if (isExpanded && keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_UP) {
                collapse()
                true
            } else {
                false
            }
        }
    }

    fun onDestroy() {
        hide()
        FilePickerProxyActivity.onFileSelectedListener = null
        FilePickerProxyActivity.onPickerCancelledListener = null
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_DESTROY)
        viewModelStore.clear()
    }
}
