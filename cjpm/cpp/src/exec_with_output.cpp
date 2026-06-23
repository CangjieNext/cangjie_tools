// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#endif

#ifndef _WIN32
extern char** environ;
#endif

extern "C" {
struct CjpmExecOutput {
    int64_t exitCode;
    char* outInfo;
    char* errInfo;
    char* failure;
};

CjpmExecOutput* CJPM_EXEC_RunWithOutput(
    const char* command,
    const char** arguments,
    size_t argCount,
    const char** environments,
    size_t envCount,
    const char* workingDirectory,
    int64_t timeoutNs);

void CJPM_EXEC_FreeOutput(CjpmExecOutput* output);
}

namespace {

struct OutputBuffer {
    std::vector<char> data;
    std::string error;
};

#ifndef _WIN32
void writeAll(int fd, const char* data, size_t size)
{
    size_t written = 0;
    while (written < size) {
        ssize_t res = write(fd, data + written, size - written);
        if (res > 0) {
            written += static_cast<size_t>(res);
            continue;
        }
        if (res < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
}
#endif

char* dupString(const std::string& value)
{
    char* result = static_cast<char*>(std::malloc(value.size() + 1));
    if (result == nullptr) {
        return nullptr;
    }
    if (!value.empty()) {
        std::memcpy(result, value.data(), value.size());
    }
    result[value.size()] = '\0';
    return result;
}

char* dupBuffer(const std::vector<char>& value)
{
    char* result = static_cast<char*>(std::malloc(value.size() + 1));
    if (result == nullptr) {
        return nullptr;
    }
    if (!value.empty()) {
        std::memcpy(result, value.data(), value.size());
    }
    result[value.size()] = '\0';
    return result;
}

CjpmExecOutput* makeOutput(int64_t exitCode, const OutputBuffer& out, const OutputBuffer& err, const std::string& failure)
{
    CjpmExecOutput* result = static_cast<CjpmExecOutput*>(std::malloc(sizeof(CjpmExecOutput)));
    if (result == nullptr) {
        return nullptr;
    }
    result->exitCode = exitCode;
    result->outInfo = dupBuffer(out.data);
    result->errInfo = dupBuffer(err.data);
    result->failure = dupString(failure);
    if (result->outInfo == nullptr || result->errInfo == nullptr || result->failure == nullptr) {
        CJPM_EXEC_FreeOutput(result);
        return nullptr;
    }
    return result;
}

CjpmExecOutput* makeFailure(const std::string& failure)
{
    OutputBuffer out;
    OutputBuffer err;
    return makeOutput(-1, out, err, failure);
}

#ifdef _WIN32

std::string getLastErrorMessage(const std::string& prefix)
{
    DWORD err = GetLastError();
    LPSTR msg = nullptr;
    DWORD len = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&msg),
        0,
        nullptr);
    std::string result = prefix + ": ";
    if (len == 0 || msg == nullptr) {
        result += std::to_string(err);
    } else {
        result += msg;
        LocalFree(msg);
    }
    return result;
}

std::string quoteArg(const char* arg)
{
    std::string value = arg == nullptr ? "" : arg;
    if (value.empty()) {
        return "\"\"";
    }
    bool needsQuote = false;
    for (char ch : value) {
        if (ch == ' ' || ch == '\t' || ch == '"' || ch == '\\') {
            needsQuote = true;
            break;
        }
    }
    if (!needsQuote) {
        return value;
    }

    std::string result = "\"";
    size_t slashCount = 0;
    for (char ch : value) {
        if (ch == '\\') {
            slashCount++;
            continue;
        }
        if (ch == '"') {
            result.append(slashCount * 2 + 1, '\\');
            result.push_back('"');
            slashCount = 0;
            continue;
        }
        if (slashCount > 0) {
            result.append(slashCount, '\\');
            slashCount = 0;
        }
        result.push_back(ch);
    }
    if (slashCount > 0) {
        result.append(slashCount * 2, '\\');
    }
    result.push_back('"');
    return result;
}

std::string buildCommandLine(const char* command, const char** arguments, size_t argCount)
{
    std::string commandLine = quoteArg(command);
    for (size_t i = 0; i < argCount; ++i) {
        commandLine.push_back(' ');
        commandLine += quoteArg(arguments[i]);
    }
    return commandLine;
}

std::string buildEnvironmentBlock(const char** environments, size_t envCount)
{
    std::string result;
    for (size_t i = 0; i < envCount; ++i) {
        if (environments[i] == nullptr) {
            continue;
        }
        result.append(environments[i]);
        result.push_back('\0');
    }
    result.push_back('\0');
    return result;
}

struct ReaderParam {
    HANDLE handle;
    OutputBuffer* output;
};

DWORD WINAPI readPipeThread(LPVOID param)
{
    ReaderParam* reader = static_cast<ReaderParam*>(param);
    char buffer[4096];
    while (true) {
        DWORD readBytes = 0;
        BOOL ok = ReadFile(reader->handle, buffer, sizeof(buffer), &readBytes, nullptr);
        if (!ok || readBytes == 0) {
            if (!ok && GetLastError() != ERROR_BROKEN_PIPE) {
                reader->output->error = getLastErrorMessage("ReadFile failed");
            }
            break;
        }
        reader->output->data.insert(reader->output->data.end(), buffer, buffer + readBytes);
    }
    CloseHandle(reader->handle);
    return 0;
}

CjpmExecOutput* runWithOutput(
    const char* command,
    const char** arguments,
    size_t argCount,
    const char** environments,
    size_t envCount,
    const char* workingDirectory,
    int64_t timeoutNs)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE outRead = nullptr;
    HANDLE outWrite = nullptr;
    HANDLE errRead = nullptr;
    HANDLE errWrite = nullptr;
    if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
        return makeFailure(getLastErrorMessage("CreatePipe stdout failed"));
    }
    if (!SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return makeFailure(getLastErrorMessage("SetHandleInformation stdout failed"));
    }
    if (!CreatePipe(&errRead, &errWrite, &sa, 0)) {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        return makeFailure(getLastErrorMessage("CreatePipe stderr failed"));
    }
    if (!SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(outRead);
        CloseHandle(outWrite);
        CloseHandle(errRead);
        CloseHandle(errWrite);
        return makeFailure(getLastErrorMessage("SetHandleInformation stderr failed"));
    }

    STARTUPINFOA si;
    std::memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = outWrite;
    si.hStdError = errWrite;

    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof(pi));
    std::string commandLine = buildCommandLine(command, arguments, argCount);
    std::string envBlock = buildEnvironmentBlock(environments, envCount);
    LPVOID envBlockPtr = envCount == 0 ? nullptr : static_cast<LPVOID>(&envBlock[0]);
    LPCSTR cwd = (workingDirectory == nullptr || workingDirectory[0] == '\0') ? nullptr : workingDirectory;

    BOOL created = CreateProcessA(
        nullptr,
        &commandLine[0],
        nullptr,
        nullptr,
        TRUE,
        0,
        envBlockPtr,
        cwd,
        &si,
        &pi);

    CloseHandle(outWrite);
    CloseHandle(errWrite);
    if (!created) {
        CloseHandle(outRead);
        CloseHandle(errRead);
        return makeFailure(getLastErrorMessage("CreateProcess failed"));
    }

    OutputBuffer out;
    OutputBuffer err;
    ReaderParam outParam{outRead, &out};
    ReaderParam errParam{errRead, &err};
    HANDLE outThread = CreateThread(nullptr, 0, readPipeThread, &outParam, 0, nullptr);
    HANDLE errThread = CreateThread(nullptr, 0, readPipeThread, &errParam, 0, nullptr);
    if (outThread == nullptr || errThread == nullptr) {
        if (outThread != nullptr) {
            CloseHandle(outThread);
        }
        if (errThread != nullptr) {
            CloseHandle(errThread);
        }
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return makeFailure(getLastErrorMessage("CreateThread failed"));
    }

    DWORD waitTimeout = INFINITE;
    if (timeoutNs >= 0) {
        uint64_t timeoutMs = static_cast<uint64_t>(timeoutNs) / 1000000ULL;
        if (timeoutNs % 1000000LL != 0) {
            timeoutMs++;
        }
        waitTimeout = timeoutMs > static_cast<uint64_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(timeoutMs);
    }

    std::string failure;
    DWORD waitRes = WaitForSingleObject(pi.hProcess, waitTimeout);
    if (waitRes == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        failure = "Process timed out.";
    } else if (waitRes == WAIT_FAILED) {
        failure = getLastErrorMessage("WaitForSingleObject failed");
    }

    WaitForSingleObject(outThread, INFINITE);
    WaitForSingleObject(errThread, INFINITE);
    CloseHandle(outThread);
    CloseHandle(errThread);

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        failure = getLastErrorMessage("GetExitCodeProcess failed");
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (!out.error.empty()) {
        failure += failure.empty() ? out.error : "\n" + out.error;
    }
    if (!err.error.empty()) {
        failure += failure.empty() ? err.error : "\n" + err.error;
    }
    return makeOutput(static_cast<int64_t>(exitCode), out, err, failure);
}

#else

std::string errnoMessage(const std::string& prefix)
{
    return prefix + ": " + std::strerror(errno);
}

struct ReaderParam {
    int fd;
    OutputBuffer* output;
};

void* readPipeThread(void* param)
{
    ReaderParam* reader = static_cast<ReaderParam*>(param);
    char buffer[4096];
    while (true) {
        ssize_t readBytes = read(reader->fd, buffer, sizeof(buffer));
        if (readBytes > 0) {
            reader->output->data.insert(reader->output->data.end(), buffer, buffer + readBytes);
            continue;
        }
        if (readBytes < 0 && errno == EINTR) {
            continue;
        }
        if (readBytes < 0) {
            reader->output->error = errnoMessage("read failed");
        }
        break;
    }
    close(reader->fd);
    return nullptr;
}

std::string findEnvValue(const char** environments, size_t envCount, const char* name)
{
    std::string prefix = std::string(name) + "=";
    if (envCount > 0) {
        for (size_t i = 0; i < envCount; ++i) {
            if (environments[i] == nullptr) {
                continue;
            }
            std::string item = environments[i];
            if (item.compare(0, prefix.size(), prefix) == 0) {
                return item.substr(prefix.size());
            }
        }
        return "";
    }

    const char* value = std::getenv(name);
    return value == nullptr ? "" : value;
}

std::vector<std::string> splitPath(const std::string& path)
{
    std::vector<std::string> result;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find(':', start);
        if (end == std::string::npos) {
            end = path.size();
        }
        result.push_back(path.substr(start, end - start));
        start = end + 1;
        if (end == path.size()) {
            break;
        }
    }
    return result;
}

std::string resolveCommand(const char* command, const char** environments, size_t envCount)
{
    std::string cmd = command == nullptr ? "" : command;
    if (cmd.find('/') != std::string::npos) {
        return cmd;
    }

    std::string path = findEnvValue(environments, envCount, "PATH");
    if (path.empty()) {
        path = "/bin:/usr/bin";
    }
    for (const std::string& dir : splitPath(path)) {
        std::string candidate = dir.empty() ? cmd : dir + "/" + cmd;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate;
        }
    }
    return cmd;
}

CjpmExecOutput* runWithOutput(
    const char* command,
    const char** arguments,
    size_t argCount,
    const char** environments,
    size_t envCount,
    const char* workingDirectory,
    int64_t timeoutNs)
{
    int outPipe[2] = {-1, -1};
    int errPipe[2] = {-1, -1};
    if (pipe(outPipe) != 0) {
        return makeFailure(errnoMessage("pipe stdout failed"));
    }
    if (pipe(errPipe) != 0) {
        close(outPipe[0]);
        close(outPipe[1]);
        return makeFailure(errnoMessage("pipe stderr failed"));
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(outPipe[0]);
        close(outPipe[1]);
        close(errPipe[0]);
        close(errPipe[1]);
        return makeFailure(errnoMessage("fork failed"));
    }

    if (pid == 0) {
        close(outPipe[0]);
        close(errPipe[0]);
        dup2(outPipe[1], STDOUT_FILENO);
        dup2(errPipe[1], STDERR_FILENO);
        close(outPipe[1]);
        close(errPipe[1]);

        if (workingDirectory != nullptr && workingDirectory[0] != '\0' && chdir(workingDirectory) != 0) {
            const char* msg = "failed to change working directory\n";
            writeAll(STDERR_FILENO, msg, std::strlen(msg));
            _exit(127);
        }

        std::vector<char*> argv;
        argv.reserve(argCount + 2);
        argv.push_back(const_cast<char*>(command));
        for (size_t i = 0; i < argCount; ++i) {
            argv.push_back(const_cast<char*>(arguments[i]));
        }
        argv.push_back(nullptr);

        std::vector<char*> envp;
        if (envCount > 0) {
            envp.reserve(envCount + 1);
            for (size_t i = 0; i < envCount; ++i) {
                envp.push_back(const_cast<char*>(environments[i]));
            }
            envp.push_back(nullptr);
        }

        std::string resolved = resolveCommand(command, environments, envCount);
        execve(resolved.c_str(), argv.data(), envCount > 0 ? envp.data() : environ);
        std::string msg = errnoMessage("execve failed") + "\n";
        writeAll(STDERR_FILENO, msg.c_str(), msg.size());
        _exit(127);
    }

    close(outPipe[1]);
    close(errPipe[1]);

    OutputBuffer out;
    OutputBuffer err;
    ReaderParam outParam{outPipe[0], &out};
    ReaderParam errParam{errPipe[0], &err};
    pthread_t outThread;
    pthread_t errThread;
    int outThreadRes = pthread_create(&outThread, nullptr, readPipeThread, &outParam);
    int errThreadRes = pthread_create(&errThread, nullptr, readPipeThread, &errParam);
    if (outThreadRes != 0 || errThreadRes != 0) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        if (outThreadRes == 0) {
            pthread_join(outThread, nullptr);
        } else {
            close(outPipe[0]);
        }
        if (errThreadRes == 0) {
            pthread_join(errThread, nullptr);
        } else {
            close(errPipe[0]);
        }
        return makeFailure("pthread_create failed.");
    }

    int status = 0;
    std::string failure;
    bool timedOut = false;
    auto start = std::chrono::steady_clock::now();
    while (true) {
        pid_t waitRes = waitpid(pid, &status, timeoutNs >= 0 ? WNOHANG : 0);
        if (waitRes == pid) {
            break;
        }
        if (waitRes < 0) {
            if (errno == EINTR) {
                continue;
            }
            failure = errnoMessage("waitpid failed");
            break;
        }
        if (timeoutNs >= 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
            if (elapsed >= timeoutNs) {
                timedOut = true;
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                failure = "Process timed out.";
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    pthread_join(outThread, nullptr);
    pthread_join(errThread, nullptr);

    int64_t exitCode = -1;
    if (WIFEXITED(status)) {
        exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exitCode = timedOut ? -1 : 128 + WTERMSIG(status);
    }
    if (!out.error.empty()) {
        failure += failure.empty() ? out.error : "\n" + out.error;
    }
    if (!err.error.empty()) {
        failure += failure.empty() ? err.error : "\n" + err.error;
    }
    return makeOutput(exitCode, out, err, failure);
}

#endif

} // namespace

extern "C" CjpmExecOutput* CJPM_EXEC_RunWithOutput(
    const char* command,
    const char** arguments,
    size_t argCount,
    const char** environments,
    size_t envCount,
    const char* workingDirectory,
    int64_t timeoutNs)
{
    if (command == nullptr || command[0] == '\0') {
        return makeFailure("Command cannot be empty.");
    }
    return runWithOutput(command, arguments, argCount, environments, envCount, workingDirectory, timeoutNs);
}

extern "C" void CJPM_EXEC_FreeOutput(CjpmExecOutput* output)
{
    if (output == nullptr) {
        return;
    }
    std::free(output->outInfo);
    std::free(output->errInfo);
    std::free(output->failure);
    std::free(output);
}
