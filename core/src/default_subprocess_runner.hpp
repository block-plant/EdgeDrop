#pragma once

#include "edgedrop/interfaces.hpp"

namespace EdgeDrop {

class DefaultSubprocessRunner : public ISubprocessRunner {
public:
    SubprocessResult run(const std::string& program,
                         const std::vector<std::string>& args,
                         ProgressCallback progress = nullptr,
                         CancelToken cancel = nullptr) override;
};

} // namespace EdgeDrop
