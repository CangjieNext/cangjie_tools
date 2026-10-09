// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "capabilities/diagnostic/UnusedSymbolDiag.h"
#include "FindDeclUsage.h"

#include <algorithm>
#include <gtest/gtest.h>

namespace ark {
namespace {
using namespace Cangjie;
using namespace Cangjie::AST;

// Exercise the public diagnostic analysis against the established reference
// finder. The synthetic ASTs model compiler clones and macro/source nodes
// independently of the implementation of the precomputed usage index.
// Named call labels remain references for rename, but are not parameter reads.
class UnusedSymbolDiagTest : public ::testing::Test {
protected:
    Package package{"unused.tests"};
    std::vector<OwnedPtr<Node>> detachedNodes;
    PrimitiveTy unitTy{TypeKind::TYPE_UNIT};
    PrimitiveTy intTy{TypeKind::TYPE_INT64};
    FuncTy functionTy{{}, &unitTy};
    File* file = AddFile("source.cj");

    File* AddFile(const std::string& name)
    {
        auto owned = MakeOwned<File>();
        auto result = owned.get();
        result->fileName = name;
        result->filePath = "/unused/" + name;
        result->curPackage = &package;
        package.files.emplace_back(std::move(owned));
        return result;
    }

    void Locate(Node& node, int line, File* sourceFile = nullptr)
    {
        node.begin = {1, line, 1};
        node.end = {1, line, 9};
        node.curFile = sourceFile ? sourceFile : file;
    }

    void Name(Decl& decl, const std::string& name, int line, File* sourceFile = nullptr)
    {
        Locate(decl, line, sourceFile);
        decl.identifier = name;
        decl.identifier.SetPos(decl.begin, decl.begin + static_cast<int>(name.size()));
        decl.fullPackageName = package.fullPackageName;
    }

    FuncDecl* AddFunction(const std::string& name, int line)
    {
        auto owned = MakeOwned<FuncDecl>();
        auto result = owned.get();
        Name(*result, name, line);
        result->EnableAttr(Attribute::GLOBAL);
        result->funcBody = MakeOwned<FuncBody>();
        result->funcBody->funcDecl = result;
        result->funcBody->paramLists.emplace_back(MakeOwned<FuncParamList>());
        result->funcBody->body = MakeOwned<Block>();
        file->decls.emplace_back(std::move(owned));
        return result;
    }

    FuncParam* AddParam(FuncDecl& function, const std::string& name, int line, bool named = false)
    {
        auto owned = MakeOwned<FuncParam>();
        auto result = owned.get();
        Name(*result, name, line);
        result->outerDecl = &function;
        result->isNamedParam = named;
        result->SetTy(&intTy);
        function.funcBody->paramLists[0]->params.emplace_back(std::move(owned));
        return result;
    }

    VarDecl* AddLocal(FuncDecl& function, const std::string& name, int line)
    {
        auto owned = MakeOwned<VarDecl>();
        auto result = owned.get();
        Name(*result, name, line);
        result->outerDecl = &function;
        function.funcBody->body->body.emplace_back(std::move(owned));
        return result;
    }

    VarDecl* AddVariant(const std::string& name, int line)
    {
        auto enumeration = MakeOwned<EnumDecl>();
        Name(*enumeration, "Choice", line - 1);
        enumeration->EnableAttr(Attribute::GLOBAL);
        auto owned = MakeOwned<VarDecl>();
        auto result = owned.get();
        Name(*result, name, line);
        result->outerDecl = enumeration.get();
        result->EnableAttr(Attribute::IN_ENUM);
        enumeration->constructors.emplace_back(std::move(owned));
        file->decls.emplace_back(std::move(enumeration));
        return result;
    }

    template<typename T> T* Detached(const std::string& name, int line, File* sourceFile = nullptr)
    {
        auto owned = MakeOwned<T>();
        auto result = owned.get();
        Name(*result, name, line, sourceFile);
        detachedNodes.emplace_back(std::move(owned));
        return result;
    }

    RefExpr* Reference(Decl& target, int line, bool originalMacro = false)
    {
        auto owned = MakeOwned<RefExpr>();
        auto result = owned.get();
        Locate(*result, line);
        result->ref.identifier = target.identifier;
        result->ref.target = &target;
        // Both collections are semantically live reference roots, even though
        // they are not ordinary children of the file's declaration AST.
        if (originalMacro) {
            file->originalMacroCallNodes.emplace_back(std::move(owned));
        } else {
            file->trashBin.emplace_back(std::move(owned));
        }
        return result;
    }

    RefExpr* SourceReference(RefExpr& generated, int sourceLine)
    {
        auto owned = MakeOwned<RefExpr>();
        auto result = owned.get();
        Locate(*result, sourceLine);
        generated.sourceExpr = result;
        detachedNodes.emplace_back(std::move(owned));
        return result;
    }

    void NamedCall(RefExpr& reference, const std::string& argumentName)
    {
        auto call = MakeOwned<CallExpr>();
        auto argument = MakeOwned<FuncArg>();
        argument->name = argumentName;
        call->args.emplace_back(std::move(argument));
        reference.callOrPattern = call.get();
        detachedNodes.emplace_back(std::move(call));
    }

    bool HasUnusedDiagnostic(const Decl& decl)
    {
        auto diagnostics = UnusedSymbolDiag::AnalyzeLocalSymbols(*file, package);
        auto identifier = decl.GetIdentifierPos();
        return std::any_of(diagnostics.begin(), diagnostics.end(), [&](const DiagnosticToken& diagnostic) {
            return diagnostic.range.start.line == identifier.line - 1 &&
                diagnostic.range.start.column == identifier.column - 1 &&
                diagnostic.message.find("'" + decl.identifier.Val() + "'") != std::string::npos;
        });
    }

    void ExpectUsage(const Decl& decl, bool used)
    {
        auto original = FindDeclUsage(decl, package);
        ASSERT_EQ(!original.empty(), used) << "Reference finder disagrees with the scenario expectation";
        EXPECT_EQ(HasUnusedDiagnostic(decl), original.empty())
            << "Precomputed usage must preserve FindDeclUsage semantics";
    }

    void ExpectNamedLabelOnly(const Decl& decl)
    {
        ASSERT_FALSE(FindDeclUsage(decl, package).empty())
            << "Named labels must remain visible to reference finding and rename";
        EXPECT_TRUE(HasUnusedDiagnostic(decl))
            << "A call label must not suppress an unused parameter diagnostic";
    }

    void ExpectVariantUsage(const Decl& decl, bool used)
    {
        auto original = FindDeclUsage(decl, package);
        bool external = std::any_of(original.begin(), original.end(), [&](Ptr<Node> reference) {
            return reference->begin != decl.begin || reference->end != decl.end;
        });
        ASSERT_EQ(external, used) << "Reference finder disagrees with the scenario expectation";
        EXPECT_EQ(HasUnusedDiagnostic(decl), !external)
            << "Enum detection must compare source references rather than generated positions";
    }
};

TEST_F(UnusedSymbolDiagTest, UsedParameterMatchesCompilerCloneBySourceLocation)
{
    auto function = AddFunction("add", 10);
    auto parameter = AddParam(*function, "id", 11);
    auto clone = Detached<FuncParam>("id", 11);
    Reference(*clone, 15);
    ASSERT_NE(parameter, clone);
    ExpectUsage(*parameter, true);
}

TEST_F(UnusedSymbolDiagTest, UsedLocalMatchesCompilerCloneBySourceLocation)
{
    auto function = AddFunction("work", 10);
    auto local = AddLocal(*function, "value", 12);
    auto clone = Detached<VarDecl>("value", 12);
    Reference(*clone, 15, true);
    ExpectUsage(*local, true);
}

TEST_F(UnusedSymbolDiagTest, SameNameAndPositionInDifferentFileDoesNotUseParameter)
{
    auto function = AddFunction("work", 10);
    auto parameter = AddParam(*function, "value", 11);
    auto otherFile = AddFile("other.cj");
    auto other = Detached<FuncParam>("value", 11, otherFile);
    Reference(*other, 15);
    ExpectUsage(*parameter, false);
}

TEST_F(UnusedSymbolDiagTest, SameNameInDifferentFunctionDoesNotUseParameter)
{
    auto first = AddFunction("first", 10);
    auto firstParameter = AddParam(*first, "value", 11);
    auto second = AddFunction("second", 20);
    auto secondParameter = AddParam(*second, "value", 21);
    Reference(*secondParameter, 25);
    ExpectUsage(*firstParameter, false);
    ExpectUsage(*secondParameter, true);
}

TEST_F(UnusedSymbolDiagTest, NamedArgumentToClonedFunctionDoesNotReadParameter)
{
    auto function = AddFunction("work", 10);
    auto parameter = AddParam(*function, "name", 11, true);
    auto clone = Detached<FuncDecl>("work", 10);
    // Source-identity matching must not require the clone to own parameters.
    NamedCall(*Reference(*clone, 20), "name");
    ExpectNamedLabelOnly(*parameter);
}

TEST_F(UnusedSymbolDiagTest, RestoredSourceCallLabelsDoNotReadParameters)
{
    auto function = AddFunction("work", 10);
    auto named = AddParam(*function, "name", 11, true);
    auto unused = AddParam(*function, "extra", 12, true);
    auto generated = Reference(*function, 30);
    auto source = SourceReference(*generated, 20);
    NamedCall(*source, "name");
    ExpectNamedLabelOnly(*named);
    ExpectUsage(*unused, false);
}

TEST_F(UnusedSymbolDiagTest, MacroFunctionTypedParameterMatchesTypeAndName)
{
    auto function = AddFunction("work", 10);
    auto parameter = AddParam(*function, "callback", 11);
    parameter->isInMacroCall = true;
    parameter->SetTy(&functionTy);
    auto target = Detached<VarDecl>("callback", 50);
    target->SetTy(&functionTy);
    Reference(*target, 55);
    ExpectUsage(*parameter, true);
}

TEST_F(UnusedSymbolDiagTest, NamedArgumentToMacroFunctionDoesNotReadParameter)
{
    auto function = AddFunction("work", 10);
    function->isInMacroCall = true;
    function->SetTy(&functionTy);
    auto parameter = AddParam(*function, "name", 11, true);
    auto target = Detached<FuncDecl>("work", 50);
    target->SetTy(&functionTy);
    NamedCall(*Reference(*target, 55), "name");
    ExpectNamedLabelOnly(*parameter);
}

TEST_F(UnusedSymbolDiagTest, NamedArgumentToForeignFunctionDoesNotReadParameter)
{
    auto function = AddFunction("work", 10);
    auto parameter = AddParam(*function, "name", 11, true);
    function->curFile = nullptr;
    auto target = Detached<FuncDecl>("work", 50);
    target->EnableAttr(Attribute::GLOBAL);
    target->funcBody = MakeOwned<FuncBody>();
    target->funcBody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto targetParameter = MakeOwned<FuncParam>();
    targetParameter->SetTy(&intTy);
    target->funcBody->paramLists[0]->params.emplace_back(std::move(targetParameter));
    NamedCall(*Reference(*target, 55), "name");
    ExpectNamedLabelOnly(*parameter);
}

TEST_F(UnusedSymbolDiagTest, EnumSourceSelfReferenceRemainsUnusedAtGeneratedExternalPosition)
{
    auto variant = AddVariant("Present", 10);
    auto generated = Reference(*variant, 30);
    SourceReference(*generated, 10);
    ExpectVariantUsage(*variant, false);
}

TEST_F(UnusedSymbolDiagTest, EnumSourceExternalReferenceIsUsedAtGeneratedSelfPosition)
{
    auto variant = AddVariant("Present", 10);
    auto generated = Reference(*variant, 10);
    SourceReference(*generated, 30);
    ExpectVariantUsage(*variant, true);
}

TEST_F(UnusedSymbolDiagTest, EnumMixedSelfAndExternalReferencesPreserveExternalUse)
{
    auto variant = AddVariant("Present", 10);
    auto clone = Detached<VarDecl>("Present", 10);
    Reference(*clone, 10);
    Reference(*clone, 30);
    Reference(*clone, 40, true);
    ExpectVariantUsage(*variant, true);
}

TEST_F(UnusedSymbolDiagTest, MacroMatchingDoesNotMarkNonMacroParameterWithSameTypeAndName)
{
    auto first = AddFunction("first", 10);
    auto ordinary = AddParam(*first, "callback", 11);
    ordinary->SetTy(&functionTy);
    auto second = AddFunction("second", 20);
    auto macro = AddParam(*second, "callback", 21);
    macro->isInMacroCall = true;
    macro->SetTy(&functionTy);
    auto target = Detached<VarDecl>("callback", 50);
    target->SetTy(&functionTy);
    Reference(*target, 55);
    ExpectUsage(*ordinary, false);
    ExpectUsage(*macro, true);
}

TEST_F(UnusedSymbolDiagTest, ForeignNamedFunctionDoesNotMergeSameLocationDifferentSignature)
{
    auto function = AddFunction("work", 10);
    auto parameter = AddParam(*function, "name", 11, true);
    function->curFile = nullptr;
    auto matching = Detached<FuncDecl>("work", 50);
    matching->EnableAttr(Attribute::GLOBAL);
    matching->funcBody = MakeOwned<FuncBody>();
    matching->funcBody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto matchingParameter = MakeOwned<FuncParam>();
    matchingParameter->SetTy(&intTy);
    matching->funcBody->paramLists[0]->params.emplace_back(std::move(matchingParameter));
    Reference(*matching, 55);

    auto unrelated = Detached<FuncDecl>("work", 50);
    unrelated->EnableAttr(Attribute::GLOBAL);
    unrelated->funcBody = MakeOwned<FuncBody>();
    unrelated->funcBody->paramLists.emplace_back(MakeOwned<FuncParamList>());
    auto unrelatedParameter = MakeOwned<FuncParam>();
    unrelatedParameter->SetTy(&unitTy);
    unrelated->funcBody->paramLists[0]->params.emplace_back(std::move(unrelatedParameter));
    NamedCall(*Reference(*unrelated, 65), "name");
    ExpectUsage(*parameter, false);
}

TEST_F(UnusedSymbolDiagTest, NamedArgumentValueReadsCallerParameterOnly)
{
    auto callee = AddFunction("callee", 10);
    auto calleeParameter = AddParam(*callee, "name", 11, true);
    auto caller = AddFunction("caller", 20);
    auto callerParameter = AddParam(*caller, "name", 21, true);
    NamedCall(*Reference(*callee, 25), "name");
    Reference(*callerParameter, 26);
    ExpectUsage(*callerParameter, true);
    ExpectNamedLabelOnly(*calleeParameter);
}

TEST_F(UnusedSymbolDiagTest, RecursiveNamedCallLabelDoesNotReadParameter)
{
    auto function = AddFunction("recursive", 10);
    auto parameter = AddParam(*function, "name", 11, true);
    NamedCall(*Reference(*function, 15), "name");
    ExpectNamedLabelOnly(*parameter);
}

TEST_F(UnusedSymbolDiagTest, RecursiveNamedCallValueReadsParameter)
{
    auto function = AddFunction("recursive", 10);
    auto parameter = AddParam(*function, "name", 11, true);
    NamedCall(*Reference(*function, 15), "name");
    auto clone = Detached<FuncParam>("name", 11);
    Reference(*clone, 16);
    ExpectUsage(*parameter, true);
}

} // namespace
} // namespace ark
