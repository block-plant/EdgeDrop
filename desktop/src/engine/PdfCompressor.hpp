#pragma once

#include "ICompressor.hpp"

namespace EdgeDrop {

class PdfCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;
};

} // namespace EdgeDrop
