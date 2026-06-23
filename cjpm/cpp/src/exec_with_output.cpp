// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <cstdint>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

extern "C" {
using CjpmPipeReadCallback = void (*)(int64_t);

void* CJPM_EXEC_StartPipeReader(int64_t readerId, CjpmPipeReadCallback callback);

bool CJPM_EXEC_JoinPipeReader(void* reader);

void CJPM_EXEC_FreePipeReader(void* reader);
}

namespace {

struct PipeReader {
    int64_t readerId;
    CjpmPipeReadCallback callback;
    bool started;
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
};

#ifdef _WIN32
DWORD WINAPI pipeReaderMain(LPVOID data)
{
    PipeReader* reader = static_cast<PipeReader*>(data);
    if (reader != nullptr && reader->callback != nullptr) {
        reader->callback(reader->readerId);
    }
    return 0;
}
#else
void* pipeReaderMain(void* data)
{
    PipeReader* reader = static_cast<PipeReader*>(data);
    if (reader != nullptr && reader->callback != nullptr) {
        reader->callback(reader->readerId);
    }
    return nullptr;
}
#endif

} // namespace

extern "C" void* CJPM_EXEC_StartPipeReader(int64_t readerId, CjpmPipeReadCallback callback)
{
    if (callback == nullptr) {
        return nullptr;
    }

    PipeReader* reader = static_cast<PipeReader*>(std::malloc(sizeof(PipeReader)));
    if (reader == nullptr) {
        return nullptr;
    }
    reader->readerId = readerId;
    reader->callback = callback;
    reader->started = false;

#ifdef _WIN32
    reader->thread = CreateThread(nullptr, 0, pipeReaderMain, reader, 0, nullptr);
    if (reader->thread == nullptr) {
        std::free(reader);
        return nullptr;
    }
#else
    if (pthread_create(&reader->thread, nullptr, pipeReaderMain, reader) != 0) {
        std::free(reader);
        return nullptr;
    }
#endif
    reader->started = true;
    return reader;
}

extern "C" bool CJPM_EXEC_JoinPipeReader(void* data)
{
    PipeReader* reader = static_cast<PipeReader*>(data);
    if (reader == nullptr || !reader->started) {
        return false;
    }

#ifdef _WIN32
    DWORD waitResult = WaitForSingleObject(reader->thread, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        return false;
    }
    CloseHandle(reader->thread);
    reader->thread = nullptr;
#else
    if (pthread_join(reader->thread, nullptr) != 0) {
        return false;
    }
#endif
    reader->started = false;
    return true;
}

extern "C" void CJPM_EXEC_FreePipeReader(void* data)
{
    PipeReader* reader = static_cast<PipeReader*>(data);
    if (reader == nullptr) {
        return;
    }
#ifdef _WIN32
    if (reader->thread != nullptr) {
        CloseHandle(reader->thread);
    }
#endif
    std::free(reader);
}
