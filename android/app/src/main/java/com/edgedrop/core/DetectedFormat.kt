package com.edgedrop.core

enum class DetectedFormat(val id: Int, val displayName: String, val category: String) {
    JPEG(0, "JPG", "Photo"),
    PNG(1, "PNG", "Photo"),
    WebP(2, "WebP", "Photo"),
    HEIC(3, "HEIC", "Photo"),
    TIFF(4, "TIFF", "Photo"),
    BMP(5, "BMP", "Photo"),
    PDF(6, "PDF", "Document"),
    DOCX(7, "DOCX", "Document"),
    PPTX(8, "PPTX", "Document"),
    XLSX(9, "XLSX", "Document"),
    MP4(10, "MP4", "Video"),
    MOV(11, "MOV", "Video"),
    MKV(12, "MKV", "Video"),
    WEBM(13, "WEBM", "Video"),
    MP3(14, "MP3", "Audio"),
    M4A(15, "M4A", "Audio"),
    AAC(16, "AAC", "Audio"),
    OGG(17, "OGG", "Audio"),
    WAV(18, "WAV", "Audio"),
    GIF(19, "GIF", "Image"),
    Unknown(20, "File", "Other");

    companion object {
        fun fromId(id: Int): DetectedFormat {
            return entries.firstOrNull { it.id == id } ?: Unknown
        }

        fun fromExtension(ext: String): DetectedFormat {
            val clean = ext.trimStart('.').lowercase()
            return when (clean) {
                "jpg", "jpeg" -> JPEG
                "png" -> PNG
                "webp" -> WebP
                "heic", "heif" -> HEIC
                "tif", "tiff" -> TIFF
                "bmp" -> BMP
                "pdf" -> PDF
                "docx" -> DOCX
                "pptx" -> PPTX
                "xlsx" -> XLSX
                "mp4" -> MP4
                "mov" -> MOV
                "mkv" -> MKV
                "webm" -> WEBM
                "mp3" -> MP3
                "m4a" -> M4A
                "aac" -> AAC
                "ogg" -> OGG
                "wav" -> WAV
                "gif" -> GIF
                else -> Unknown
            }
        }
    }
}
