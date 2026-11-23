package com.edgedrop.core;

public class EdgeDropCore {
    static {
        System.loadLibrary("edgedrop_jni");
    }

    public interface ProgressListener {
        void onProgress(double progress);
    }

    public static native int detectFormat(String filePath);

    public static native long createCancelToken();

    public static native void cancelJob(long cancelHandle);

    public static native void freeCancelToken(long cancelHandle);

    public static native CompressionResult compressFile(
        String inputPath,
        String outputDir,
        long targetBytes,
        ProgressListener listener,
        long cancelHandle
    );
}
