// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/IntroduceParameter.h"
#include "ArkAST.h"
#include "gtest/gtest.h"

using namespace ark;
using namespace Cangjie::AST;

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

OwnedPtr<FuncParam> MakeParam(const std::string &name, const Position &begin, const Position &end)
{
    auto param = MakeOwner<FuncParam>();
    param->identifier = name;
    param->begin = begin;
    param->end = end;
    return param;
}

OwnedPtr<RefExpr> MakeParamRef(FuncParam &param, const Position &begin, const Position &end)
{
    auto ref = MakeOwner<RefExpr>();
    ref->begin = begin;
    ref->end = end;
    ref->ref = Reference(param.identifier);
    ref->ref.target = &param;
    return ref;
}

OwnedPtr<BinaryExpr> MakeSelectedBinaryExpr(FuncParam &leftParam, FuncParam &rightParam)
{
    auto expr = MakeOwner<BinaryExpr>();
    expr->begin = {1, 2, 12};
    expr->end = {1, 2, 24};
    expr->leftExpr = MakeParamRef(leftParam, {1, 2, 12}, {1, 2, 16});
    expr->rightExpr = MakeParamRef(rightParam, {1, 2, 19}, {1, 2, 24});
    auto intTy = new PrimitiveTy(TypeKind::TYPE_INT64);
    intTy->name = "Int64";
    expr->SetTy(intTy);
    return expr;
}

OwnedPtr<LitConstExpr> MakeArgExpr(const Position &begin, const Position &end)
{
    auto expr = MakeOwner<LitConstExpr>();
    expr->begin = begin;
    expr->end = end;
    return expr;
}

OwnedPtr<FuncArg> MakeCallArg(OwnedPtr<Expr> expr, const Position &begin, const Position &end,
    const std::string &name = "")
{
    auto arg = MakeOwner<FuncArg>();
    arg->begin = begin;
    arg->end = end;
    arg->expr = std::move(expr);
    if (!name.empty()) {
        arg->name = SrcIdentifier(name);
    }
    return arg;
}

OwnedPtr<CallExpr> MakeCallExpr(FuncDecl &targetFunc)
{
    auto callExpr = MakeOwner<CallExpr>();
    callExpr->begin = {1, 6, 12};
    callExpr->end = {1, 6, 23};
    callExpr->rightParenPos = {1, 6, 23};
    callExpr->resolvedFunction = &targetFunc;
    callExpr->args.emplace_back(MakeCallArg(MakeArgExpr({1, 6, 19}, {1, 6, 20}), {1, 6, 19}, {1, 6, 20}));
    callExpr->args.emplace_back(MakeCallArg(MakeArgExpr({1, 6, 22}, {1, 6, 23}), {1, 6, 22}, {1, 6, 23}));
    return callExpr;
}

template <typename Callback>
void RunWithIntroduceParameterSelection(OwnedPtr<File> file, ark::Range &selectedRange,
    const std::string &sourceText, Callback callback)
{
    DiagnosticEngine diagEngine;
    SourceManager sourceManager;
    sourceManager.AddSource("test.cj", sourceText);
    auto package = MakeOwner<Package>();
    file->curPackage = package.get();
    file->filePath = "test.cj";
    package->files.emplace_back(std::move(file));

    std::pair<std::string, std::string> paths = {"test.cj", sourceText};
    ArkAST arkAst(paths, package->files.back().get(), diagEngine, nullptr, &sourceManager);
    ASSERT_TRUE(SelectionTree::CreateEach(
        arkAst, "test.cj", selectedRange.start, selectedRange.end, [&](SelectionTree selectionTree) {
            Tweak::Selection sel(arkAst, selectedRange, std::move(selectionTree), {{"suggestName", "newParam"}});
            callback(sel);
            return true;
        }));
}

OwnedPtr<FuncDecl> MakeTargetFuncWithSelectedExpression()
{
    auto funcDecl = MakeOwner<FuncDecl>();
    funcDecl->identifier = "target";
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};

    auto paramList = MakeParamList({1, 1, 39});
    auto leftParam = MakeParam("left", {1, 1, 13}, {1, 1, 24});
    auto rightParam = MakeParam("right", {1, 1, 26}, {1, 1, 38});
    auto leftPtr = leftParam.get();
    auto rightPtr = rightParam.get();
    paramList->params.emplace_back(std::move(leftParam));
    paramList->params.emplace_back(std::move(rightParam));

    funcDecl->funcBody = MakeOwner<FuncBody>();
    funcDecl->funcBody->paramLists.emplace_back(std::move(paramList));
    funcDecl->funcBody->body = MakeOwner<Block>();
    funcDecl->funcBody->body->begin = {1, 1, 48};
    funcDecl->funcBody->body->end = {1, 3, 1};
    auto selectedExpr = MakeSelectedBinaryExpr(*leftPtr, *rightPtr);
    funcDecl->funcBody->body->body.emplace_back(std::move(selectedExpr));
    return funcDecl;
}

OwnedPtr<FuncDecl> MakeTargetFuncKeepingFirstParameter()
{
    auto funcDecl = MakeOwner<FuncDecl>();
    funcDecl->identifier = "target";
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 4, 1};

    auto paramList = MakeParamList({1, 1, 39});
    auto leftParam = MakeParam("left", {1, 1, 13}, {1, 1, 24});
    auto rightParam = MakeParam("right", {1, 1, 26}, {1, 1, 38});
    auto leftPtr = leftParam.get();
    auto rightPtr = rightParam.get();
    paramList->params.emplace_back(std::move(leftParam));
    paramList->params.emplace_back(std::move(rightParam));

    funcDecl->funcBody = MakeOwner<FuncBody>();
    funcDecl->funcBody->paramLists.emplace_back(std::move(paramList));
    funcDecl->funcBody->body = MakeOwner<Block>();
    funcDecl->funcBody->body->begin = {1, 1, 48};
    funcDecl->funcBody->body->end = {1, 4, 1};
    funcDecl->funcBody->body->body.emplace_back(MakeParamRef(*leftPtr, {1, 2, 17}, {1, 2, 21}));
    auto selectedExpr = MakeSelectedBinaryExpr(*leftPtr, *rightPtr);
    selectedExpr->begin = {1, 3, 12};
    selectedExpr->end = {1, 3, 24};
    selectedExpr->leftExpr->begin = {1, 3, 12};
    selectedExpr->leftExpr->end = {1, 3, 16};
    selectedExpr->rightExpr->begin = {1, 3, 19};
    selectedExpr->rightExpr->end = {1, 3, 24};
    funcDecl->funcBody->body->body.emplace_back(std::move(selectedExpr));
    return funcDecl;
}

OwnedPtr<FuncDecl> MakeCallerFunc(FuncDecl &targetFunc)
{
    auto caller = MakeOwner<FuncDecl>();
    caller->identifier = "caller";
    caller->begin = {1, 5, 1};
    caller->end = {1, 7, 1};
    caller->funcBody = MakeOwner<FuncBody>();
    caller->funcBody->body = MakeOwner<Block>();
    caller->funcBody->body->begin = {1, 5, 24};
    caller->funcBody->body->end = {1, 7, 1};
    caller->funcBody->body->body.emplace_back(MakeCallExpr(targetFunc));
    return caller;
}

OwnedPtr<FuncDecl> MakeNamedCallerFunc(FuncDecl &targetFunc)
{
    auto caller = MakeOwner<FuncDecl>();
    caller->identifier = "caller";
    caller->begin = {1, 6, 1};
    caller->end = {1, 8, 1};
    caller->funcBody = MakeOwner<FuncBody>();
    caller->funcBody->body = MakeOwner<Block>();
    caller->funcBody->body->begin = {1, 6, 24};
    caller->funcBody->body->end = {1, 8, 1};

    auto callExpr = MakeOwner<CallExpr>();
    callExpr->begin = {1, 7, 12};
    callExpr->end = {1, 7, 36};
    callExpr->rightParenPos = {1, 7, 36};
    callExpr->resolvedFunction = &targetFunc;
    callExpr->args.emplace_back(MakeCallArg(MakeArgExpr({1, 7, 19}, {1, 7, 20}), {1, 7, 19}, {1, 7, 20}));
    callExpr->args.emplace_back(
        MakeCallArg(MakeArgExpr({1, 7, 29}, {1, 7, 30}), {1, 7, 22}, {1, 7, 30}, "right"));
    caller->funcBody->body->body.emplace_back(std::move(callExpr));
    return caller;
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

TEST(IntroduceParameterTest, ApplyReplacesAllUsedParametersAndUpdatesCallSiteArguments)
{
    const std::string sourceText =
        "func target(left: Int64, right: Int64): Int64 {\n"
        "    return left + right\n"
        "}\n"
        "\n"
        "func caller(): Int64 {\n"
        "    return target(1, 2)\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 12}, {1, 2, 24}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto targetFunc = MakeTargetFuncWithSelectedExpression();
    auto targetPtr = targetFunc.get();
    auto callerFunc = MakeCallerFunc(*targetPtr);
    file->decls.emplace_back(std::move(targetFunc));
    file->decls.emplace_back(std::move(callerFunc));

    RunWithIntroduceParameterSelection(std::move(file), selectedRange, sourceText, [&](const Tweak::Selection &sel) {
        IntroduceParameter tweak;
        effect = tweak.Apply(sel);
    });

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    ASSERT_EQ(edits.size(), 3);

    EXPECT_EQ(edits[0].newText, "newParam: Int64");
    EXPECT_EQ(edits[0].range.start, Position(0, 0, 12));
    EXPECT_EQ(edits[0].range.end, Position(0, 0, 37));

    EXPECT_EQ(edits[1].newText, "newParam");
    EXPECT_EQ(edits[1].range.start, Position(0, 1, 11));
    EXPECT_EQ(edits[1].range.end, Position(0, 1, 23));

    EXPECT_EQ(edits[2].newText, "1 + 2");
    EXPECT_EQ(edits[2].range.start, Position(0, 5, 18));
    EXPECT_EQ(edits[2].range.end, Position(0, 5, 22));
}

TEST(IntroduceParameterTest, ApplyKeepsExternallyUsedParameterAndBuildsNamedCallSiteArgument)
{
    const std::string sourceText =
        "func target(left: Int64, right: Int64): Int64 {\n"
        "    let keep = left\n"
        "    return left + right\n"
        "}\n"
        "\n"
        "func caller(): Int64 {\n"
        "    return target(1, right: 2)\n"
        "}\n";
    ark::Range selectedRange{{1, 3, 12}, {1, 3, 24}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto targetFunc = MakeTargetFuncKeepingFirstParameter();
    auto targetPtr = targetFunc.get();
    auto callerFunc = MakeNamedCallerFunc(*targetPtr);
    file->decls.emplace_back(std::move(targetFunc));
    file->decls.emplace_back(std::move(callerFunc));

    RunWithIntroduceParameterSelection(std::move(file), selectedRange, sourceText, [&](const Tweak::Selection &sel) {
        IntroduceParameter tweak;
        effect = tweak.Apply(sel);
    });

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    ASSERT_EQ(edits.size(), 4);

    EXPECT_EQ(edits[0].newText, "newParam: Int64");
    EXPECT_EQ(edits[1].newText, "");
    EXPECT_EQ(edits[2].newText, "newParam");
    EXPECT_EQ(edits[3].newText, "newParam: 1 + 2");
    EXPECT_EQ(edits[3].range.start, Position(0, 6, 35));
    EXPECT_EQ(edits[3].range.end, Position(0, 6, 35));
}
