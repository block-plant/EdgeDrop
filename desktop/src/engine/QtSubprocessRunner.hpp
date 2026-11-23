#pragma once

#include "edgedrop/interfaces.hpp"
#include <memory>
#include <string>
#include <vector>

namespace EdgeDrop {

class QtSubprocessRunner : public ISubprocessRunner {
public:
    QtSubprocessRunner() = default;
    ~QtSubprocessRunner() override = default;

    SubprocessResult run(const std::string& program,
                         const std::vector<std::string>& args,
                         ProgressCallback progress = nullptr,
                         CancelToken cancel = nullptr) override;

    static bool isTrustedExecutable(const std::string& path, std::string& resolvedPath);
};

} // namespace EdgeDrop
