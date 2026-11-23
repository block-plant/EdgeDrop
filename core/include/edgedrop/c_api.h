#pragma once

#include <stdint.h>
#include <stdbool.h>

#if defined(_WIN32)
    #if defined(EDGEDROP_SHARED)
        #if defined(EDGEDROP_EXPORTS)
            #define EDGEDROP_API __declspec(dllexport)
        #else
            #define EDGEDROP_API __declspec(dllimport)
        #endif
    #else
        #define EDGEDROP_API
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define EDGEDROP_API __attribute__((visibility("default")))
#else
    #define EDGEDROP_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool success;
    bool hit_floor;
    const char* floor_note;
    const char* output_path;
    int64_t output_bytes;
    const char* error_message;
    int64_t duration_ms;
} EdgeDropResult;

typedef void (*EdgeDropProgressCallback)(double progress, void* user_data);

EDGEDROP_API int edgedrop_detect_format(const char* file_path);

EDGEDROP_API EdgeDropResult* edgedrop_compress_file(const char* input_path,
                                                   const char* output_dir,
                                                   int64_t target_bytes,
                                                   EdgeDropProgressCallback progress_cb,
                                                   void* user_data,
                                                   bool* cancel_flag);

EDGEDROP_API void edgedrop_cancel(bool* cancel_flag);

EDGEDROP_API void edgedrop_free_result(EdgeDropResult* result);

#ifdef __cplusplus
}
#endif
