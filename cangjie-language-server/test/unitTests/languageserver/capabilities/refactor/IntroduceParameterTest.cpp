// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/IntroduceParameter.h"
#include "gtest/gtest.h"

using namespace ark;
using namespace Cangjie::AST;

namespace {
OwnedPtr<FuncParamList> MakeParamList(Position rightParenPos)
{
    auto paramList = OwnedPtr<FuncParamList>(new FuncParamList());
    paramList->rightParenPos = rightParenPos;
    return paramList;
}

void SetFuncParamList(FuncDecl &funcDecl, OwnedPtr<FuncParamList> paramList)
{
    auto funcBody = OwnedPtr<FuncBody>(new FuncBody());
    funcBody->paramLists.emplace_back(std::move(paramList));
    funcDecl.funcBody = std::move(funcBody);
}
} // namespace

TEST(IntroduceParameterTest, InsertParameter)
{
    auto emptyParamList = MakeParamList({1, 10, 18});
    FuncDecl emptyFunc;
    SetFuncParamList(emptyFunc, std::move(emptyParamList));
    std::string paramName = "newParameter";
    std::string typeName = "Int64";
    TextEdit emptyEdit = IntroduceParameter::InsertParameter(emptyFunc, paramName, typeName);
    EXPECT_EQ(emptyEdit.newText, "newParameter: Int64");

    auto paramList = MakeParamList({1, 20, 26});
    paramList->params.emplace_back(new FuncParam());
    paramList->params.emplace_back(new FuncParam());
    FuncDecl funcWithParam;
    SetFuncParamList(funcWithParam, std::move(paramList));
    TextEdit appendEdit = IntroduceParameter::InsertParameter(funcWithParam, paramName, typeName);
    EXPECT_EQ(appendEdit.newText, ", newParameter: Int64");

    FuncDecl noBody;
    TextEdit invalidEdit = IntroduceParameter::InsertParameter(noBody, paramName, typeName);
    EXPECT_TRUE(invalidEdit.newText.empty());
}
