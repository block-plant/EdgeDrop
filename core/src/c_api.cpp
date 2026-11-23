#include "edgedrop/c_api.h"
#include "edgedrop/format_detector.hpp"
#include "edgedrop/compressor_router.hpp"

#include <cstring>
#include <cstdlib>

static char* duplicateString(const std::string& str) {
    if (str.empty()) return nullptr;
    char* copy = static_cast<char*>(malloc(str.size() + 1));
    if (copy) {
        std::memcpy(copy, str.c_str(), str.size() + 1);
    }
    return copy;
}

extern "C" {

int edgedrop_detect_format(const char* file_path) {
    if (!file_path) return static_cast<int>(EdgeDrop::DetectedFormat::Unknown);
    return static_cast<int>(EdgeDrop::FormatDetector::detect(file_path));
}

EdgeDropResult* edgedrop_compress_file(const char* input_path,
                                       const char* output_dir,
                                       int64_t target_bytes,
                                       EdgeDropProgressCallback progress_cb,
                                       void* user_data,
                                       bool* cancel_flag) {
    if (!input_path) return nullptr;

    EdgeDrop::JobRequest req;
    req.inputFilePath = input_path;
    if (output_dir) req.outputDirectory = output_dir;
    req.targetBytes = target_bytes;

    EdgeDrop::ProgressCallback cb = nullptr;
    if (progress_cb) {
        cb = [progress_cb, user_data](double p) {
            progress_cb(p, user_data);
        };
    }

    std::atomic<bool> atomicCancel{false};
    EdgeDrop::CancelToken pCancel = nullptr;
    if (cancel_flag) {
        atomicCancel.store(*cancel_flag);
        pCancel = &atomicCancel;
    }

    EdgeDrop::JobResult res = EdgeDrop::CompressorRouter::route(req, cb, pCancel);

    auto* out = static_cast<EdgeDropResult*>(malloc(sizeof(EdgeDropResult)));
    if (!out) return nullptr;

    out->success = res.success;
    out->hit_floor = res.hitFloor;
    out->floor_note = duplicateString(res.floorNote);
    out->output_path = duplicateString(res.outputFilePath.string());
    out->output_bytes = res.outputSizeBytes;
    out->error_message = duplicateString(res.errorMessage);
    out->duration_ms = res.durationMs;

    return out;
}

void edgedrop_cancel(bool* cancel_flag) {
    if (cancel_flag) {
        *cancel_flag = true;
    }
}

void edgedrop_free_result(EdgeDropResult* result) {
    if (!result) return;
    if (result->floor_note) free(const_cast<char*>(result->floor_note));
    if (result->output_path) free(const_cast<char*>(result->output_path));
    if (result->error_message) free(const_cast<char*>(result->error_message));
    free(result);
}

}
