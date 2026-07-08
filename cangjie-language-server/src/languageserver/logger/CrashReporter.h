// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef LSPSERVER_CRASHREPORTER_H
#define LSPSERVER_CRASHREPORTER_H

#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#include <csignal>
#endif

namespace ark {
class CrashReporter {
public:
#if defined(__linux__) || defined(__APPLE__)
    static void SignalHandler(int sig, siginfo_t *info, void *context);
#endif
    static void RegisterHandlers();

private:
};
} // namespace ark

#endif // LSPSERVER_CRASHREPORTER_H
