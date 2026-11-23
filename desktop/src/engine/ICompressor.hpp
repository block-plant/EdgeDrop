#pragma once

#include "ProcessingTypes.hpp"
#include <functional>
#include <atomic>

namespace EdgeDrop {

using ProgressCallback = std::function<void(double)>; // 0.0–1.0
using CancelToken      = std::atomic<bool>*;

class ICompressor {
public:
    virtual ~ICompressor() = default;
    virtual bool canHandle(DetectedFormat fmt) const = 0;
    virtual JobResult compress(const JobRequest& req,
                               ProgressCallback progress,
                               CancelToken cancel) = 0;
};

} // namespace EdgeDrop
