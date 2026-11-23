#pragma once

#include "ICompressor.hpp"

namespace EdgeDrop {

class CompressorRouter {
public:
    static JobResult route(const JobRequest& req,
                           ProgressCallback cb,
                           CancelToken cancel);
};

} // namespace EdgeDrop
