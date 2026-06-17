// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/IntroduceParameter.h"
#include "ArkAST.h"
#include "capabilities/refactor/TweakUtils.h"
#include "gtest/gtest.h"
#include <memory>

using namespace ark;
using namespace Cangjie::AST;

namespace ark {
bool IntroducedParameterTypeBreaksPublicSignature(Cangjie::AST::FuncDecl &funcDecl, Ptr<Cangjie::AST::Ty> ty);
} // namespace ark

namespace {
template <typename T> OwnedPtr<T> MakeOwner()
{
    return OwnedPtr<T>(new T());
}

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

OwnedPtr<LitConstExpr> MakeIntLiteral(Position begin, Position end)
{
    auto expr = MakeOwner<LitConstExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
    return expr;
}

OwnedPtr<BinaryExpr> MakeAddExpr(OwnedPtr<Expr> leftExpr, OwnedPtr<Expr> rightExpr, Position begin, Position end)
{
    auto expr = MakeOwner<BinaryExpr>();
    expr->leftExpr = std::move(leftExpr);
    expr->rightExpr = std::move(rightExpr);
    expr->begin = begin;
    expr->end = end;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
    return expr;
}

OwnedPtr<RefExpr> MakeParamRef(FuncParam &param, Position begin, Position end)
{
    auto expr = MakeOwner<RefExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->ref.target = &param;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
    return expr;
}

template <typename Callback>
void RunWithSelection(OwnedPtr<File> file, ark::Range &selectedRange, Callback callback)
{
    DiagnosticEngine diagEngine;
    auto package = MakeOwner<Package>();
    file->curPackage = package.get();
    file->filePath = "test.cj";
    package->files.emplace_back(std::move(file));

    std::pair<std::string, std::string> paths = {"test.cj", ""};
    ArkAST arkAst(paths, package->files.back().get(), diagEngine, nullptr, nullptr);
    ASSERT_TRUE(SelectionTree::CreateEach(
        arkAst, "test.cj", selectedRange.start, selectedRange.end, [&](SelectionTree selectionTree) {
            Tweak::Selection sel(arkAst, selectedRange, std::move(selectionTree), {});
            callback(sel);
            return true;
        }));
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
    EXPECT_EQ(emptyEdit.range.start.line, 9);
    EXPECT_EQ(emptyEdit.range.start.column, 17);
    EXPECT_EQ(emptyEdit.range.end.line, 9);
    EXPECT_EQ(emptyEdit.range.end.column, 17);

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

TEST(IntroduceParameterTest, InsertParameterHandlesDifferentNamesTypesAndPositions)
{
    auto paramList = MakeParamList({2, 3, 4});
    FuncDecl funcDecl;
    SetFuncParamList(funcDecl, std::move(paramList));

    std::string unitName = "done";
    std::string unitType = "Unit";
    TextEdit unitEdit = IntroduceParameter::InsertParameter(funcDecl, unitName, unitType);
    EXPECT_EQ(unitEdit.newText, "done: Unit");
    EXPECT_EQ(unitEdit.range.start.line, 2);
    EXPECT_EQ(unitEdit.range.start.column, 3);

    auto existingParams = MakeParamList({2, 8, 12});
    auto existingParam = OwnedPtr<FuncParam>(new FuncParam());
    existingParam->identifier = "old";
    existingParams->params.emplace_back(std::move(existingParam));
    FuncDecl funcWithExistingParam;
    SetFuncParamList(funcWithExistingParam, std::move(existingParams));

    std::string stringName = "text";
    std::string stringType = "String";
    TextEdit stringEdit = IntroduceParameter::InsertParameter(funcWithExistingParam, stringName, stringType);
    EXPECT_EQ(stringEdit.newText, ", text: String");
    EXPECT_EQ(stringEdit.range.start.line, 7);
    EXPECT_EQ(stringEdit.range.start.column, 11);
}

TEST(IntroduceParameterTest, InsertParameterAppendsNamedParameterAfterPositionalParameters)
{
    auto mixedParams = MakeParamList({3, 25, 38});
    auto namedParam = OwnedPtr<FuncParam>(new FuncParam());
    namedParam->identifier = "factor";
    namedParam->isNamedParam = true;
    namedParam->begin = {3, 25, 20};
    namedParam->end = {3, 25, 35};
    mixedParams->params.emplace_back(std::move(namedParam));

    auto positionalParam = OwnedPtr<FuncParam>(new FuncParam());
    positionalParam->identifier = "base";
    positionalParam->begin = {3, 25, 37};
    positionalParam->end = {3, 25, 48};
    mixedParams->params.emplace_back(std::move(positionalParam));

    FuncDecl funcWithMixedParams;
    SetFuncParamList(funcWithMixedParams, std::move(mixedParams));
    std::string paramName = "newParam";
    std::string typeName = "Int64";

    TextEdit insertEdit = IntroduceParameter::InsertParameter(funcWithMixedParams, paramName, typeName);
    EXPECT_EQ(insertEdit.newText, ", newParam!: Int64");
    EXPECT_EQ(insertEdit.range.start.line, 24);
    EXPECT_EQ(insertEdit.range.start.column, 37);
    EXPECT_EQ(insertEdit.range.end.line, 24);
    EXPECT_EQ(insertEdit.range.end.column, 37);
}

TEST(IntroduceParameterTest, InsertParameterAppendsNamedParameterWhenAllExistingParamsAreNamed)
{
    auto namedParams = MakeParamList({4, 30, 32});
    auto namedParam = OwnedPtr<FuncParam>(new FuncParam());
    namedParam->identifier = "factor";
    namedParam->isNamedParam = true;
    namedParams->params.emplace_back(std::move(namedParam));

    FuncDecl funcWithNamedParam;
    SetFuncParamList(funcWithNamedParam, std::move(namedParams));
    std::string paramName = "newParam";
    std::string typeName = "Bool";

    TextEdit insertEdit = IntroduceParameter::InsertParameter(funcWithNamedParam, paramName, typeName);
    EXPECT_EQ(insertEdit.newText, ", newParam!: Bool");
    EXPECT_EQ(insertEdit.range.start.line, 29);
    EXPECT_EQ(insertEdit.range.start.column, 31);
}

TEST(IntroduceParameterTest, InsertParameterRejectsMissingParamList)
{
    FuncDecl missingBody;
    std::string paramName = "value";
    std::string typeName = "Float64";
    EXPECT_TRUE(IntroduceParameter::InsertParameter(missingBody, paramName, typeName).newText.empty());

    FuncDecl emptyBody;
    emptyBody.funcBody = OwnedPtr<FuncBody>(new FuncBody());
    EXPECT_TRUE(IntroduceParameter::InsertParameter(emptyBody, paramName, typeName).newText.empty());

    FuncDecl nullParamList;
    nullParamList.funcBody = OwnedPtr<FuncBody>(new FuncBody());
    nullParamList.funcBody->paramLists.emplace_back(nullptr);
    EXPECT_TRUE(IntroduceParameter::InsertParameter(nullParamList, paramName, typeName).newText.empty());
}

TEST(IntroduceParameterTest, ExtraOptionsReturnStoredOptions)
{
    IntroduceParameter introduceParameter;
    EXPECT_TRUE(introduceParameter.ExtraOptions().empty());

    introduceParameter.extraOptions["ErrorCode"] = "2";
    introduceParameter.extraOptions["suggestName"] = "extracted";

    auto options = introduceParameter.ExtraOptions();
    EXPECT_EQ(options.at("ErrorCode"), "2");
    EXPECT_EQ(options.at("suggestName"), "extracted");

    options.erase("ErrorCode");
    EXPECT_EQ(introduceParameter.ExtraOptions().count("ErrorCode"), 1);
}

TEST(IntroduceParameterTest, CompleteExprRangeUsesSelectedSubExpression)
{
    ark::Range selectedRange{{1, 2, 33}, {1, 2, 38}};
    auto file = MakeOwner<File>();
    auto funcDecl = MakeOwner<FuncDecl>();
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody = MakeOwner<FuncBody>();
    funcDecl->funcBody->body = MakeOwner<Block>();
    funcDecl->funcBody->body->begin = {1, 1, 69};
    funcDecl->funcBody->body->end = {1, 3, 1};

    auto leftExpr = MakeIntLiteral({1, 2, 5}, {1, 2, 32});
    auto selectedExpr = MakeAddExpr(
        MakeIntLiteral({1, 2, 33}, {1, 2, 34}), MakeIntLiteral({1, 2, 37}, {1, 2, 38}),
        selectedRange.start, selectedRange.end);
    auto fullExpr = MakeAddExpr(std::move(leftExpr), std::move(selectedExpr), {1, 2, 5}, {1, 2, 38});
    funcDecl->funcBody->body->body.emplace_back(std::move(fullExpr));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        const auto *exprNode = TweakUtils::GetCompleteExprNode(sel.selectionTree, selectedRange);
        ASSERT_NE(exprNode, nullptr);
        ASSERT_NE(exprNode->node, nullptr);
        EXPECT_EQ(exprNode->node->begin, selectedRange.start);
        EXPECT_EQ(exprNode->node->end, selectedRange.end);
        EXPECT_EQ(TweakUtils::GetSelectedExprTypeName(sel.selectionTree, selectedRange), "Int64");

        IntroduceParameter introduceParameter;
        EXPECT_TRUE(introduceParameter.Prepare(sel));
        EXPECT_EQ(introduceParameter.ExtraOptions().count("ErrorCode"), 0);
    });
}

TEST(IntroduceParameterTest, SelectedTrailingBinaryExprRemovesOnlyUsedParameter)
{
    ark::Range selectedRange{{1, 2, 25}, {1, 2, 30}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto funcDecl = MakeOwner<FuncDecl>();
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody = MakeOwner<FuncBody>();
    auto paramList = MakeParamList({1, 1, 75});
    std::vector<Ptr<FuncParam>> params;
    const std::vector<std::string> names = {"a", "b", "c", "d", "e"};
    const std::vector<Position> begins = {{1, 1, 20}, {1, 1, 30}, {1, 1, 40}, {1, 1, 50}, {1, 1, 60}};
    const std::vector<Position> ends = {{1, 1, 28}, {1, 1, 38}, {1, 1, 48}, {1, 1, 58}, {1, 1, 68}};
    for (size_t i = 0; i < names.size(); ++i) {
        auto param = MakeOwner<FuncParam>();
        param->identifier = names[i];
        param->begin = begins[i];
        param->end = ends[i];
        param->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
        params.push_back(param.get());
        paramList->params.emplace_back(std::move(param));
    }
    funcDecl->funcBody->paramLists.emplace_back(std::move(paramList));
    funcDecl->funcBody->body = MakeOwner<Block>();
    funcDecl->funcBody->body->begin = {1, 1, 84};
    funcDecl->funcBody->body->end = {1, 3, 1};

    auto selectedExpr = MakeAddExpr(MakeParamRef(*params[4], {1, 2, 25}, {1, 2, 26}),
        MakeIntLiteral({1, 2, 29}, {1, 2, 30}), selectedRange.start, selectedRange.end);
    auto fullExpr = MakeAddExpr(MakeParamRef(*params[0], {1, 2, 1}, {1, 2, 2}), std::move(selectedExpr),
        {1, 2, 1}, selectedRange.end);
    funcDecl->funcBody->body->body.emplace_back(std::move(fullExpr));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceParameter introduceParameter;
        EXPECT_TRUE(introduceParameter.Prepare(sel));
        sel.extraOptions["suggestName"] = "newParameter";
        effect = introduceParameter.Apply(sel);
    });

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newParameter: Int64";
    }));
    EXPECT_FALSE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == ", newParameter: Int64";
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newParameter" &&
            edit.range.start.line == 1 &&
            edit.range.start.column == 24 &&
            edit.range.end.line == 1 &&
            edit.range.end.column == 29;
    }));
}

TEST(IntroduceParameterTest, PublicFunctionRejectsInternalIntroducedParameterType)
{
    FuncDecl publicFunc;
    publicFunc.EnableAttr(Attribute::PUBLIC);

    EnumDecl internalEnum;
    internalEnum.identifier = "Color";
    Ptr<Ty> internalEnumTy(new EnumTy("Color", internalEnum, std::vector<Ptr<Ty>>{}));
    EXPECT_TRUE(IntroducedParameterTypeBreaksPublicSignature(publicFunc, internalEnumTy));

    internalEnum.EnableAttr(Attribute::PUBLIC);
    EXPECT_FALSE(IntroducedParameterTypeBreaksPublicSignature(publicFunc, internalEnumTy));

    FuncDecl internalFunc;
    EXPECT_FALSE(IntroducedParameterTypeBreaksPublicSignature(internalFunc, internalEnumTy));
    EXPECT_FALSE(IntroducedParameterTypeBreaksPublicSignature(publicFunc, nullptr));
}
