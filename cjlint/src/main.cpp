// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <common/CommonFunc.h>
#include <regex>
#include <string>
#include <vector>
#include "Cjlint.h"
#ifdef _WIN32
#include "checker/Checker.h"
#include <cstdlib>
#endif

using namespace Cangjie;
using namespace Cangjie::CodeCheck;
using namespace CjLint;

static const std::unordered_map<std::string, std::function<void(ParamsInCJLint&, char*)>> optionMap = {
    {"-o", [](ParamsInCJLint& params, char* optarg) { params.reportFile = optarg; }},
    {"-r", [](ParamsInCJLint& params, char* optarg) { params.reportFormat = optarg; }},
    {"-c", [](ParamsInCJLint& params, char* optarg) { params.configFileDir = optarg; }},
    {"-f", [](ParamsInCJLint& params, char* optarg) { params.srcFileDir = optarg; }},
    {"-m", [](ParamsInCJLint& params, char* optarg) { params.modulesDir = optarg; }},
    {"-e", [](ParamsInCJLint& params, char* optarg) { params.excludeRule = optarg; }},
    {"--import-path", [](ParamsInCJLint& params, char* optarg) { params.cjoPath = optarg; }}};

int main(int argc, char **argv, const char **envp)
{
    if (argc == 1) {
        PrintHelp();
        return OK;
    }

#ifdef _WIN32
    // Check if running as worker process (for process isolation)
    // New format: --cjlint-worker <config_read_handle> <result_write_handle>
    if (argc >= 4 && std::string(argv[1]) == "--cjlint-worker") {
        // Parse the two pipe handles
        uintptr_t configReadHandle = std::strtoull(argv[2], nullptr, 10);
        uintptr_t resultWriteHandle = std::strtoull(argv[3], nullptr, 10);

        // Directly call WorkerMain - it reads config from pipe, no need for command line args
        int result = Checker::WorkerMain(
            reinterpret_cast<void*>(configReadHandle),
            reinterpret_cast<void*>(resultWriteHandle));
        _exit(result);
        return result;
    }
#endif

    ParamsInCJLint params;
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "-v") {
            PrintVersion();
            return OK;
        }
        if (arg == "-h") {
            PrintHelp();
            return OK;
        }
        auto handleOption = optionMap.find(arg);
        if (handleOption == optionMap.end()) {
            Errorln("Illegal option: ", arg);
            Println("Try: 'cjlint -h' for more information.");
            return ERR;
        }
        if (i + 1 >= argc || optionMap.find(argv[i + 1]) != optionMap.end()) {
            Errorln("Option that requires an argument: ", arg);
            return ERR;
        }
        handleOption->second(params, argv[i + 1]);
        // Jump to the next option ('2' means option and argument)
        i += 2;
    }
    int errCode = CJLint(params, envp);
    _exit(errCode);
    return OK;
}
