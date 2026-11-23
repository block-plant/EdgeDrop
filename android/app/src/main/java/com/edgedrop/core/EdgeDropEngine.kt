package com.edgedrop.core

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.pdf.PdfDocument
import android.graphics.pdf.PdfRenderer
import android.os.ParcelFileDescriptor
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

class EdgeDropEngine(private val context: Context) {

    fun detectFormat(file: File): DetectedFormat {
        if (!file.exists() || file.length() == 0L) return DetectedFormat.Unknown

        if (EdgeDropCore.isLoaded()) {
            val fmtInt = EdgeDropCore.detectFormat(file.absolutePath)
            val fmt = DetectedFormat.fromId(fmtInt)
            if (fmt != DetectedFormat.Unknown) return fmt
        }

        return DetectedFormat.fromExtension(file.extension)
    }

    suspend fun compressFile(
        inputFile: File,
        outputDir: File,
        targetBytes: Long,
        onProgress: (Double) -> Unit = {}
    ): CompressionResult = withContext(Dispatchers.IO) {
        val startTime = System.currentTimeMillis()

        if (!inputFile.exists()) {
            return@withContext CompressionResult(
                success = false,
                errorMessage = "Input file does not exist: ${inputFile.name}"
            )
        }

        val originalSize = inputFile.length()
        val format = detectFormat(inputFile)

        if (!outputDir.exists()) {
            outputDir.mkdirs()
        }

        val baseName = inputFile.nameWithoutExtension
        val safeBaseName = if (EdgeDropCore.isLoaded()) {
            EdgeDropCore.sanitizeFilename(baseName)
        } else {
            baseName.replace("[^a-zA-Z0-9._-]".toRegex(), "_")
        }

        try {
            when (format) {
                DetectedFormat.JPEG, DetectedFormat.PNG, DetectedFormat.WebP,
                DetectedFormat.HEIC, DetectedFormat.TIFF, DetectedFormat.BMP -> {
                    compressImage(
                        inputFile = inputFile,
                        outputDir = outputDir,
                        safeBaseName = safeBaseName,
                        format = format,
                        originalSize = originalSize,
                        targetBytes = targetBytes,
                        startTime = startTime,
                        onProgress = onProgress
                    )
                }
                DetectedFormat.PDF -> {
                    compressPdf(
                        inputFile = inputFile,
                        outputDir = outputDir,
                        safeBaseName = safeBaseName,
                        originalSize = originalSize,
                        targetBytes = targetBytes,
                        startTime = startTime,
                        onProgress = onProgress
                    )
                }
                DetectedFormat.DOCX, DetectedFormat.PPTX, DetectedFormat.XLSX -> {
                    compressOffice(
                        inputFile = inputFile,
                        outputDir = outputDir,
                        safeBaseName = safeBaseName,
                        format = format,
                        originalSize = originalSize,
                        targetBytes = targetBytes,
                        startTime = startTime,
                        onProgress = onProgress
                    )
                }
                else -> {
                    // Pass-through / copy if already under or unsupported
                    val outputFile = File(outputDir, "${safeBaseName}_shrunk.${inputFile.extension}")
                    inputFile.copyTo(outputFile, overwrite = true)
                    CompressionResult(
                        success = true,
                        hitFloor = true,
                        floorNote = "Original format already at maximum native efficiency.",
                        outputPath = outputFile.absolutePath,
                        originalSizeBytes = originalSize,
                        outputSizeBytes = outputFile.length(),
                        outputFormat = format.displayName,
                        durationMs = System.currentTimeMillis() - startTime
                    )
                }
            }
        } catch (e: Exception) {
            CompressionResult(
                success = false,
                originalSizeBytes = originalSize,
                errorMessage = e.message ?: "Compression failed",
                durationMs = System.currentTimeMillis() - startTime
            )
        }
    }

    private fun compressImage(
        inputFile: File,
        outputDir: File,
        safeBaseName: String,
        format: DetectedFormat,
        originalSize: Long,
        targetBytes: Long,
        startTime: Long,
        onProgress: (Double) -> Unit
    ): CompressionResult {
        onProgress(0.1)

        val options = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeFile(inputFile.absolutePath, options)
        val origWidth = options.outWidth
        val origHeight = options.outHeight

        if (origWidth <= 0 || origHeight <= 0) {
            return CompressionResult(
                success = false,
                errorMessage = "Could not decode image dimensions."
            )
        }

        onProgress(0.25)
        var bitmap = BitmapFactory.decodeFile(inputFile.absolutePath) ?: return CompressionResult(
            success = false,
            errorMessage = "Failed to load image bitmap."
        )

        // Flatten alpha to pure white if converting to JPEG
        val isTargetJpeg = format == DetectedFormat.JPEG || format == DetectedFormat.HEIC || format == DetectedFormat.BMP
        if (isTargetJpeg && bitmap.hasAlpha()) {
            val flattened = Bitmap.createBitmap(bitmap.width, bitmap.height, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(flattened)
            canvas.drawColor(Color.WHITE)
            canvas.drawBitmap(bitmap, 0f, 0f, null)
            bitmap.recycle()
            bitmap = flattened
        }

        val compressFormat = when (format) {
            DetectedFormat.PNG -> Bitmap.CompressFormat.PNG
            DetectedFormat.WebP -> Bitmap.CompressFormat.WEBP
            else -> Bitmap.CompressFormat.JPEG
        }
        val outExt = when (format) {
            DetectedFormat.PNG -> "png"
            DetectedFormat.WebP -> "webp"
            else -> "jpg"
        }

        val effectiveTarget = if (targetBytes > 0) targetBytes else (originalSize * 0.5).toLong().coerceAtLeast(50 * 1024)

        var bestBytes: ByteArray? = null
        var currentQuality = 90
        var lowQuality = 15
        var highQuality = 95
        var currentBitmap = bitmap
        var scale = 1.0

        // Binary search for optimal quality
        for (step in 0..6) {
            onProgress(0.3 + (step.toDouble() / 10.0))
            val midQuality = (lowQuality + highQuality) / 2
            val stream = ByteArrayOutputStream()
            currentBitmap.compress(compressFormat, midQuality, stream)
            val encoded = stream.toByteArray()

            if (encoded.size <= effectiveTarget) {
                bestBytes = encoded
                currentQuality = midQuality
                lowQuality = midQuality + 1 // try higher quality
            } else {
                highQuality = midQuality - 1 // try lower quality
            }
        }

        // If even lowest quality exceeds target, progressively downscale
        while (bestBytes == null || bestBytes.size > effectiveTarget) {
            if (scale <= 0.25) {
                // Reached quality floor
                if (bestBytes == null) {
                    val stream = ByteArrayOutputStream()
                    currentBitmap.compress(compressFormat, 20, stream)
                    bestBytes = stream.toByteArray()
                }
                break
            }
            scale *= 0.85
            onProgress(0.75)
            val targetW = max(32, (currentBitmap.width * 0.85).roundToInt())
            val targetH = max(32, (currentBitmap.height * 0.85).roundToInt())
            val scaled = Bitmap.createScaledBitmap(currentBitmap, targetW, targetH, true)
            if (currentBitmap !== bitmap) {
                currentBitmap.recycle()
            }
            currentBitmap = scaled

            val stream = ByteArrayOutputStream()
            currentBitmap.compress(compressFormat, 65, stream)
            bestBytes = stream.toByteArray()
        }

        onProgress(0.9)
        val finalFile = File(outputDir, "${safeBaseName}_shrunk.$outExt")
        FileOutputStream(finalFile).use { it.write(bestBytes) }

        // Strip JPEG privacy EXIF metadata via native engine
        if (isTargetJpeg && EdgeDropCore.isLoaded()) {
            EdgeDropCore.stripJpegMetadataFile(finalFile.absolutePath)
        }

        onProgress(1.0)
        val hitFloor = finalFile.length() > (effectiveTarget * 1.15)
        val floorNote = if (hitFloor) "Preserved readable quality at natural visual floor." else null

        val result = CompressionResult(
            success = true,
            hitFloor = hitFloor,
            floorNote = floorNote,
            outputPath = finalFile.absolutePath,
            originalSizeBytes = originalSize,
            outputSizeBytes = finalFile.length(),
            outputWidth = currentBitmap.width,
            outputHeight = currentBitmap.height,
            outputDpi = 300,
            outputFormat = format.displayName,
            durationMs = System.currentTimeMillis() - startTime
        )

        if (currentBitmap !== bitmap) currentBitmap.recycle()
        bitmap.recycle()

        return result
    }

    private fun compressPdf(
        inputFile: File,
        outputDir: File,
        safeBaseName: String,
        originalSize: Long,
        targetBytes: Long,
        startTime: Long,
        onProgress: (Double) -> Unit
    ): CompressionResult {
        onProgress(0.15)
        val pfd = ParcelFileDescriptor.open(inputFile, ParcelFileDescriptor.MODE_READ_ONLY)
        val renderer = PdfRenderer(pfd)
        val pageCount = renderer.pageCount

        if (pageCount == 0) {
            renderer.close()
            pfd.close()
            return CompressionResult(success = false, errorMessage = "PDF contains 0 pages.")
        }

        val pdfDocument = PdfDocument()
        val effectiveTarget = if (targetBytes > 0) targetBytes else (originalSize * 0.6).toLong().coerceAtLeast(100 * 1024)
        val perPageBudget = effectiveTarget / pageCount

        // Quality heuristic for JPEG stream per page
        val jpegQuality = when {
            perPageBudget < 50 * 1024 -> 45
            perPageBudget < 150 * 1024 -> 65
            perPageBudget < 300 * 1024 -> 80
            else -> 88
        }

        val scale = when {
            perPageBudget < 40 * 1024 -> 0.7f
            perPageBudget < 80 * 1024 -> 0.85f
            else -> 1.0f
        }

        for (i in 0 until pageCount) {
            onProgress(0.2 + (i.toDouble() / pageCount.toDouble()) * 0.6)
            val page = renderer.openPage(i)
            val pageW = (page.width * scale).roundToInt()
            val pageH = (page.height * scale).roundToInt()

            val bmp = Bitmap.createBitmap(pageW, pageH, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bmp)
            canvas.drawColor(Color.WHITE)
            page.render(bmp, null, null, PdfRenderer.Page.RENDER_MODE_FOR_PRINT)
            page.close()

            // Encode to compressed JPEG stream
            val stream = ByteArrayOutputStream()
            bmp.compress(Bitmap.CompressFormat.JPEG, jpegQuality, stream)
            val compressedBytes = stream.toByteArray()
            val compressedBmp = BitmapFactory.decodeByteArray(compressedBytes, 0, compressedBytes.size)

            val pageInfo = PdfDocument.PageInfo.Builder(page.width, page.height, i + 1).create()
            val docPage = pdfDocument.startPage(pageInfo)
            val pageCanvas = docPage.canvas
            pageCanvas.drawBitmap(compressedBmp, null, android.graphics.Rect(0, 0, page.width, page.height), Paint(Paint.FILTER_BITMAP_FLAG))
            pdfDocument.finishPage(docPage)

            compressedBmp.recycle()
            bmp.recycle()
        }

        renderer.close()
        pfd.close()

        onProgress(0.9)
        val finalFile = File(outputDir, "${safeBaseName}_shrunk.pdf")
        FileOutputStream(finalFile).use { pdfDocument.writeTo(it) }
        pdfDocument.close()

        onProgress(1.0)
        return CompressionResult(
            success = true,
            hitFloor = false,
            outputPath = finalFile.absolutePath,
            originalSizeBytes = originalSize,
            outputSizeBytes = finalFile.length(),
            outputWidth = 0,
            outputHeight = 0,
            outputDpi = 150,
            outputFormat = "PDF",
            durationMs = System.currentTimeMillis() - startTime
        )
    }

    private fun compressOffice(
        inputFile: File,
        outputDir: File,
        safeBaseName: String,
        format: DetectedFormat,
        originalSize: Long,
        targetBytes: Long,
        startTime: Long,
        onProgress: (Double) -> Unit
    ): CompressionResult {
        onProgress(0.2)
        val ext = inputFile.extension
        val finalFile = File(outputDir, "${safeBaseName}_shrunk.$ext")

        ZipInputStream(FileInputStream(inputFile)).use { zis ->
            ZipOutputStream(FileOutputStream(finalFile)).use { zos ->
                var entry: ZipEntry? = zis.nextEntry
                while (entry != null) {
                    val entryName = entry.name
                    zos.putNextEntry(ZipEntry(entryName))

                    val isMedia = entryName.startsWith("word/media/") ||
                                  entryName.startsWith("ppt/media/") ||
                                  entryName.startsWith("xl/media/")

                    if (isMedia && (entryName.endsWith(".jpg", true) || entryName.endsWith(".jpeg", true) || entryName.endsWith(".png", true))) {
                        val raw = zis.readBytes()
                        val bmp = BitmapFactory.decodeByteArray(raw, 0, raw.size)
                        if (bmp != null) {
                            val stream = ByteArrayOutputStream()
                            bmp.compress(Bitmap.CompressFormat.JPEG, 65, stream)
                            val compBytes = stream.toByteArray()
                            if (compBytes.size < raw.size) {
                                zos.write(compBytes)
                            } else {
                                zos.write(raw)
                            }
                            bmp.recycle()
                        } else {
                            zos.write(raw)
                        }
                    } else {
                        zis.copyTo(zos)
                    }

                    zos.closeEntry()
                    zis.closeEntry()
                    entry = zis.nextEntry
                }
            }
        }

        onProgress(1.0)
        return CompressionResult(
            success = true,
            outputPath = finalFile.absolutePath,
            originalSizeBytes = originalSize,
            outputSizeBytes = finalFile.length(),
            outputFormat = format.displayName,
            durationMs = System.currentTimeMillis() - startTime
        )
    }
}
