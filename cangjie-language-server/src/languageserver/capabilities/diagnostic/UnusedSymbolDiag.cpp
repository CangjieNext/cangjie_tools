// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "UnusedSymbolDiag.h"
#include "../../common/FindDeclUsage.h"
#include "../../common/Utils.h"

namespace ark {
using namespace Cangjie;
using namespace AST;
using namespace lsp;

// ---------------------------------------------------------------------------
// Helper: Try to add a macro-decorated decl symbol to the result set.
// Checks if the macro is in EXCLUDED_MACRO_NAMES (@Entry/@Component) and
// the declaration is a class/struct, then adds its SymbolID to result.
// ---------------------------------------------------------------------------
static void TryAddMacroDeclSymbol(
    MacroExpandDecl* macroExpandDecl,
    ASTKind astKind,
    Decl* decl,
    std::unordered_set<SymbolID>& result)
{
    if (!macroExpandDecl || !macroExpandDecl->invocation.decl) {
        return;
    }
    const auto& macroName = macroExpandDecl->invocation.macroCallDiagInfo.identifier;
    if (EXCLUDED_MACRO_NAMES.count(macroName) == 0) {
        return;
    }
    if (astKind != ASTKind::CLASS_DECL && astKind != ASTKind::STRUCT_DECL) {
        return;
    }
    if (!decl) {
        return;
    }
    auto symId = GetSymbolId(*decl);
    if (symId != INVALID_SYMBOL_ID) {
        (void)result.emplace(symId);
    }
}

// ---------------------------------------------------------------------------
// Collect SymbolIDs of class/struct declarations decorated with @Entry/@Component.
// Walks the file-level declarations of each file in the package.
// ---------------------------------------------------------------------------
std::unordered_set<SymbolID> UnusedSymbolDiag::CollectExcludedMacroDecls(
    const Package* package)
{
    std::unordered_set<SymbolID> result;
    if (!package) {
        return result;
    }

    for (auto& file : package->files) {
        if (!file) {
            continue;
        }
        for (auto& toplevelDecl : file->decls) {
            if (!toplevelDecl) {
                continue;
            }
            if (toplevelDecl->astKind == ASTKind::MACRO_EXPAND_DECL) {
                auto macroExpandDecl = dynamic_cast<MacroExpandDecl*>(toplevelDecl.get().get());
                auto innerDecl = macroExpandDecl ? macroExpandDecl->invocation.decl.get() : nullptr;
                TryAddMacroDeclSymbol(macroExpandDecl,
                    innerDecl ? innerDecl->astKind : static_cast<ASTKind>(0),
                    innerDecl, result);
            }
            if (toplevelDecl->curMacroCall && !toplevelDecl->isInMacroCall) {
                auto* macroExpandDecl = dynamic_cast<MacroExpandDecl*>(toplevelDecl->curMacroCall.get());
                TryAddMacroDeclSymbol(macroExpandDecl, toplevelDecl->astKind,
                    DynamicCast<Decl*>(toplevelDecl.get()), result);
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helper: Check if symbol should be excluded based on type or modifier.
// Handles simple exclusion rules that don't require relation/macro analysis.
// ---------------------------------------------------------------------------
static bool ShouldExcludeByTypeOrModifier(const Symbol& symbol)
{
    if ((symbol.modifier == Modifier::PUBLIC || symbol.modifier == Modifier::PROTECTED) &&
        symbol.kind != ASTKind::TYPE_ALIAS_DECL) {
        return true;
    }

    if (symbol.symInfo.kind == lsp::SymbolKind::CONSTRUCTOR ||
        symbol.symInfo.kind == lsp::SymbolKind::DESTRUCTOR) {
        return true;
    }

    if (symbol.kind == ASTKind::MAIN_DECL ||
        (symbol.kind == ASTKind::FUNC_DECL && symbol.name == "main")) {
        return true;
    }

    if (symbol.isMemberParam) {
        return true;
    }

    if (symbol.isCjoSym) {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Determine whether a symbol should be excluded from unused detection.
// ---------------------------------------------------------------------------
bool UnusedSymbolDiag::ShouldExclude(
    const Symbol& symbol,
    const SymbolIndex& index,
    const std::unordered_set<SymbolID>& excludedMacroDecls)
{
    if (ShouldExcludeByTypeOrModifier(symbol)) {
        return true;
    }

    if (symbol.kind == ASTKind::FUNC_DECL || symbol.kind == ASTKind::PROP_DECL) {
        if (index.IsSymbolOverridden(symbol.id)) {
            return true;
        }
    }

    if (excludedMacroDecls.count(symbol.id) > 0) {
        return true;
    }

    SymbolID containerId = index.GetSymbolContainerId(symbol.id);
    if (containerId != INVALID_SYMBOL_ID && excludedMacroDecls.count(containerId) > 0) {
        if (symbol.kind != ASTKind::VAR_DECL) {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Get a human-readable description for an ASTKind
// ---------------------------------------------------------------------------
std::string UnusedSymbolDiag::GetKindDescription(ASTKind kind)
{
    switch (kind) {
        case ASTKind::FUNC_DECL:
            return "Function";
        case ASTKind::VAR_DECL:
            return "Variable";
        case ASTKind::LAMBDA_EXPR:
            return "Lambda";
        case ASTKind::CLASS_DECL:
            return "Class";
        case ASTKind::STRUCT_DECL:
            return "Struct";
        case ASTKind::ENUM_DECL:
            return "Enum";
        case ASTKind::INTERFACE_DECL:
            return "Interface";
        case ASTKind::TYPE_ALIAS_DECL:
            return "Type alias";
        default:
            return "Symbol";
    }
}

// ---------------------------------------------------------------------------
// Create a DiagnosticToken for an unused symbol
// ---------------------------------------------------------------------------
DiagnosticToken UnusedSymbolDiag::CreateUnusedDiag(
    const std::string& name,
    const SymbolLocation& location,
    const std::string& symbolKindDesc)
{
    DiagnosticToken diagToken;
    ark::Range lspRange = TransformFromChar2IDE({
        {location.begin.fileID, location.begin.line, location.begin.column},
        {location.end.fileID, location.end.line, location.end.column}
    });
    diagToken.range = {
        {lspRange.start.fileID, lspRange.start.line, lspRange.start.column},
        {lspRange.end.fileID, lspRange.end.line, lspRange.end.column}
    };
    diagToken.severity = static_cast<int>(DiagLSPSeverity::HINT);  // 4 = Hint
    diagToken.tags = {1};  // DiagnosticTag.Unnecessary = 1 (editor renders as faded/grey)
    diagToken.source = "Cangjie";
    diagToken.message = symbolKindDesc + " '" + name + "' is declared but never used";
    diagToken.code = 0;
    diagToken.diagFix = DiagFix{false, false, true};  // Enable removeUnusedSymbol QuickFix
    return diagToken;
}

// ---------------------------------------------------------------------------
// Main analysis: detect unused global/member symbols from the index
// ---------------------------------------------------------------------------
std::vector<DiagnosticToken> UnusedSymbolDiag::Analyze(
    const SymbolIndex& index,
    const std::string& pkgName,
    const std::string& filePath,
    const Package* package)
{
    std::vector<DiagnosticToken> diagnostics;

    auto excludedMacroDecls = CollectExcludedMacroDecls(package);

    index.ForEachFileSymbol(pkgName, filePath, [&](const Symbol& sym) -> bool {
        if (UNUSED_CHECK_KINDS.count(sym.kind) == 0) {
            return true;
        }
        if (sym.location.IsZeroLoc()) {
            return true;
        }
        if (sym.id == INVALID_SYMBOL_ID) {
            return true;
        }
        if (ShouldExclude(sym, index, excludedMacroDecls)) {
            return true;
        }

        bool hasRef = index.HasSymbolReference(sym.id);
        if (!hasRef && (sym.kind == ASTKind::CLASS_DECL || sym.kind == ASTKind::STRUCT_DECL)) {
            hasRef = index.HasReferencedConstructorChild(sym.id);
        }
        if (!hasRef) {
            diagnostics.push_back(CreateUnusedDiag(
                sym.name, sym.location, GetKindDescription(sym.kind)));
        }
        return true;
    });

    return diagnostics;
}

// ---------------------------------------------------------------------------
// Helper: Check if FuncParam should be skipped from unused detection.
// Skips named params, override methods, abstract/open methods, and interface methods.
// ---------------------------------------------------------------------------
static bool ShouldSkipFuncParam(const FuncParam* funcParam)
{
    if (funcParam->isNamedParam) {
        return false;
    }

    if (!funcParam->outerDecl) {
        return false;
    }

    auto outerFunc = DynamicCast<const FuncDecl*>(funcParam->outerDecl.get());
    if (!outerFunc) {
        return false;
    }

    if (outerFunc->TestAttr(Attribute::REDEF)) {
        return true;
    }

    if (outerFunc->TestAnyAttr(Attribute::ABSTRACT, Attribute::OPEN)) {
        return true;
    }

    if (outerFunc->outerDecl && outerFunc->outerDecl->astKind == ASTKind::ENUM_DECL) {
        return true;
    }

    if (outerFunc->outerDecl && outerFunc->outerDecl->astKind == ASTKind::INTERFACE_DECL) {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Helper: Check and report unused local variable.
// ---------------------------------------------------------------------------
static void CheckUnusedVarDecl(VarDecl* varDecl, Package& package,
    const std::string& filePath, std::vector<DiagnosticToken>& diagnostics)
{
    bool isGlobalOrMember = IsGlobalOrMember(*varDecl);
    // For macro-expanded declarations (e.g., member variables in @Component/@Entry classes),
    // also check if they have curMacroCall set and have valid outerDecl
    if (!isGlobalOrMember && varDecl->curMacroCall && !varDecl->identifier.Empty() && varDecl->outerDecl) {
        isGlobalOrMember = true;
    }
    if (!isGlobalOrMember) {
        auto usages = FindDeclUsage(*varDecl, package);
        if (usages.empty()) {
            auto identifierPos = varDecl->GetIdentifierPos();
            SymbolLocation loc{
                identifierPos,
                identifierPos + CountUnicodeCharacters(varDecl->identifier),
                filePath
            };
            diagnostics.push_back(UnusedSymbolDiag::CreateUnusedDiag(
                varDecl->identifier, loc, "Variable"));
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: Check if a parameter is auto-generated by @Component/@Entry macros.
// These parameters (elmtId, isInitialRender) are added by the macro system
// and users cannot control them, so they should not be reported as unused.
// ---------------------------------------------------------------------------
static bool IsComponentGeneratedParam(const FuncParam* funcParam)
{
    if (!funcParam || !funcParam->curMacroCall || funcParam->isInMacroCall) {
        return false;
    }

    // Check if this is a known @Component/@Entry generated parameter name
    static const std::unordered_set<std::string> componentParams = {
        "elmtId",
        "isInitialRender"
    };

    return componentParams.count(std::string(funcParam->identifier)) > 0;
}

// ---------------------------------------------------------------------------
// Helper: Check and report unused function parameter.
// ---------------------------------------------------------------------------
static void CheckUnusedFuncParam(FuncParam* funcParam, Package& package,
    const std::string& filePath, std::vector<DiagnosticToken>& diagnostics)
{
    // Skip @Component/@Entry auto-generated parameters
    if (IsComponentGeneratedParam(funcParam)) {
        return;
    }

    auto usages = FindDeclUsage(*funcParam, package);
    if (!usages.empty()) {
        return;
    }

    auto identifierPos = funcParam->GetIdentifierPos();
    SymbolLocation loc{
        identifierPos,
        identifierPos + CountUnicodeCharacters(funcParam->identifier),
        filePath
    };
    diagnostics.push_back(UnusedSymbolDiag::CreateUnusedDiag(
        funcParam->identifier, loc, "Parameter"));
}

// ---------------------------------------------------------------------------
// Helper: Check and report unused enum variant (enum constructor).
// Enum variants can be FuncDecl (with associated values) or VarDecl
// (without associated values), both stored in EnumDecl::constructors.
// ---------------------------------------------------------------------------
static void CheckUnusedEnumVariantCommon(Decl& decl, Package& package,
    const std::string& filePath, std::vector<DiagnosticToken>& diagnostics)
{
    auto usages = FindDeclUsage(decl, package);
    bool hasExternalUsage = false;
    for (auto& usage : usages) {
        if (usage->begin != decl.begin || usage->end != decl.end) {
            hasExternalUsage = true;
            break;
        }
    }
    if (hasExternalUsage) {
        return;
    }
    auto identifierPos = decl.GetIdentifierPos();
    SymbolLocation loc{
        identifierPos,
        identifierPos + CountUnicodeCharacters(decl.identifier),
        filePath
    };
    diagnostics.push_back(UnusedSymbolDiag::CreateUnusedDiag(
        decl.identifier, loc, "Enum variant"));
}

// ---------------------------------------------------------------------------
// Walker visitor for local symbol analysis
// ---------------------------------------------------------------------------
static VisitAction CollectUnusedLocalDiags(
    Ptr<Node> node,
    Package& package,
    const std::string& filePath,
    std::vector<DiagnosticToken>& diagnostics)
{
    auto decl = DynamicCast<Decl*>(node);
    if (!decl) {
        return VisitAction::WALK_CHILDREN;
    }

    if (decl->isInMacroCall) {
        return VisitAction::WALK_CHILDREN;
    }

    if (auto varDecl = DynamicCast<VarDecl*>(node)) {
        if (!DynamicCast<FuncParam*>(node)) {
            CheckUnusedVarDecl(varDecl, package, filePath, diagnostics);
        }
    }

    if (auto funcParam = DynamicCast<FuncParam*>(node)) {
        if (ShouldSkipFuncParam(funcParam)) {
            return VisitAction::WALK_CHILDREN;
        }
        CheckUnusedFuncParam(funcParam, package, filePath, diagnostics);
    }

    if (auto enumDecl = DynamicCast<EnumDecl*>(node)) {
        for (auto& ctor : enumDecl->constructors) {
            if (ctor && !ctor->isInMacroCall) {
                CheckUnusedEnumVariantCommon(*ctor, package, filePath, diagnostics);
            }
        }
    }

    return VisitAction::WALK_CHILDREN;
}

// ---------------------------------------------------------------------------
// Local symbol analysis: detect unused local variables and non-named params
// ---------------------------------------------------------------------------
std::vector<DiagnosticToken> UnusedSymbolDiag::AnalyzeLocalSymbols(
    const File& file,
    Package& package)
{
    std::vector<DiagnosticToken> diagnostics;

    if (!file.curPackage) {
        return diagnostics;
    }

    std::function<VisitAction(Ptr<Node>)> collector =
        [&](Ptr<Node> node) -> VisitAction {
            return CollectUnusedLocalDiags(node, package, file.filePath, diagnostics);
        };

    Walker(const_cast<File*>(&file), collector).Walk();
    return diagnostics;
}

} // namespace ark
