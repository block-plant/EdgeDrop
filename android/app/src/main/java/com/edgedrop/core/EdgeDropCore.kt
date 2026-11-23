package com.edgedrop.core

object EdgeDropCore {
    private var isNativeLoaded = false

    init {
        try {
            System.loadLibrary("edgedrop_jni")
            isNativeLoaded = true
        } catch (e: UnsatisfiedLinkError) {
            android.util.Log.w("EdgeDropCore", "Could not load libedgedrop_jni.so, falling back to pure Android implementations", e)
        }
    }

    fun isLoaded(): Boolean = isNativeLoaded

    external fun detectFormat(filePath: String): Int
    external fun getFormatName(formatId: Int): String
    external fun getFormatLabel(formatId: Int, filePath: String?): String
    external fun validateInputPath(filePath: String): String
    external fun sanitizeFilename(name: String): String
    external fun stripJpegMetadata(data: ByteArray): ByteArray?
    external fun stripJpegMetadataFile(filePath: String): Boolean
    external fun createCancelToken(): Long
    external fun cancelJob(cancelHandle: Long)
    external fun freeCancelToken(cancelHandle: Long)
}
