#include "default_subprocess_runner.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>

namespace EdgeDrop {

#ifdef _WIN32

SubprocessResult DefaultSubprocessRunner::run(const std::string& program,
                                             const std::vector<std::string>& args,
                                             ProgressCallback progress,
                                             CancelToken cancel) {
    SubprocessResult res;
    std::string cmdLine = "\"" + program + "\"";
    for (const auto& a : args) {
        cmdLine += " \"" + a + "\"";
    }

    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0) ||
        !CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        res.exitCode = -1;
        res.stdErr = "CreatePipe failed";
        return res;
    }
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdVec(cmdLine.begin(), cmdLine.end());
    cmdVec.push_back('\0');

    if (!CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hStdOutRead); CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead); CloseHandle(hStdErrWrite);
        res.exitCode = -1;
        res.stdErr = "CreateProcess failed: " + program;
        return res;
    }
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    char buf[4096];
    DWORD bytesRead;
    while (true) {
        if (cancel && cancel->load()) {
            TerminateProcess(pi.hProcess, 1);
            res.cancelled = true;
            break;
        }

        DWORD avail = 0;
        if (PeekNamedPipe(hStdOutRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            if (ReadFile(hStdOutRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                res.stdOut.append(buf, bytesRead);
            }
        }
        if (PeekNamedPipe(hStdErrRead, NULL, 0, NULL, &avail, NULL) && avail > 0) {
            if (ReadFile(hStdErrRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                res.stdErr.append(buf, bytesRead);
            }
        }

        DWORD waitRes = WaitForSingleObject(pi.hProcess, 50);
        if (waitRes == WAIT_OBJECT_0) {
            // Read remaining data
            while (ReadFile(hStdOutRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                res.stdOut.append(buf, bytesRead);
            }
            while (ReadFile(hStdErrRead, buf, sizeof(buf) - 1, &bytesRead, NULL) && bytesRead > 0) {
                res.stdErr.append(buf, bytesRead);
            }
            break;
        }
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    res.exitCode = static_cast<int>(exitCode);

    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return res;
}

#else

SubprocessResult DefaultSubprocessRunner::run(const std::string& program,
                                             const std::vector<std::string>& args,
                                             ProgressCallback progress,
                                             CancelToken cancel) {
    SubprocessResult res;
    int outPipe[2];
    int errPipe[2];
    if (pipe(outPipe) != 0 || pipe(errPipe) != 0) {
        res.exitCode = -1;
        res.stdErr = "pipe() failed";
        return res;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);
        res.exitCode = -1;
        res.stdErr = "fork() failed";
        return res;
    }

    if (pid == 0) {
        // Child
        close(outPipe[0]);
        close(errPipe[0]);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);
        close(outPipe[1]);
        close(errPipe[1]);

        std::vector<const char*> argv;
        argv.push_back(program.c_str());
        for (const auto& a : args) {
            argv.push_back(a.c_str());
        }
        argv.push_back(nullptr);

        execvp(program.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    // Parent
    close(outPipe[1]);
    close(errPipe[1]);

    // Set non-blocking on read pipes
    fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

    char buf[4096];
    bool childRunning = true;
    while (childRunning) {
        if (cancel && cancel->load()) {
            kill(pid, SIGTERM);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            kill(pid, SIGKILL);
            res.cancelled = true;
            childRunning = false;
            break;
        }

        ssize_t nOut = read(outPipe[0], buf, sizeof(buf) - 1);
        if (nOut > 0) {
            res.stdOut.append(buf, nOut);
        }

        ssize_t nErr = read(errPipe[0], buf, sizeof(buf) - 1);
        if (nErr > 0) {
            res.stdErr.append(buf, nErr);
        }

        int status = 0;
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            if (WIFEXITED(status)) {
                res.exitCode = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                res.exitCode = 128 + WTERMSIG(status);
            }
            childRunning = false;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Drain remaining output
    ssize_t n;
    while ((n = read(outPipe[0], buf, sizeof(buf) - 1)) > 0) {
        res.stdOut.append(buf, n);
    }
    while ((n = read(errPipe[0], buf, sizeof(buf) - 1)) > 0) {
        res.stdErr.append(buf, n);
    }

    close(outPipe[0]);
    close(errPipe[0]);
    return res;
}

#endif

} // namespace EdgeDrop
