// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/refactor/tweaks/IntroduceField.h"
#include "ArkAST.h"
#include "gtest/gtest.h"
#include <memory>

using namespace ark;
using namespace Cangjie::AST;

namespace {
template <typename T> OwnedPtr<T> MakeOwner()
{
    return OwnedPtr<T>(new T());
}

OwnedPtr<FuncDecl> MakeFuncDecl(int line)
{
    auto funcDecl = MakeOwner<FuncDecl>();
    funcDecl->begin = {1, line, 5};
    funcDecl->end = {1, line + 2, 5};
    funcDecl->funcBody = MakeOwner<FuncBody>();
    funcDecl->funcBody->body = MakeOwner<Block>();
    funcDecl->funcBody->body->begin = {1, line, 18};
    funcDecl->funcBody->body->end = {1, line + 2, 5};
    return funcDecl;
}

OwnedPtr<VarDecl> MakeFieldDecl(int beginLine, int endLine)
{
    auto fieldDecl = MakeOwner<VarDecl>();
    fieldDecl->begin = {1, beginLine, 5};
    fieldDecl->end = {1, endLine, 27};
    return fieldDecl;
}

void ExpectRangePosition(const ark::Range &range, const Position &start, const Position &end)
{
    EXPECT_EQ(range.start, start);
    EXPECT_EQ(range.end, end);
}

template <typename Callback>
void RunWithSelection(OwnedPtr<File> file, ark::Range &selectedRange, Callback callback,
                      const std::string &sourceText = "")
{
    DiagnosticEngine diagEngine;
    SourceManager sourceManager;
    SourceManager *sourceManagerPtr = nullptr;
    if (!sourceText.empty()) {
        sourceManager.AddSource("test.cj", sourceText);
        sourceManagerPtr = &sourceManager;
    }
    auto package = MakeOwner<Package>();
    file->curPackage = package.get();
    file->filePath = "test.cj";
    package->files.emplace_back(std::move(file));

    std::pair<std::string, std::string> paths = {"test.cj", sourceText};
    ArkAST arkAst(paths, package->files.back().get(), diagEngine, nullptr, sourceManagerPtr);
    ASSERT_TRUE(SelectionTree::CreateEach(
        arkAst, "test.cj", selectedRange.start, selectedRange.end, [&](SelectionTree selectionTree) {
            Tweak::Selection sel(arkAst, selectedRange, std::move(selectionTree), {});
            callback(sel);
            return true;
        }));
}

void ExpectTarget(FuncDecl &funcDecl, bool supported, bool member, bool isStatic)
{
    EXPECT_EQ(IntroduceField::IsSupportedTargetFunc(funcDecl), supported);
    EXPECT_EQ(IntroduceField::IsMemberFieldTarget(funcDecl), member);
    EXPECT_EQ(IntroduceField::IsStaticFieldTarget(funcDecl), isStatic);
}
} // namespace

TEST(IntroduceFieldTest, TargetKinds)
{
    FuncDecl topLevelFunc;
    ExpectTarget(topLevelFunc, true, false, false);

    auto classDecl = MakeOwner<ClassDecl>();
    FuncDecl classFunc;
    classFunc.outerDecl = classDecl.get();
    ExpectTarget(classFunc, true, true, false);
    classFunc.EnableAttr(Attribute::STATIC);
    ExpectTarget(classFunc, true, true, true);

    auto structDecl = MakeOwner<StructDecl>();
    FuncDecl structFunc;
    structFunc.outerDecl = structDecl.get();
    ExpectTarget(structFunc, true, true, false);

    auto interfaceDecl = MakeOwner<InterfaceDecl>();
    FuncDecl interfaceFunc;
    interfaceFunc.outerDecl = interfaceDecl.get();
    ExpectTarget(interfaceFunc, false, false, false);

    auto enumDecl = MakeOwner<EnumDecl>();
    FuncDecl enumFunc;
    enumFunc.outerDecl = enumDecl.get();
    ExpectTarget(enumFunc, false, false, false);

    auto extendDecl = MakeOwner<ExtendDecl>();
    FuncDecl extendFunc;
    extendFunc.outerDecl = extendDecl.get();
    ExpectTarget(extendFunc, false, false, false);
}

TEST(IntroduceFieldTest, StaticOnlyAppliesToSupportedMemberFunctions)
{
    FuncDecl topLevelStaticFunc;
    topLevelStaticFunc.EnableAttr(Attribute::STATIC);
    ExpectTarget(topLevelStaticFunc, true, false, false);

    auto interfaceDecl = MakeOwner<InterfaceDecl>();
    FuncDecl interfaceStaticFunc;
    interfaceStaticFunc.outerDecl = interfaceDecl.get();
    interfaceStaticFunc.EnableAttr(Attribute::STATIC);
    ExpectTarget(interfaceStaticFunc, false, false, false);

    auto structDecl = MakeOwner<StructDecl>();
    FuncDecl structStaticFunc;
    structStaticFunc.outerDecl = structDecl.get();
    structStaticFunc.EnableAttr(Attribute::STATIC);
    ExpectTarget(structStaticFunc, true, true, true);
}

TEST(IntroduceFieldTest, ExtraOptionsReturnStoredOptions)
{
    IntroduceField introduceField;
    EXPECT_TRUE(introduceField.ExtraOptions().empty());

    introduceField.extraOptions["ErrorCode"] = "4";
    introduceField.extraOptions["suggestName"] = "fieldValue";

    auto options = introduceField.ExtraOptions();
    EXPECT_EQ(options.at("ErrorCode"), "4");
    EXPECT_EQ(options.at("suggestName"), "fieldValue");

    options["ErrorCode"] = "changed";
    EXPECT_EQ(introduceField.ExtraOptions().at("ErrorCode"), "4");
}

TEST(IntroduceFieldTest, FieldInsertRangeUsesClassBodyFirstLineWhenNoPriorField)
{
    ark::Range selectedRange{{1, 6, 16}, {1, 6, 21}};
    auto file = MakeOwner<File>();
    std::optional<ark::Range> insertRange;
    Ptr<InheritableDecl> owner = nullptr;

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 2, 1};
    classDecl->end = {1, 10, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 3, 13};
    auto funcDecl = MakeFuncDecl(5);
    funcDecl->outerDecl = classDecl.get();
    auto funcPtr = funcDecl.get();
    auto classPtr = classDecl.get();
    classDecl->body->decls.emplace_back(std::move(funcDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        owner = IntroduceField::GetOwnerDecl(sel);
        insertRange = IntroduceField::GetFieldInsertRange(sel, selectedRange, *funcPtr);
    });

    EXPECT_EQ(owner.get(), classPtr);
    ASSERT_TRUE(insertRange.has_value());
    ExpectRangePosition(*insertRange, {1, 4, 1}, {1, 4, 1});
}

TEST(IntroduceFieldTest, FieldInsertRangeUsesLastPriorClassField)
{
    ark::Range selectedRange{{1, 10, 16}, {1, 10, 21}};
    auto file = MakeOwner<File>();
    std::optional<ark::Range> insertRange;

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 2, 1};
    classDecl->end = {1, 13, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 3, 13};
    classDecl->body->decls.emplace_back(MakeFieldDecl(4, 4));
    classDecl->body->decls.emplace_back(MakeFieldDecl(6, 6));
    auto funcDecl = MakeFuncDecl(9);
    funcDecl->outerDecl = classDecl.get();
    auto funcPtr = funcDecl.get();
    classDecl->body->decls.emplace_back(std::move(funcDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        insertRange = IntroduceField::GetFieldInsertRange(sel, selectedRange, *funcPtr);
    });

    ASSERT_TRUE(insertRange.has_value());
    ExpectRangePosition(*insertRange, {1, 6, 27}, {1, 6, 27});
}

TEST(IntroduceFieldTest, FieldInsertRangeCleansWhitespaceUntilFirstNonFieldMember)
{
    const std::string sourceText =
        "class Demo {\n"
        "    private var oldField: Int64 = 0\n"
        "\n"
        "\n"
        "    func target(): Int64 {\n"
        "        return 1 + 2\n"
        "    }\n"
        "}\n";
    ark::Range selectedRange{{1, 6, 16}, {1, 6, 21}};
    auto file = MakeOwner<File>();
    std::optional<ark::Range> insertRange;

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 1, 1};
    classDecl->end = {1, 8, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 1, 12};
    auto skippedFunc = MakeFuncDecl(1);
    skippedFunc->begin = {1, 1, 5};
    skippedFunc->end = {1, 1, 12};
    classDecl->body->decls.emplace_back(std::move(skippedFunc));
    classDecl->body->decls.emplace_back(MakeFieldDecl(2, 2));
    classDecl->body->decls.emplace_back(nullptr);
    auto funcDecl = MakeFuncDecl(5);
    funcDecl->outerDecl = classDecl.get();
    auto funcPtr = funcDecl.get();
    classDecl->body->decls.emplace_back(std::move(funcDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        insertRange = IntroduceField::GetFieldInsertRange(sel, selectedRange, *funcPtr);
    }, sourceText);

    ASSERT_TRUE(insertRange.has_value());
    ExpectRangePosition(*insertRange, {1, 2, 27}, {1, 5, 1});
}

TEST(IntroduceFieldTest, FieldAssignmentFallsBackToLeadingIndentBeforeSelection)
{
    const std::string sourceText =
        "func target(): Int64 {\n"
        "    return 1 + 2\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 12}, {1, 2, 17}};
    auto file = MakeOwner<File>();
    TextEdit assignmentEdit;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody->body->begin = {1, 1, 22};
    funcDecl->funcBody->body->end = {1, 3, 1};
    auto funcPtr = funcDecl.get();
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        std::string fieldName = "newField";
        sel.arkAst->tokens.clear();
        assignmentEdit = IntroduceField::InsertFieldAssignment(sel, selectedRange, fieldName, *funcPtr);
    }, sourceText);

    EXPECT_EQ(assignmentEdit.newText, "    newField = 1 + 2\n");
    ExpectRangePosition(assignmentEdit.range, {1, 1, 0}, {1, 1, 0});
}

TEST(IntroduceFieldTest, FieldInsertRangeHandlesStructAndRejectsOwnerWithoutBody)
{
    ark::Range selectedRange{{1, 7, 16}, {1, 7, 21}};
    auto file = MakeOwner<File>();
    std::optional<ark::Range> insertRange;
    std::optional<ark::Range> classWithoutBodyRange;
    Ptr<InheritableDecl> owner = nullptr;

    auto structDecl = MakeOwner<StructDecl>();
    structDecl->begin = {1, 1, 1};
    structDecl->end = {1, 9, 1};
    structDecl->body = MakeOwner<StructBody>();
    structDecl->body->leftCurlPos = {1, 2, 14};
    auto structFunc = MakeFuncDecl(6);
    structFunc->outerDecl = structDecl.get();
    auto structFuncPtr = structFunc.get();
    auto structPtr = structDecl.get();
    structDecl->body->decls.emplace_back(std::move(structFunc));
    file->decls.emplace_back(std::move(structDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        owner = IntroduceField::GetOwnerDecl(sel);
        insertRange = IntroduceField::GetFieldInsertRange(sel, selectedRange, *structFuncPtr);

        auto classDeclWithoutBody = MakeOwner<ClassDecl>();
        auto classFunc = MakeFuncDecl(8);
        classFunc->outerDecl = classDeclWithoutBody.get();
        classWithoutBodyRange = IntroduceField::GetFieldInsertRange(sel, selectedRange, *classFunc);
    });

    EXPECT_EQ(owner.get(), structPtr);
    ASSERT_TRUE(insertRange.has_value());
    ExpectRangePosition(*insertRange, {1, 3, 1}, {1, 3, 1});
    EXPECT_FALSE(classWithoutBodyRange.has_value());
}

TEST(IntroduceFieldTest, OwnerDeclRejectsTopLevelFunction)
{
    ark::Range selectedRange{{1, 3, 12}, {1, 3, 17}};
    auto file = MakeOwner<File>();
    Ptr<InheritableDecl> owner = nullptr;
    auto funcDecl = MakeFuncDecl(2);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 5, 1};
    funcDecl->funcBody->body->begin = {1, 2, 12};
    funcDecl->funcBody->body->end = {1, 4, 1};
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        owner = IntroduceField::GetOwnerDecl(sel);
    });

    EXPECT_EQ(owner, nullptr);
}
