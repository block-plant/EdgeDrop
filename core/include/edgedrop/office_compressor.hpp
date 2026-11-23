#pragma once

#include "interfaces.hpp"

namespace EdgeDrop {

class OfficeCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;
};

} // namespace EdgeDrop
