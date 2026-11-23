#pragma once

#include "interfaces.hpp"
#include <string>
#include <memory>

namespace EdgeDrop {

class MediaCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;

    static void setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner);
    static std::string locateFfmpeg();
    static std::string locateFfprobe();
};

} // namespace EdgeDrop
