package com.edgedrop.core

data class CompressionResult(
    val success: Boolean,
    val hitFloor: Boolean = false,
    val floorNote: String? = null,
    val outputPath: String? = null,
    val originalSizeBytes: Long = 0L,
    val outputSizeBytes: Long = 0L,
    val outputWidth: Int = 0,
    val outputHeight: Int = 0,
    val outputDpi: Int = 0,
    val outputFormat: String = "",
    val errorMessage: String? = null,
    val durationMs: Long = 0L
) {
    val savingsPercentage: Double
        get() {
            if (originalSizeBytes <= 0L || outputSizeBytes <= 0L) return 0.0
            return ((originalSizeBytes - outputSizeBytes).toDouble() / originalSizeBytes.toDouble()) * 100.0
        }

    fun formattedOriginalSize(): String = formatBytes(originalSizeBytes)
    fun formattedOutputSize(): String = formatBytes(outputSizeBytes)

    fun specSummary(): String {
        return if (outputWidth > 0 && outputHeight > 0 && outputDpi > 0) {
            "$outputFormat · ${outputWidth}×${outputHeight} px · $outputDpi DPI"
        } else if (outputFormat.isNotEmpty()) {
            outputFormat
        } else {
            "Compressed"
        }
    }

    companion object {
        fun formatBytes(bytes: Long): String {
            if (bytes <= 0L) return "0 KB"
            val kb = bytes.toDouble() / 1024.0
            return if (kb < 1024.0) {
                String.format("%.1f KB", kb)
            } else {
                String.format("%.2f MB", kb / 1024.0)
            }
        }
    }
}
