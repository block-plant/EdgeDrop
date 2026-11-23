package com.edgedrop.util

import android.content.ClipData
import android.content.ClipboardManager
import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.provider.OpenableColumns
import androidx.core.content.FileProvider
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream

object FileUtils {

    fun copyUriToCache(context: Context, uri: Uri): File? {
        return try {
            val fileName = getFileName(context, uri) ?: "file_${System.currentTimeMillis()}"
            val cacheDir = File(context.cacheDir, "incoming")
            if (!cacheDir.exists()) cacheDir.mkdirs()

            val tempFile = File(cacheDir, fileName)
            context.contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(tempFile).use { output ->
                    input.copyTo(output)
                }
            }
            tempFile
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }

    fun getFileName(context: Context, uri: Uri): String? {
        var name: String? = null
        if (uri.scheme == "content") {
            context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) {
                    val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    if (idx >= 0) {
                        name = cursor.getString(idx)
                    }
                }
            }
        }
        if (name == null) {
            name = uri.path?.substringAfterLast('/')
        }
        return name
    }

    fun saveToDownloads(context: Context, sourceFile: File, displayName: String? = null): Uri? {
        val name = displayName ?: sourceFile.name
        val mimeType = getMimeType(sourceFile)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val values = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, name)
                put(MediaStore.Downloads.MIME_TYPE, mimeType)
                put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/EdgeDrop")
                put(MediaStore.Downloads.IS_PENDING, 1)
            }

            val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values) ?: return null

            context.contentResolver.openOutputStream(uri)?.use { out ->
                FileInputStream(sourceFile).use { inStream ->
                    inStream.copyTo(out)
                }
            }

            values.clear()
            values.put(MediaStore.Downloads.IS_PENDING, 0)
            context.contentResolver.update(uri, values, null, null)
            return uri
        } else {
            val downloadsDir = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "EdgeDrop")
            if (!downloadsDir.exists()) downloadsDir.mkdirs()
            val destFile = File(downloadsDir, name)
            sourceFile.copyTo(destFile, overwrite = true)
            return Uri.fromFile(destFile)
        }
    }

    fun getShareUri(context: Context, file: File): Uri {
        return FileProvider.getUriForFile(
            context,
            "${context.packageName}.fileprovider",
            file
        )
    }

    fun copyFileToClipboard(context: Context, file: File) {
        val uri = getShareUri(context, file)
        val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val clip = ClipData.newUri(context.contentResolver, file.name, uri)
        clipboard.setPrimaryClip(clip)
    }

    fun getMimeType(file: File): String {
        val ext = file.extension.lowercase()
        return when (ext) {
            "jpg", "jpeg" -> "image/jpeg"
            "png" -> "image/png"
            "webp" -> "image/webp"
            "heic" -> "image/heic"
            "pdf" -> "application/pdf"
            "docx" -> "application/vnd.openxmlformats-officedocument.wordprocessingml.document"
            "pptx" -> "application/vnd.openxmlformats-officedocument.presentationml.presentation"
            "xlsx" -> "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
            "mp4" -> "video/mp4"
            "mp3" -> "audio/mpeg"
            else -> "*/*"
        }
    }

    fun clearCache(context: Context) {
        try {
            val cacheDir = File(context.cacheDir, "incoming")
            if (cacheDir.exists()) {
                cacheDir.deleteRecursively()
            }
        } catch (_: Exception) {}
    }
}
