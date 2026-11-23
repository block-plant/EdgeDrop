#pragma once

#include "interfaces.hpp"
#include <memory>

namespace EdgeDrop {

class PdfCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;

    static void setSubprocessRunner(std::shared_ptr<ISubprocessRunner> runner);
};

} // namespace EdgeDrop
