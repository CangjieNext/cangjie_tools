// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"

#include<string>
#include<thread>
#include<vector>

#include "common.h"
#include "SingleInstance.h"

using namespace test::common;

namespace TestLspProgress {
    bool LspProgressTest(TestParam param)
    {
        SingleInstance *p = SingleInstance::GetInstance();
        std::string testFile = p->messagePath + "/" + param.testFile;

        std::string rootUri;
        bool isMultiModule = false;

        if (CreateMsg(p->pathIn, testFile, rootUri, isMultiModule) != true) {
            return false;
        }
        if (IsMacroExpandTest(rootUri)) {
            return true;
        }
        if (CreateBuildScript(p->pathBuildScript, testFile)) {
            BuildDynamicBinary(p->pathBuildScript);
        }
        std::thread ThreadObj(StartLspServer, SingleInstance::GetInstance()->useDB);
        ThreadObj.join();

        nlohmann::json result = ReadLastFileByMethod(p->pathOut, "progress/UpdateIndexingProgress");

        if (result.empty()) {
            std::cout << "No progress message found" << std::endl;
            return false;
        }

        if (!result.contains("params")) {
            std::cout << "No params field in progress message" << std::endl;
            return false;
        }

        nlohmann::json params = result["params"];
        int current = params["current"];
        int total = params["total"];

        if (current != total) {
            std::cout << "Progress incomplete: current=" << current << ", total=" << total << std::endl;
            return false;
        }

        return true;
    }

    class ProgressTest : public testing::TestWithParam<struct TestParam> {
    protected:
        void SetUp()
        {
            SingleInstance *p = SingleInstance::GetInstance();
            p->testFolder = "progress";
            p->pathIn = GetRealPath(p->testFolder + "_freopen.in");
            p->pathOut = GetRealPath(p->testFolder + "_freopen.out");
            p->pathPwd = GetPwd();
            p->workPath = GetRootPath(p->pathPwd);
            p->messagePath = p->workPath + "/test/message/" + p->testFolder;
        }
    };
    INSTANTIATE_TEST_SUITE_P(Progress, ProgressTest, testing::ValuesIn(GetTestCaseList("progress")));

    TEST_P(ProgressTest, ProgressCase)
    {
        TestParam param = GetParam();
        ASSERT_TRUE(LspProgressTest(param));
    }
}