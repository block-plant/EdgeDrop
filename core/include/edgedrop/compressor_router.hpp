#pragma once

#include "interfaces.hpp"
#include <memory>

namespace EdgeDrop {

class CompressorRouter {
public:
    static JobResult route(const JobRequest& req,
                           ProgressCallback cb = nullptr,
                           CancelToken cancel = nullptr);

    static void setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner);
    static void setNativeImageCodec(std::shared_ptr<INativeImageCodec> codec);
};

} // namespace EdgeDrop
