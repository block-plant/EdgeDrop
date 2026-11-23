#include <jni.h>
#include <string>
#include <atomic>
#include "edgedrop/c_api.h"

extern "C" {

JNIEXPORT jint JNICALL
Java_com_edgedrop_core_EdgeDropCore_detectFormat(JNIEnv* env, jclass /*clazz*/, jstring jFilePath) {
    if (!jFilePath) return 0;
    const char* pathStr = env->GetStringUTFChars(jFilePath, nullptr);
    if (!pathStr) return 0;

    int format = edgedrop_detect_format(pathStr);
    env->ReleaseStringUTFChars(jFilePath, pathStr);
    return format;
}

JNIEXPORT jlong JNICALL
Java_com_edgedrop_core_EdgeDropCore_createCancelToken(JNIEnv* /*env*/, jclass /*clazz*/) {
    auto* flag = new bool(false);
    return reinterpret_cast<jlong>(flag);
}

JNIEXPORT void JNICALL
Java_com_edgedrop_core_EdgeDropCore_cancelJob(JNIEnv* /*env*/, jclass /*clazz*/, jlong cancelHandle) {
    if (cancelHandle != 0) {
        auto* flag = reinterpret_cast<bool*>(cancelHandle);
        edgedrop_cancel(flag);
    }
}

JNIEXPORT void JNICALL
Java_com_edgedrop_core_EdgeDropCore_freeCancelToken(JNIEnv* /*env*/, jclass /*clazz*/, jlong cancelHandle) {
    if (cancelHandle != 0) {
        auto* flag = reinterpret_cast<bool*>(cancelHandle);
        delete flag;
    }
}

struct ProgressContext {
    JavaVM* jvm;
    jobject callbackObj;
    jmethodID onProgressMethod;
};

static void jniProgressBridge(double progress, void* userData) {
    if (!userData) return;
    auto* ctx = static_cast<ProgressContext*>(userData);
    if (!ctx->jvm || !ctx->callbackObj || !ctx->onProgressMethod) return;

    JNIEnv* env = nullptr;
    jint getEnvRes = ctx->jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    bool didAttach = false;
    if (getEnvRes == JNI_EDETACHED) {
        if (ctx->jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            didAttach = true;
        }
    }

    if (env) {
        env->CallVoidMethod(ctx->callbackObj, ctx->onProgressMethod, static_cast<jdouble>(progress));
        if (didAttach) {
            ctx->jvm->DetachCurrentThread();
        }
    }
}

JNIEXPORT jobject JNICALL
Java_com_edgedrop_core_EdgeDropCore_compressFile(JNIEnv* env, jclass /*clazz*/,
                                                jstring jInputPath,
                                                jstring jOutputDir,
                                                jlong jTargetBytes,
                                                jobject jCallback,
                                                jlong cancelHandle) {
    if (!jInputPath) return nullptr;

    const char* inputStr = env->GetStringUTFChars(jInputPath, nullptr);
    const char* outputStr = jOutputDir ? env->GetStringUTFChars(jOutputDir, nullptr) : nullptr;

    ProgressContext progCtx{nullptr, nullptr, nullptr};
    EdgeDropProgressCallback cbPtr = nullptr;
    void* userData = nullptr;

    if (jCallback) {
        env->GetJavaVM(&progCtx.jvm);
        progCtx.callbackObj = jCallback;
        jclass cbClass = env->GetObjectClass(jCallback);
        progCtx.onProgressMethod = env->GetMethodID(cbClass, "onProgress", "(D)V");
        if (progCtx.onProgressMethod) {
            cbPtr = jniProgressBridge;
            userData = &progCtx;
        }
    }

    bool* cancelFlag = cancelHandle != 0 ? reinterpret_cast<bool*>(cancelHandle) : nullptr;

    EdgeDropResult* result = edgedrop_compress_file(inputStr,
                                                    outputStr,
                                                    static_cast<int64_t>(jTargetBytes),
                                                    cbPtr,
                                                    userData,
                                                    cancelFlag);

    env->ReleaseStringUTFChars(jInputPath, inputStr);
    if (outputStr && jOutputDir) {
        env->ReleaseStringUTFChars(jOutputDir, outputStr);
    }

    if (!result) return nullptr;

    // Find CompressionResult class and constructor:
    // CompressionResult(boolean success, boolean hitFloor, String floorNote,
    //                   String outputPath, long outputBytes, String errorMessage, long durationMs)
    jclass resClass = env->FindClass("com/edgedrop/core/CompressionResult");
    if (!resClass) {
        edgedrop_free_result(result);
        return nullptr;
    }

    jmethodID constructor = env->GetMethodID(resClass, "<init>",
        "(ZZLjava/lang/String;Ljava/lang/String;JLjava/lang/String;J)V");
    if (!constructor) {
        edgedrop_free_result(result);
        return nullptr;
    }

    jstring jFloorNote = result->floor_note ? env->NewStringUTF(result->floor_note) : nullptr;
    jstring jOutputPath = result->output_path ? env->NewStringUTF(result->output_path) : nullptr;
    jstring jErrMsg = result->error_message ? env->NewStringUTF(result->error_message) : nullptr;

    jobject jResultObj = env->NewObject(resClass, constructor,
        static_cast<jboolean>(result->success),
        static_cast<jboolean>(result->hit_floor),
        jFloorNote,
        jOutputPath,
        static_cast<jlong>(result->output_bytes),
        jErrMsg,
        static_cast<jlong>(result->duration_ms));

    edgedrop_free_result(result);
    return jResultObj;
}

} // extern "C"
