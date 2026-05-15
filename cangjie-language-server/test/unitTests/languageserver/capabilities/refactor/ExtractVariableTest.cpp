// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/ExtractVariable.h"
#include "gtest/gtest.h"
#include "cangjie/AST/Node.h"

using namespace ark;
using namespace Cangjie::AST;

namespace {
OwnedPtr<LitConstExpr> MakeLiteral(Position begin, Position end)
{
    auto expr = OwnedPtr<LitConstExpr>(new LitConstExpr());
    expr->begin = begin;
    expr->end = end;
    return expr;
}
} // namespace

TEST(ExtractVariableTest, DealIfExprRecognizesConditionAndElseIf)
{
    IfExpr ifExpr;
    ifExpr.begin = {1, 1, 1};
    ifExpr.end = {1, 5, 1};
    ifExpr.condExpr = MakeLiteral({1, 1, 4}, {1, 1, 12});

    ark::Range condRange{{1, 1, 5}, {1, 1, 10}};
    EXPECT_TRUE(ExtractVariable::DealIfExpr(ifExpr, condRange));

    ark::Range bodyRange{{1, 2, 5}, {1, 2, 10}};
    EXPECT_FALSE(ExtractVariable::DealIfExpr(ifExpr, bodyRange));

    auto elseIf = OwnedPtr<IfExpr>(new IfExpr());
    elseIf->begin = {1, 3, 1};
    elseIf->end = {1, 5, 1};
    elseIf->condExpr = MakeLiteral({1, 3, 8}, {1, 3, 16});
    ifExpr.elseBody = std::move(elseIf);

    ark::Range elseIfCondRange{{1, 3, 9}, {1, 3, 15}};
    EXPECT_TRUE(ExtractVariable::DealIfExpr(ifExpr, elseIfCondRange));
}

TEST(ExtractVariableTest, MatchModifierKeepsStaticModifierFromVarDecl)
{
    VarDecl varDecl;
    std::string modifier;
    ExtractVariable::MatchModifier(varDecl, modifier);
    EXPECT_TRUE(modifier.empty());

    varDecl.EnableAttr(Attribute::STATIC);
    ExtractVariable::MatchModifier(varDecl, modifier);
    EXPECT_EQ(modifier, "static ");
}
