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

OwnedPtr<LitConstExpr> MakeIntLiteral(Position begin, Position end)
{
    auto expr = MakeOwner<LitConstExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
    return expr;
}

OwnedPtr<RefExpr> MakeRefExpr(Position begin, Position end)
{
    auto expr = MakeOwner<RefExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64)));
    return expr;
}

OwnedPtr<RefExpr> MakeRefToDecl(Decl &decl, Position begin, Position end, TypeKind typeKind = TypeKind::TYPE_INT64)
{
    auto expr = MakeOwner<RefExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->ref.target = &decl;
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(typeKind)));
    return expr;
}

OwnedPtr<RefExpr> MakeRefToDeclWithTy(Decl &decl, Position begin, Position end, Ptr<Ty> ty)
{
    auto expr = MakeOwner<RefExpr>();
    expr->begin = begin;
    expr->end = end;
    expr->ref.target = &decl;
    expr->SetTy(ty);
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

OwnedPtr<TupleLit> MakeTupleLit(Position begin, Position end, std::vector<Ptr<Ty>> elementTys)
{
    auto expr = MakeOwner<TupleLit>();
    expr->begin = begin;
    expr->end = end;
    expr->SetTy(Ptr<Ty>(new TupleTy(std::move(elementTys))));
    return expr;
}

OwnedPtr<MemberAccess> MakeMemberAccess(OwnedPtr<Expr> baseExpr, Position begin, Position end)
{
    auto expr = MakeOwner<MemberAccess>();
    expr->begin = begin;
    expr->end = end;
    expr->baseExpr = std::move(baseExpr);
    expr->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_UNIT)));
    return expr;
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

TEST(IntroduceFieldTest, ReplaceUnicodeStringLiteralKeepsFollowingMemberAccess)
{
    const std::string sourceText =
        "func target() {\n"
        "    let runes = \"你好\".toRuneArray()\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 17}, {1, 2, 25}};
    auto file = MakeOwner<File>();
    TextEdit replaceEdit;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody->body->begin = {1, 1, 16};
    funcDecl->funcBody->body->end = {1, 3, 1};
    auto funcPtr = funcDecl.get();
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        std::string fieldName = "newField";
        replaceEdit = IntroduceField::ReplaceExprWithField(sel, selectedRange, fieldName, *funcPtr);
    }, sourceText);

    EXPECT_EQ(replaceEdit.newText, "newField");
    ExpectRangePosition(replaceEdit.range, {1, 16}, {1, 20});
}

TEST(IntroduceFieldTest, StringLiteralBeforeMemberAccessCanIntroduceField)
{
    const std::string sourceText =
        "func target() {\n"
        "    let runes = \"你好\".toRuneArray()\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 17}, {1, 2, 25}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody->body->begin = {1, 1, 16};
    funcDecl->funcBody->body->end = {1, 3, 1};
    auto stringLiteral = MakeOwner<LitConstExpr>();
    stringLiteral->begin = selectedRange.start;
    stringLiteral->end = selectedRange.end;
    stringLiteral->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_STRING)));
    funcDecl->funcBody->body->body.emplace_back(MakeMemberAccess(std::move(stringLiteral), {1, 2, 17}, {1, 2, 39}));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceField introduceField;
        EXPECT_TRUE(introduceField.Prepare(sel));
        sel.extraOptions["suggestName"] = "newField";
        effect = introduceField.Apply(sel);
    }, sourceText);

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText.find("private var newField: String = \"你好\"") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newField" && edit.range.start.line == 1 && edit.range.start.column == 16 &&
            edit.range.end.line == 1 && edit.range.end.column == 20;
    }));
}

TEST(IntroduceFieldTest, PrepareRejectsMemberFieldNameSelection)
{
    const std::string sourceText =
        "private class testname {\n"
        "    var a = 0\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 9}, {1, 2, 10}};
    auto file = MakeOwner<File>();
    bool prepared = true;
    std::map<std::string, std::string> extraOptions;

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 1, 1};
    classDecl->end = {1, 3, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 1, 24};
    auto fieldDecl = MakeOwner<VarDecl>();
    fieldDecl->begin = {1, 2, 5};
    fieldDecl->end = {1, 2, 14};
    fieldDecl->EnableAttr(Attribute::IN_CLASSLIKE);
    classDecl->body->decls.emplace_back(std::move(fieldDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        IntroduceField introduceField;
        prepared = introduceField.Prepare(sel);
        extraOptions = introduceField.ExtraOptions();
    }, sourceText);

    EXPECT_FALSE(prepared);
    EXPECT_FALSE(extraOptions.empty());
}

TEST(IntroduceFieldTest, MemberInitializerLiteralCanIntroduceField)
{
    const std::string sourceText =
        "private class testname {\n"
        "    var a = 0\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 13}, {1, 2, 14}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 1, 1};
    classDecl->end = {1, 3, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 1, 24};

    auto fieldDecl = MakeOwner<VarDecl>();
    fieldDecl->begin = {1, 2, 5};
    fieldDecl->end = {1, 2, 14};
    fieldDecl->EnableAttr(Attribute::IN_CLASSLIKE);
    fieldDecl->outerDecl = classDecl.get();
    fieldDecl->initializer = MakeIntLiteral(selectedRange.start, selectedRange.end);
    classDecl->body->decls.emplace_back(std::move(fieldDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceField introduceField;
        EXPECT_TRUE(introduceField.Prepare(sel));
        sel.extraOptions["suggestName"] = "newField";
        effect = introduceField.Apply(sel);
    }, sourceText);

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    ASSERT_EQ(edits.size(), 2);
    EXPECT_EQ(edits[0].newText, "    private var newField: Int64 = 0\n\n");
    EXPECT_EQ(edits[1].newText, "newField");
    ExpectRangePosition(edits[1].range, {1, 12}, {1, 13});
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

TEST(IntroduceFieldTest, ConstInitializerSelectionKeepsActionAndShowsWarning)
{
    const std::string sourceText =
        "main() {\n"
        "    const a1 = 9\n"
        "    const a2 = a1 + 0\n"
        "}\n";
    ark::Range selectedRange{{1, 3, 16}, {1, 3, 22}};
    auto file = MakeOwner<File>();
    bool prepared = true;
    std::map<std::string, std::string> extraOptions;
    std::optional<Tweak::Effect> effect;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 4, 1};
    funcDecl->funcBody->body->begin = {1, 1, 8};
    funcDecl->funcBody->body->end = {1, 4, 1};

    auto constDecl = MakeOwner<VarDecl>();
    constDecl->begin = {1, 3, 5};
    constDecl->end = {1, 3, 22};
    constDecl->isConst = true;
    constDecl->initializer = MakeAddExpr(
        MakeRefExpr({1, 3, 16}, {1, 3, 18}), MakeIntLiteral({1, 3, 21}, {1, 3, 22}),
        selectedRange.start, selectedRange.end);
    funcDecl->funcBody->body->body.emplace_back(std::move(constDecl));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        IntroduceField introduceField;
        prepared = introduceField.Prepare(sel);
        extraOptions = introduceField.ExtraOptions();
        effect = introduceField.Apply(sel);
    }, sourceText);

    EXPECT_TRUE(prepared);
    ASSERT_EQ(extraOptions.count("ErrorCode"), 1);
    EXPECT_EQ(extraOptions["ErrorCode"],
        std::to_string(static_cast<int>(IntroduceField::IntroduceFieldError::INVALID_CONST_INITIALIZER)));
    ASSERT_TRUE(effect.has_value());
    EXPECT_TRUE(effect->showMessage.has_value());
    EXPECT_TRUE(effect->applyEdits.empty());
    EXPECT_TRUE(effect->documentChanges.empty());
}

TEST(IntroduceFieldTest, ConstInitializerSubExpressionKeepsActionAndShowsWarning)
{
    const std::string sourceText =
        "const func add(a: Int64, b: Int64): Int64 {\n"
        "    return a + b\n"
        "}\n"
        "\n"
        "main() {\n"
        "    var a = add(10, 20)\n"
        "    const a1 = 9\n"
        "    const  a2=a1+0\n"
        "    const sum: Int64 = add(10, 20)\n"
        "}\n";
    ark::Range selectedRange{{1, 8, 15}, {1, 8, 19}};
    auto file = MakeOwner<File>();
    bool prepared = false;
    std::map<std::string, std::string> extraOptions;
    std::optional<Tweak::Effect> effect;

    auto addFunc = MakeFuncDecl(1);
    addFunc->begin = {1, 1, 1};
    addFunc->end = {1, 3, 1};
    addFunc->EnableAttr(Attribute::CONST);
    file->decls.emplace_back(std::move(addFunc));

    auto mainDecl = MakeFuncDecl(5);
    mainDecl->begin = {1, 5, 1};
    mainDecl->end = {1, 10, 1};
    mainDecl->funcBody->body->begin = {1, 5, 8};
    mainDecl->funcBody->body->end = {1, 10, 1};

    auto constDecl = MakeOwner<VarDecl>();
    constDecl->begin = {1, 8, 5};
    constDecl->end = {1, 8, 19};
    constDecl->isConst = true;
    constDecl->initializer = MakeAddExpr(
        MakeRefExpr({1, 8, 15}, {1, 8, 17}), MakeIntLiteral({1, 8, 18}, {1, 8, 19}),
        selectedRange.start, selectedRange.end);
    mainDecl->funcBody->body->body.emplace_back(std::move(constDecl));
    file->decls.emplace_back(std::move(mainDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        IntroduceField introduceField;
        prepared = introduceField.Prepare(sel);
        extraOptions = introduceField.ExtraOptions();
        effect = introduceField.Apply(sel);
    }, sourceText);

    EXPECT_TRUE(prepared);
    ASSERT_EQ(extraOptions.count("ErrorCode"), 1);
    EXPECT_EQ(extraOptions["ErrorCode"],
        std::to_string(static_cast<int>(IntroduceField::IntroduceFieldError::INVALID_CONST_INITIALIZER)));
    ASSERT_TRUE(effect.has_value());
    EXPECT_TRUE(effect->showMessage.has_value());
    EXPECT_TRUE(effect->applyEdits.empty());
    EXPECT_TRUE(effect->documentChanges.empty());
}

TEST(IntroduceFieldTest, LetPatternDestructorSelectionKeepsActionAndShowsWarning)
{
    const std::string sourceText =
        "func demo5() {\n"
        "    let opt: Option<Int64> = Some(100)\n"
        "    if (let Some(value) <- opt) {\n"
        "        println(value)\n"
        "    }\n"
        "}\n";
    ark::Range selectedRange{{1, 3, 9}, {1, 3, 31}};
    auto file = MakeOwner<File>();
    bool prepared = false;
    std::map<std::string, std::string> extraOptions;
    std::optional<Tweak::Effect> effect;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 6, 1};
    funcDecl->funcBody->body->begin = {1, 1, 14};
    funcDecl->funcBody->body->end = {1, 6, 1};

    auto letPattern = MakeOwner<LetPatternDestructor>();
    letPattern->begin = selectedRange.start;
    letPattern->end = selectedRange.end;
    letPattern->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_BOOLEAN)));
    funcDecl->funcBody->body->body.emplace_back(std::move(letPattern));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](const Tweak::Selection &sel) {
        IntroduceField introduceField;
        prepared = introduceField.Prepare(sel);
        extraOptions = introduceField.ExtraOptions();
        effect = introduceField.Apply(sel);
    }, sourceText);

    EXPECT_TRUE(prepared);
    ASSERT_EQ(extraOptions.count("ErrorCode"), 1);
    EXPECT_EQ(extraOptions["ErrorCode"],
        std::to_string(static_cast<int>(IntroduceField::IntroduceFieldError::INVALID_LET_PATTERN_DESTRUCTOR)));
    ASSERT_TRUE(effect.has_value());
    EXPECT_TRUE(effect->showMessage.has_value());
    EXPECT_TRUE(effect->applyEdits.empty());
    EXPECT_TRUE(effect->documentChanges.empty());
}

TEST(IntroduceFieldTest, LocalVarReferencePromotesDeclarationAndAllReferences)
{
    const std::string sourceText =
        "private var newField11: Int64 = 10\n"
        "\n"
        "func demo(): Int64 {\n"
        "    let a: Int64 = newField11\n"
        "    var b: Int32 = 20\n"
        "    const PI: Float64 = 3.14159\n"
        "\n"
        "    b = b + 5\n"
        "    println(\"a = ${a}, b = ${b}, PI = ${PI}\")\n"
        "    return 0\n"
        "}\n";
    ark::Range selectedRange{{1, 9, 31}, {1, 9, 32}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto existingField = MakeFieldDecl(1, 1);
    existingField->begin = {1, 1, 1};
    existingField->end = {1, 1, 36};
    existingField->EnableAttr(Attribute::GLOBAL);
    file->decls.emplace_back(std::move(existingField));

    auto funcDecl = MakeFuncDecl(3);
    funcDecl->begin = {1, 3, 1};
    funcDecl->end = {1, 11, 1};
    funcDecl->funcBody->body->begin = {1, 3, 21};
    funcDecl->funcBody->body->end = {1, 11, 1};

    auto localVar = MakeOwner<VarDecl>();
    localVar->begin = {1, 5, 5};
    localVar->end = {1, 5, 22};
    localVar->isVar = true;
    localVar->initializer = MakeIntLiteral({1, 5, 20}, {1, 5, 22});
    localVar->SetTy(Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT32)));
    auto localVarPtr = localVar.get();
    funcDecl->funcBody->body->body.emplace_back(std::move(localVar));
    funcDecl->funcBody->body->body.emplace_back(
        MakeRefToDecl(*localVarPtr, {1, 8, 5}, {1, 8, 6}, TypeKind::TYPE_INT32));
    funcDecl->funcBody->body->body.emplace_back(
        MakeRefToDecl(*localVarPtr, {1, 8, 9}, {1, 8, 10}, TypeKind::TYPE_INT32));
    funcDecl->funcBody->body->body.emplace_back(
        MakeRefToDecl(*localVarPtr, selectedRange.start, selectedRange.end, TypeKind::TYPE_INT32));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceField introduceField;
        EXPECT_TRUE(introduceField.Prepare(sel));
        sel.extraOptions["suggestName"] = "newField3";
        effect = introduceField.Apply(sel);
    }, sourceText);

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    ASSERT_GE(edits.size(), 5);
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "private var newField3: Int32 = 0";
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newField3 = 20";
    }));
    EXPECT_EQ(std::count_if(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newField3" &&
            edit.range.start.line == 7 &&
            (edit.range.start.column == 4 || edit.range.start.column == 8);
    }), 2);
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newField3" && edit.range.start.line == 8 && edit.range.start.column == 30;
    }));
}

TEST(IntroduceFieldTest, ParameterReferenceOfEnumAliasUsesEnumDefaultAndAssignsParameter)
{
    const std::string sourceText =
        "public enum astatuname {\n"
        "    pending | success | failed | cancelled\n"
        "}\n"
        "\n"
        "public type Status = astatuname\n"
        "public class StatusHandler {\n"
        "    public func handleStatus(status: Status): String {\n"
        "        match (status) {\n"
        "        }\n"
        "    }\n"
        "}\n";
    ark::Range selectedRange{{1, 8, 16}, {1, 8, 22}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto enumDecl = MakeOwner<EnumDecl>();
    enumDecl->begin = {1, 1, 1};
    enumDecl->end = {1, 3, 1};
    enumDecl->identifier = "astatuname";
    auto pending = MakeOwner<VarDecl>();
    pending->identifier = "pending";
    enumDecl->constructors.emplace_back(std::move(pending));
    auto enumPtr = enumDecl.get();
    file->decls.emplace_back(std::move(enumDecl));

    auto aliasDecl = MakeOwner<TypeAliasDecl>();
    aliasDecl->begin = {1, 5, 1};
    aliasDecl->end = {1, 5, 31};
    aliasDecl->identifier = "Status";
    file->decls.emplace_back(std::move(aliasDecl));

    auto classDecl = MakeOwner<ClassDecl>();
    classDecl->begin = {1, 6, 1};
    classDecl->end = {1, 11, 1};
    classDecl->body = MakeOwner<ClassBody>();
    classDecl->body->leftCurlPos = {1, 6, 28};

    auto funcDecl = MakeFuncDecl(7);
    funcDecl->begin = {1, 7, 5};
    funcDecl->end = {1, 10, 5};
    funcDecl->outerDecl = classDecl.get();
    funcDecl->funcBody->body->begin = {1, 7, 53};
    funcDecl->funcBody->body->end = {1, 10, 5};

    auto paramList = MakeOwner<FuncParamList>();
    auto param = MakeOwner<FuncParam>();
    param->begin = {1, 7, 30};
    param->end = {1, 7, 44};
    param->identifier = "status";
    param->type = MakeOwner<RefType>();
    param->type->begin = {1, 7, 38};
    param->type->end = {1, 7, 44};
    param->SetTy(Ptr<Ty>(new EnumTy("astatuname", *enumPtr, {})));
    auto paramPtr = param.get();
    paramList->params.emplace_back(std::move(param));
    funcDecl->funcBody->paramLists.emplace_back(std::move(paramList));
    funcDecl->funcBody->body->body.emplace_back(
        MakeRefToDeclWithTy(*paramPtr, selectedRange.start, selectedRange.end,
            Ptr<Ty>(new EnumTy("astatuname", *enumPtr, {}))));

    classDecl->body->decls.emplace_back(std::move(funcDecl));
    file->decls.emplace_back(std::move(classDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceField introduceField;
        EXPECT_TRUE(introduceField.Prepare(sel));
        sel.extraOptions["suggestName"] = "newField332";
        effect = introduceField.Apply(sel);
    }, sourceText);

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText.find("private var newField332: astatuname = Status.pending") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "        this.newField332 = status\n";
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "this.newField332";
    }));
}

TEST(IntroduceFieldTest, TupleLiteralInNamedTupleContextUsesUnnamedTupleFieldType)
{
    const std::string sourceText =
        "public func testTupleType(): Unit {\n"
        "    let tu: (name: String, price: Int64) = (\"banana\", 5)\n"
        "}\n";
    ark::Range selectedRange{{1, 2, 45}, {1, 2, 58}};
    auto file = MakeOwner<File>();
    std::optional<Tweak::Effect> effect;

    auto funcDecl = MakeFuncDecl(1);
    funcDecl->begin = {1, 1, 1};
    funcDecl->end = {1, 3, 1};
    funcDecl->funcBody->body->begin = {1, 1, 35};
    funcDecl->funcBody->body->end = {1, 3, 1};
    funcDecl->funcBody->body->body.emplace_back(MakeTupleLit(selectedRange.start, selectedRange.end,
        {Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_STRING)), Ptr<Ty>(new PrimitiveTy(TypeKind::TYPE_INT64))}));
    file->decls.emplace_back(std::move(funcDecl));

    RunWithSelection(std::move(file), selectedRange, [&](Tweak::Selection &sel) {
        IntroduceField introduceField;
        EXPECT_TRUE(introduceField.Prepare(sel));
        sel.extraOptions["suggestName"] = "newField";
        effect = introduceField.Apply(sel);
    }, sourceText);

    ASSERT_TRUE(effect.has_value());
    ASSERT_EQ(effect->applyEdits.size(), 1);
    const auto &edits = effect->applyEdits.begin()->second;
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText.find("private var newField: (String, Int64) = (\"banana\", 5)") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(edits.begin(), edits.end(), [](const TextEdit &edit) {
        return edit.newText == "newField";
    }));
}
