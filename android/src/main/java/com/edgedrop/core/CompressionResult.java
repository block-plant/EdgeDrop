package com.edgedrop.core;

public class CompressionResult {
    public final boolean success;
    public final boolean hitFloor;
    public final String floorNote;
    public final String outputPath;
    public final long outputBytes;
    public final String errorMessage;
    public final long durationMs;

    public CompressionResult(boolean success,
                             boolean hitFloor,
                             String floorNote,
                             String outputPath,
                             long outputBytes,
                             String errorMessage,
                             long durationMs) {
        this.success = success;
        this.hitFloor = hitFloor;
        this.floorNote = floorNote;
        this.outputPath = outputPath;
        this.outputBytes = outputBytes;
        this.errorMessage = errorMessage;
        this.durationMs = durationMs;
    }

    @Override
    public String toString() {
        return "CompressionResult{" +
                "success=" + success +
                ", hitFloor=" + hitFloor +
                ", floorNote='" + floorNote + '\'' +
                ", outputPath='" + outputPath + '\'' +
                ", outputBytes=" + outputBytes +
                ", errorMessage='" + errorMessage + '\'' +
                ", durationMs=" + durationMs +
                '}';
    }
}
