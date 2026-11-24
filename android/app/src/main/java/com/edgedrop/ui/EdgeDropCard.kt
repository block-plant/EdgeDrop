package com.edgedrop.ui

import android.app.Activity
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import androidx.activity.compose.LocalActivityResultRegistryOwner
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableDoubleStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.edgedrop.core.CompressionResult
import com.edgedrop.core.DetectedFormat
import com.edgedrop.core.EdgeDropEngine
import com.edgedrop.ui.components.ConfigPanel
import com.edgedrop.ui.components.DropZoneCard
import com.edgedrop.ui.components.ResultCard
import com.edgedrop.ui.components.SettingsPopover
import com.edgedrop.ui.components.TopBar
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe
import com.edgedrop.util.FileUtils
import com.edgedrop.util.PreferencesManager
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import java.io.File

@Composable
fun EdgeDropCard(
    initialFile: File? = null,
    onClose: () -> Unit = {},
    onLaunchFilePicker: (() -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val prefs = remember { PreferencesManager(context) }
    val engine = remember { EdgeDropEngine(context) }
    val theme = EdgeDropTheme.tokens

    var currentVibe by remember { mutableStateOf(Vibe.fromId(prefs.vibe)) }
    var isPinned by remember { mutableStateOf(prefs.isPinned) }
    var showSettings by remember { mutableStateOf(false) }

    var selectedFile by remember { mutableStateOf(initialFile) }
    var detectedFormat by remember { mutableStateOf(initialFile?.let { engine.detectFormat(it) } ?: DetectedFormat.Unknown) }

    var targetNumber by remember { mutableIntStateOf(500) }
    var targetUnit by remember { mutableStateOf("KB") }

    var isProcessing by remember { mutableStateOf(false) }
    var progress by remember { mutableDoubleStateOf(0.0) }
    var compressionResult by remember { mutableStateOf<CompressionResult?>(null) }
    var compressionJob by remember { mutableStateOf<Job?>(null) }

    var recentFiles by remember { mutableStateOf(prefs.recentFiles) }

    // Update if initialFile changes (e.g. from Share Intent)
    LaunchedEffect(initialFile) {
        if (initialFile != null) {
            selectedFile = initialFile
            detectedFormat = engine.detectFormat(initialFile)
            compressionResult = null
            prefs.addRecentFile(initialFile.absolutePath)
            recentFiles = prefs.recentFiles
        }
    }

    val registryOwner = LocalActivityResultRegistryOwner.current
    val filePickerLauncher = if (registryOwner != null && onLaunchFilePicker == null) {
        rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri ->
            if (uri != null) {
                val cached = FileUtils.copyUriToCache(context, uri)
                if (cached != null) {
                    selectedFile = cached
                    detectedFormat = engine.detectFormat(cached)
                    compressionResult = null
                    prefs.addRecentFile(cached.absolutePath)
                    recentFiles = prefs.recentFiles
                }
            }
        }
    } else {
        null
    }

    fun chooseFile() {
        if (onLaunchFilePicker != null) {
            onLaunchFilePicker()
        } else {
            filePickerLauncher?.launch(arrayOf("*/*"))
        }
    }

    fun pasteFromClipboard() {
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = clipboard.primaryClip
        if (clip != null && clip.itemCount > 0) {
            val item = clip.getItemAt(0)
            val uri = item.uri
            if (uri != null) {
                val cached = FileUtils.copyUriToCache(context, uri)
                if (cached != null) {
                    selectedFile = cached
                    detectedFormat = engine.detectFormat(cached)
                    compressionResult = null
                    prefs.addRecentFile(cached.absolutePath)
                    recentFiles = prefs.recentFiles
                }
            }
        }
    }

    fun startCompression() {
        val file = selectedFile ?: return
        isProcessing = true
        progress = 0.0
        compressionResult = null

        val targetBytes = if (targetUnit == "MB") {
            targetNumber.toLong() * 1024L * 1024L
        } else {
            targetNumber.toLong() * 1024L
        }

        val outputDir = File(context.cacheDir, "outputs")

        compressionJob = scope.launch {
            val res = engine.compressFile(
                inputFile = file,
                outputDir = outputDir,
                targetBytes = targetBytes,
                onProgress = { p -> progress = p }
            )
            isProcessing = false
            compressionResult = res
        }
    }

    EdgeDropTheme(vibe = currentVibe) {
        Box(
            modifier = modifier
                .fillMaxWidth()
                .widthIn(max = 380.dp)
                .clip(RoundedCornerShape(theme.cardRadius))
                .background(theme.surface)
                .border(
                    width = 1.dp,
                    color = theme.borderHairline,
                    shape = RoundedCornerShape(theme.cardRadius)
                )
                .padding(12.dp)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // ── Top Bar ───────────────────────────────────────────────────
                TopBar(
                    currentVibe = currentVibe,
                    isPinned = isPinned,
                    onVibeSelected = { v ->
                        currentVibe = v
                        prefs.vibe = v.id
                    },
                    onTogglePin = {
                        isPinned = !isPinned
                        prefs.isPinned = isPinned
                    },
                    onOpenSettings = { showSettings = !showSettings },
                    onClose = onClose
                )

                // ── Settings Popover ──────────────────────────────────────────
                AnimatedVisibility(
                    visible = showSettings,
                    enter = fadeIn(),
                    exit = fadeOut()
                ) {
                    SettingsPopover(
                        currentVibe = currentVibe,
                        edgeSide = prefs.edgeSide,
                        isOverlayEnabled = prefs.isOverlayEnabled,
                        onVibeSelected = { v ->
                            currentVibe = v
                            prefs.vibe = v.id
                        },
                        onEdgeSideChanged = { side -> prefs.edgeSide = side },
                        onToggleOverlay = { enabled -> prefs.isOverlayEnabled = enabled },
                        onDismiss = { showSettings = false }
                    )
                }

                // ── Main Content State ────────────────────────────────────────
                if (!showSettings) {
                    if (isProcessing || compressionResult != null) {
                        ResultCard(
                            isProcessing = isProcessing,
                            progress = progress,
                            result = compressionResult,
                            onStop = {
                                compressionJob?.cancel()
                                isProcessing = false
                            },
                            onNewFile = {
                                selectedFile = null
                                compressionResult = null
                                isProcessing = false
                            }
                        )
                    } else if (selectedFile != null) {
                        ConfigPanel(
                            selectedFile = selectedFile!!,
                            detectedFormat = detectedFormat,
                            targetNumber = targetNumber,
                            targetUnit = targetUnit,
                            onTargetNumberChanged = { num -> targetNumber = num },
                            onTargetUnitChanged = { u -> targetUnit = u },
                            onRemoveFile = {
                                selectedFile = null
                                compressionResult = null
                            },
                            onCompress = { startCompression() }
                        )
                    } else {
                        DropZoneCard(
                            onChooseFile = { chooseFile() },
                            onPasteFromClipboard = { pasteFromClipboard() },
                            recentFiles = recentFiles,
                            onSelectRecentFile = { file ->
                                selectedFile = file
                                detectedFormat = engine.detectFormat(file)
                                compressionResult = null
                            }
                        )
                    }
                }
            }
        }
    }
}
