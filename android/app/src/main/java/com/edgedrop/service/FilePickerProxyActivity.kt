package com.edgedrop.service

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import com.edgedrop.util.FileUtils
import java.io.File

/**
 * Lightweight, transparent Activity used by OverlayNotchService to safely invoke
 * the Android Storage Access Framework (SAF) document picker without killing
 * or interrupting the background overlay service.
 */
class FilePickerProxyActivity : ComponentActivity() {

    companion object {
        var onFileSelectedListener: ((File) -> Unit)? = null
        var onPickerCancelledListener: (() -> Unit)? = null

        fun launch(context: Context) {
            val intent = Intent(context, FilePickerProxyActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_NO_ANIMATION)
            }
            context.startActivity(intent)
        }
    }

    private val documentPicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri != null) {
            try {
                // Attempt to persist readable access if content provider supports it
                try {
                    val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
                    contentResolver.takePersistableUriPermission(uri, flags)
                } catch (_: SecurityException) {}

                val cachedFile = FileUtils.copyUriToCache(this, uri)
                if (cachedFile != null) {
                    onFileSelectedListener?.invoke(cachedFile)
                    OverlayNotchService.onFilePicked(this, cachedFile.absolutePath)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        } else {
            onPickerCancelledListener?.invoke()
            OverlayNotchService.onPickerCancelled(this)
        }
        finish()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Request document picker for any file type supported by EdgeDrop
        documentPicker.launch(
            arrayOf(
                "image/*",
                "application/pdf",
                "video/*",
                "audio/*",
                "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                "application/msword",
                "application/vnd.ms-excel",
                "application/vnd.ms-powerpoint",
                "*/*"
            )
        )
    }

    override fun finish() {
        super.finish()
        // Suppress transition animation to keep experience seamless
        if (Build.VERSION.SDK_INT >= 34) {
            overrideActivityTransition(OVERRIDE_TRANSITION_CLOSE, 0, 0)
        } else {
            @Suppress("DEPRECATION")
            overridePendingTransition(0, 0)
        }
    }
}
