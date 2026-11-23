#pragma once

#include "ICompressor.hpp"

namespace EdgeDrop {

class MediaCompressor : public ICompressor {
public:
    bool canHandle(DetectedFormat fmt) const override;
    JobResult compress(const JobRequest& req,
                       ProgressCallback progress,
                       CancelToken cancel) override;

private:
    static QString locateFfmpeg();
    static QString locateFfprobe();
};

} // namespace EdgeDrop
