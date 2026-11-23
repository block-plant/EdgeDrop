#include <jni.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <android/log.h>

#include "edgedrop/format_detector.hpp"
#include "edgedrop/security_guards.hpp"
#include "edgedrop/types.hpp"

#define TAG "EdgeDropJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

extern "C" {

JNIEXPORT jint JNICALL
Java_com_edgedrop_core_EdgeDropCore_detectFormat(JNIEnv* env, jclass /*clazz*/, jstring jFilePath) {
    if (!jFilePath) return static_cast<jint>(EdgeDrop::DetectedFormat::Unknown);
    const char* pathStr = env->GetStringUTFChars(jFilePath, nullptr);
    if (!pathStr) return static_cast<jint>(EdgeDrop::DetectedFormat::Unknown);

    auto format = EdgeDrop::FormatDetector::detect(std::filesystem::path(pathStr));
    env->ReleaseStringUTFChars(jFilePath, pathStr);
    return static_cast<jint>(format);
}

JNIEXPORT jstring JNICALL
Java_com_edgedrop_core_EdgeDropCore_getFormatName(JNIEnv* env, jclass /*clazz*/, jint jFormat) {
    auto fmt = static_cast<EdgeDrop::DetectedFormat>(jFormat);
    std::string name = EdgeDrop::FormatDetector::formatName(fmt);
    return env->NewStringUTF(name.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_edgedrop_core_EdgeDropCore_getFormatLabel(JNIEnv* env, jclass /*clazz*/, jint jFormat, jstring jFilePath) {
    auto fmt = static_cast<EdgeDrop::DetectedFormat>(jFormat);
    std::filesystem::path path;
    if (jFilePath) {
        const char* p = env->GetStringUTFChars(jFilePath, nullptr);
        if (p) {
            path = p;
            env->ReleaseStringUTFChars(jFilePath, p);
        }
    }
    std::string label = EdgeDrop::FormatDetector::formatLabel(fmt, path);
    return env->NewStringUTF(label.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_edgedrop_core_EdgeDropCore_validateInputPath(JNIEnv* env, jclass /*clazz*/, jstring jFilePath) {
    if (!jFilePath) return env->NewStringUTF("File path is null.");
    const char* pathStr = env->GetStringUTFChars(jFilePath, nullptr);
    if (!pathStr) return env->NewStringUTF("Invalid path encoding.");

    std::string err = EdgeDrop::SecurityGuards::validateInputPath(std::filesystem::path(pathStr));
    env->ReleaseStringUTFChars(jFilePath, pathStr);
    return env->NewStringUTF(err.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_edgedrop_core_EdgeDropCore_sanitizeFilename(JNIEnv* env, jclass /*clazz*/, jstring jName) {
    if (!jName) return env->NewStringUTF("file");
    const char* nameStr = env->GetStringUTFChars(jName, nullptr);
    if (!nameStr) return env->NewStringUTF("file");

    std::string safe = EdgeDrop::SecurityGuards::sanitizeFilename(nameStr);
    env->ReleaseStringUTFChars(jName, nameStr);
    return env->NewStringUTF(safe.c_str());
}

JNIEXPORT jbyteArray JNICALL
Java_com_edgedrop_core_EdgeDropCore_stripJpegMetadata(JNIEnv* env, jclass /*clazz*/, jbyteArray jData) {
    if (!jData) return nullptr;
    jsize len = env->GetArrayLength(jData);
    if (len <= 0) return jData;

    std::vector<uint8_t> buffer(len);
    env->GetByteArrayRegion(jData, 0, len, reinterpret_cast<jbyte*>(buffer.data()));

    EdgeDrop::SecurityGuards::stripJpegPrivacyMetadata(buffer);

    jbyteArray outArray = env->NewByteArray(static_cast<jsize>(buffer.size()));
    if (!outArray) return nullptr;

    env->SetByteArrayRegion(outArray, 0, static_cast<jsize>(buffer.size()), reinterpret_cast<const jbyte*>(buffer.data()));
    return outArray;
}

JNIEXPORT jboolean JNICALL
Java_com_edgedrop_core_EdgeDropCore_stripJpegMetadataFile(JNIEnv* env, jclass /*clazz*/, jstring jFilePath) {
    if (!jFilePath) return JNI_FALSE;
    const char* pathStr = env->GetStringUTFChars(jFilePath, nullptr);
    if (!pathStr) return JNI_FALSE;

    std::filesystem::path path(pathStr);
    env->ReleaseStringUTFChars(jFilePath, pathStr);

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return JNI_FALSE;

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    EdgeDrop::SecurityGuards::stripJpegPrivacyMetadata(buffer);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return JNI_FALSE;
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    out.close();

    return JNI_TRUE;
}

JNIEXPORT jlong JNICALL
Java_com_edgedrop_core_EdgeDropCore_createCancelToken(JNIEnv* /*env*/, jclass /*clazz*/) {
    auto* flag = new std::atomic<bool>(false);
    return reinterpret_cast<jlong>(flag);
}

JNIEXPORT void JNICALL
Java_com_edgedrop_core_EdgeDropCore_cancelJob(JNIEnv* /*env*/, jclass /*clazz*/, jlong cancelHandle) {
    if (cancelHandle != 0) {
        auto* flag = reinterpret_cast<std::atomic<bool>*>(cancelHandle);
        flag->store(true);
    }
}

JNIEXPORT void JNICALL
Java_com_edgedrop_core_EdgeDropCore_freeCancelToken(JNIEnv* /*env*/, jclass /*clazz*/, jlong cancelHandle) {
    if (cancelHandle != 0) {
        auto* flag = reinterpret_cast<std::atomic<bool>*>(cancelHandle);
        delete flag;
    }
}

} // extern "C"
