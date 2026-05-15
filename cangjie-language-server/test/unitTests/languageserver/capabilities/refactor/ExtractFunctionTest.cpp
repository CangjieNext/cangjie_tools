// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/ExtractFunction.h"
#include "gtest/gtest.h"
#include <optional>
#include <string>

using namespace ark;

TEST(ExtractFunctionTest, DeclarationBuildsModifiersGenericsParamsAndReturnBody)
{
    ExtractedFunction function;
    function.name = "extracted";
    function.isStatic = true;
    function.isConst = true;
    function.isMut = true;
    function.generics.insert("T");
    function.generics.insert("R");
    function.params.insert(ExtractedFunction::Param{"value", "T", std::nullopt, true});
    function.params.insert(ExtractedFunction::Param{"count", "Int64", std::string("countParam"), false});
    function.body = ": R {\n    return value\n}";

    std::string declaration = function.GetFunctionDeclaration();
    EXPECT_TRUE(declaration.find("static const mut func extracted<") == 0);
    EXPECT_TRUE(declaration.find("T") != std::string::npos);
    EXPECT_TRUE(declaration.find("R") != std::string::npos);
    EXPECT_TRUE(declaration.find("value: T") != std::string::npos);
    EXPECT_TRUE(declaration.find("countParam: Int64") != std::string::npos);
    EXPECT_TRUE(declaration.find(": R {\n    return value\n}") != std::string::npos);

    ExtractedFunction::Param left{"a", "Int64", std::nullopt, false};
    ExtractedFunction::Param same{"a", "Int64", std::nullopt, false};
    ExtractedFunction::Param right{"b", "Int64", std::nullopt, false};
    EXPECT_TRUE(left == same);
    EXPECT_FALSE(left != same);
    EXPECT_TRUE(left < right);
}

TEST(ExtractFunctionTest, DeclarationHandlesEmptyAndReturnValueBodies)
{
    ExtractedFunction empty;
    empty.name = "plain";
    empty.body = "{\n}";
    EXPECT_EQ(empty.GetFunctionDeclaration(), "func plain() {\n}");

    ExtractedFunction withReturn;
    withReturn.name = "value";
    withReturn.params.insert(ExtractedFunction::Param{"source", "String", std::nullopt, false});
    withReturn.returnValue = ExtractedFunction::ReturnValue{"source", true, false};
    withReturn.body = ": String {\n    return source\n}";
    EXPECT_EQ(withReturn.GetFunctionDeclaration(), "func value(source: String) : String {\n    return source\n}");
}
