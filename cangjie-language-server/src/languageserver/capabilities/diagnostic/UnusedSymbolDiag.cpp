// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "UnusedSymbolDiag.h"
#include "../../CompilerCangjieProject.h"
#include "../../common/FindDeclUsage.h"
#include "../../common/Utils.h"
#include "cangjie/AST/Node.h"
#include "cangjie/AST/Walker.h"
#include <unordered_map>

namespace ark {
using namespace Cangjie;
using namespace AST;
using namespace lsp;

// ---------------------------------------------------------------------------
// Constants for macro-generated symbol patterns
// ---------------------------------------------------------------------------
namespace {
    // Macro names
    constexpr const char* COMPONENT_MACRO = "Component";
    constexpr const char* ENTRY_MACRO = "Entry";

    // Generated variable prefixes
    constexpr const char* STATE_VAR_DECL_PREFIX = "stateVarDecl_";
    constexpr const char* A_P_PREFIX = "A_P_";

    // Macro-generated function name patterns
    constexpr const char* GET_PREFIX = "get_";
    constexpr const char* SET_PREFIX = "set_";
    constexpr const char* UNDERSCORE_PREFIX = "__";
    constexpr const char* ABOUT_TO_PREFIX = "aboutTo";
    constexpr const char* PURGE_PREFIX = "purge";
    constexpr const char* UPDATE_WITH_VALUE_PARAMS = "updateWithValueParams";
    constexpr const char* INIT_FUNC = "init";
    constexpr const char* ABOUT_TO_APPEAR = "aboutToAppear";
    constexpr const char* ABOUT_TO_DISAPPEAR = "aboutToDisappear";
}

namespace {
bool MatchesRawUnusedDiagnostic(Ptr<const Decl> decl, const DiagnosticToken& diagnostic, const ArkAST& ast)
{
    if (!decl || !decl->identifier.IsRaw() || decl->curMacroCall || decl->isInMacroCall ||
        decl->identifier.Begin().fileID != ast.fileID) {
        return false;
    }
    auto pos = decl->GetIdentifierPos();
    if (decl->astKind != ASTKind::FUNC_PARAM && !decl->TestAttr(Attribute::ENUM_CONSTRUCTOR) &&
        (IsGlobalOrMember(*decl) || decl->astKind == ASTKind::FUNC_DECL ||
        (isLambda(*decl) && diagnostic.message.find("Lambda '") == 0))) {
        PositionUTF8ToIDE(ast.tokens, pos, *decl);
    }
    const auto name = std::string(decl->identifier);
    return pos.line == diagnostic.range.start.line + 1 && pos.column == diagnostic.range.start.column + 1 &&
        diagnostic.message.find("'" + name + "'") != std::string::npos;
}

std::optional<int> GetRawUnusedDiagnosticLength(const DiagnosticToken& diagnostic, const ArkAST& ast)
{
    if (!diagnostic.diagFix || !diagnostic.diagFix->removeUnusedSymbol || !ast.sourceManager || !ast.file) {
        return std::nullopt;
    }
    std::optional<int> length;
    ConstWalker(ast.file, [&](Ptr<const Node> node) {
        auto decl = DynamicCast<const Decl*>(node);
        if (!MatchesRawUnusedDiagnostic(decl, diagnostic, ast)) {
            return VisitAction::WALK_CHILDREN;
        }
        const auto begin = decl->identifier.Begin();
        const auto end = decl->identifier.End();
        if (begin.line <= 0 || begin.column <= 0 || end.fileID != begin.fileID || end.line != begin.line ||
            end.column <= begin.column) {
            return VisitAction::WALK_CHILDREN;
        }
        const auto sourceName = ast.sourceManager->GetContentBetween(begin, end);
        if (sourceName.empty() || sourceName.size() != static_cast<size_t>(end.column - begin.column)) {
            return VisitAction::WALK_CHILDREN;
        }
        length = CountUnicodeCharacters(sourceName);
        return VisitAction::STOP_NOW;
    }).Walk();
    return length;
}

void UpdateRawUnusedDiagnosticRanges(std::vector<DiagnosticToken>& diagnostics, const std::string& filePath)
{
    if (diagnostics.empty()) {
        return;
    }
    auto* project = CompilerCangjieProject::GetInstance();
    auto* ast = project ? project->GetArkAST(filePath) : nullptr;
    if (!ast || !std::any_of(ast->tokens.begin(), ast->tokens.end(), [](const Token& token) {
        return token.Value().find('`') != std::string::npos;
    })) {
        return;
    }
    for (auto& diagnostic : diagnostics) {
        if (auto length = GetRawUnusedDiagnosticLength(diagnostic, *ast)) {
            // Use the source spelling for the length without changing the diagnostic start.
            diagnostic.range.end.column = diagnostic.range.start.column + *length;
        }
    }
}
} // namespace

// ---------------------------------------------------------------------------
// Helper: Check if a function name matches macro-generated patterns
// ---------------------------------------------------------------------------
static bool IsMacroGeneratedFuncName(const std::string& funcName)
{
    static const std::unordered_set<std::string> exactMatches = {
        INIT_FUNC,
        ABOUT_TO_APPEAR,
        ABOUT_TO_DISAPPEAR,
        UPDATE_WITH_VALUE_PARAMS
    };

    return exactMatches.count(funcName) > 0 ||
           funcName.find(GET_PREFIX) == 0 ||
           funcName.find(SET_PREFIX) == 0 ||
           funcName.find(UNDERSCORE_PREFIX) == 0 ||
           funcName.find(ABOUT_TO_PREFIX) == 0 ||
           funcName.find(PURGE_PREFIX) == 0 ||
           funcName.find('.') != std::string::npos;
}

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
    if ((symbol.modifier == lsp::Modifier::PUBLIC || symbol.modifier == lsp::Modifier::PROTECTED) &&
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

    if ((symbol.kind == ASTKind::CLASS_DECL || symbol.kind == ASTKind::STRUCT_DECL) &&
        excludedMacroDecls.count(symbol.id) > 0) {
        return true;
    }

    // Skip @Component/@Entry generated internal symbols
    auto symName = symbol.name;

    // Skip variables with specific prefixes
    if (symName.find(STATE_VAR_DECL_PREFIX) == 0 || symName.find(A_P_PREFIX) == 0) {
        return true;
    }

    // Skip macro-generated functions
    if (symbol.kind == ASTKind::FUNC_DECL && IsMacroGeneratedFuncName(symName)) {
        return true;
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

    UpdateRawUnusedDiagnosticRanges(diagnostics, filePath);
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
// Helper: Check if a node is in @Component/@Entry macro-expanded code
// ---------------------------------------------------------------------------
static bool IsInComponentMacro(const Node* node)
{
    if (!node || !node->curMacroCall || node->isInMacroCall) {
        return false;
    }

    // Check if the macro is @Component or @Entry
    auto macroCall = node->curMacroCall.get();
    if (!macroCall) {
        return false;
    }

    auto invocation = macroCall->GetConstInvocation();
    if (!invocation || !invocation->target) {
        return false;
    }

    auto macroName = invocation->target->identifier;
    return macroName == COMPONENT_MACRO || macroName == ENTRY_MACRO;
}

// ---------------------------------------------------------------------------
// Collect only declarations that the current file's diagnostics will query.
// The package walk resolves their original FindDeclUsage identities once.
// ---------------------------------------------------------------------------
namespace {
enum class LocalSymbolKind : uint8_t { NONE, VARIABLE, PARAMETER, ENUM_VARIANT };

struct UsageQuery {
    const Decl* decl;
    uint32_t firstNamedParam = UINT32_MAX;
    LocalSymbolKind kind = LocalSymbolKind::NONE;
    bool used = false;
};

struct NamedParamLink {
    uint32_t parameter;
    uint32_t next;
};

static size_t LocationHash(const Decl& decl)
{
    return std::hash<uint64_t>{}(decl.begin.Hash64()) ^
        (std::hash<uint64_t>{}(decl.end.Hash64()) << 1) ^
        (std::hash<const File*>{}(decl.curFile.get()) << 2);
}

static bool SameLocation(const Decl& source, const Decl& target)
{
    return source.begin == target.begin && source.end == target.end && source.curFile == target.curFile;
}

static size_t MacroIdentityHash(const Decl& decl)
{
    return std::hash<const Ty*>{}(decl.GetTy().get()) ^
        (std::hash<std::string>{}(decl.identifier.Val()) << 1);
}

struct UsageSets {
    explicit UsageSets(const Package& package) : searchingPackage(&package) {}

    const Package* searchingPackage;
    // Keep one compact state per query. Hashed indices store only the hash and
    // query offset; equality is checked against the original declaration.
    std::vector<UsageQuery> queries;
    std::vector<NamedParamLink> namedParams;
    std::unordered_map<const Decl*, uint32_t> namedFunctions;
    std::unordered_multimap<size_t, uint32_t> byLocation;
    std::unordered_multimap<size_t, uint32_t> byMacroFunc;
    std::unordered_multimap<std::string, uint32_t> byExceptionalName;
    const File* queryFile = nullptr;
    bool singleFile = true;

    uint32_t AddQuery(const Decl& decl, LocalSymbolKind kind)
    {
        auto index = static_cast<uint32_t>(queries.size());
        queries.push_back({&decl, UINT32_MAX, kind, false});
        return index;
    }

    void Add(const Decl& decl, LocalSymbolKind kind)
    {
        auto parameterIndex = AddQuery(decl, kind);
        if (kind != LocalSymbolKind::PARAMETER) {
            return;
        }
        auto param = DynamicCast<const FuncParam*>(&decl);
        if (!param || !param->isNamedParam || !param->outerDecl ||
            param->outerDecl->astKind != ASTKind::FUNC_DECL) {
            return;
        }
        auto function = param->outerDecl.get();
        auto match = namedFunctions.find(function);
        uint32_t functionIndex;
        if (match == namedFunctions.end()) {
            functionIndex = AddQuery(*function, LocalSymbolKind::NONE);
            namedFunctions.emplace(function, functionIndex);
        } else {
            functionIndex = match->second;
        }
        auto previous = queries[functionIndex].firstNamedParam;
        queries[functionIndex].firstNamedParam = static_cast<uint32_t>(namedParams.size());
        namedParams.push_back({parameterIndex, previous});
    }

    void BuildIndex()
    {
        queries.shrink_to_fit();
        byLocation.reserve(queries.size());
        queryFile = queries.empty() ? nullptr : queries.front().decl->curFile.get();
        for (uint32_t i = 0; i < queries.size(); ++i) {
            const auto& decl = *queries[i].decl;
            singleFile = singleFile && decl.curFile.get() == queryFile;
            byLocation.emplace(LocationHash(decl), i);
            if (decl.isInMacroCall) {
                auto ty = decl.GetTy();
                if (ty && ty->kind == TypeKind::TYPE_FUNC) {
                    byMacroFunc.emplace(MacroIdentityHash(decl), i);
                }
            }
            bool inSamePkg = decl.curFile && decl.curFile->curPackage == searchingPackage;
            bool partialDecl = (!decl.outerDecl && !decl.TestAttr(Attribute::GLOBAL)) ||
                (decl.outerDecl && decl.outerDecl->astKind == ASTKind::FUNC_DECL);
            if (!inSamePkg && !partialDecl) {
                byExceptionalName.emplace(GetDefinedDecl(&decl)->identifier.Val(), i);
            }
        }
    }

    void Match(uint32_t index, Ptr<Node> node, Ptr<Node>& realNode)
    {
        auto& query = queries[index];
        if (!query.used && query.kind != LocalSymbolKind::NONE) {
            if (query.kind == LocalSymbolKind::ENUM_VARIANT) {
                if (!realNode) {
                    realNode = GetRealNode(node);
                }
                query.used = realNode->begin != query.decl->begin || realNode->end != query.decl->end;
            } else {
                query.used = true;
            }
        }
        while (query.firstNamedParam != UINT32_MAX &&
            queries[namedParams[query.firstNamedParam].parameter].used) {
            query.firstNamedParam = namedParams[query.firstNamedParam].next;
        }
        if (query.firstNamedParam == UINT32_MAX) {
            if (query.kind == LocalSymbolKind::NONE) {
                query.used = true;
            }
            return;
        }
        if (!realNode) {
            realNode = GetRealNode(node);
        }
        auto ref = DynamicCast<NameReferenceExpr*>(realNode);
        auto call = ref ? DynamicCast<CallExpr*>(ref->callOrPattern) : nullptr;
        if (!call || call->args.empty()) {
            return;
        }
        for (auto link = query.firstNamedParam; link != UINT32_MAX; link = namedParams[link].next) {
            auto& parameter = queries[namedParams[link].parameter];
            if (parameter.used) {
                continue;
            }
            for (auto& arg : call->args) {
                if (arg && arg->name == parameter.decl->identifier) {
                    parameter.used = true;
                    break;
                }
            }
        }
        while (query.firstNamedParam != UINT32_MAX &&
            queries[namedParams[query.firstNamedParam].parameter].used) {
            query.firstNamedParam = namedParams[query.firstNamedParam].next;
        }
        if (query.kind == LocalSymbolKind::NONE && query.firstNamedParam == UINT32_MAX) {
            query.used = true;
        }
    }

    void Record(const Decl& target, Ptr<Node> node)
    {
        Ptr<Node> realNode;
        if (!singleFile || target.curFile.get() == queryFile) {
            auto locations = byLocation.equal_range(LocationHash(target));
            for (auto it = locations.first; it != locations.second; ++it) {
                auto& query = queries[it->second];
                if (query.used && query.firstNamedParam == UINT32_MAX) {
                    continue;
                }
                if (query.decl == &target || SameLocation(*query.decl, target)) {
                    Match(it->second, node, realNode);
                }
            }
        }
        if (!byMacroFunc.empty()) {
            auto ty = target.GetTy();
            if (ty && ty->kind == TypeKind::TYPE_FUNC) {
                auto macroFunctions = byMacroFunc.equal_range(MacroIdentityHash(target));
                for (auto it = macroFunctions.first; it != macroFunctions.second; ++it) {
                    auto& query = queries[it->second];
                    if (query.used && query.firstNamedParam == UINT32_MAX) {
                        continue;
                    }
                    auto& decl = *query.decl;
                    if (decl.GetTy() == ty && decl.identifier.Val() == target.identifier.Val()) {
                        Match(it->second, node, realNode);
                    }
                }
            }
        }
        if (!byExceptionalName.empty()) {
            auto names = byExceptionalName.equal_range(GetDefinedDecl(&target)->identifier.Val());
            for (auto it = names.first; it != names.second; ++it) {
                auto& query = queries[it->second];
                if (query.used && query.firstNamedParam == UINT32_MAX) {
                    continue;
                }
                if (CheckDeclEqual(*query.decl, target)) {
                    Match(it->second, node, realNode);
                }
            }
        }
    }

    void ReleaseIndex()
    {
        decltype(byLocation){}.swap(byLocation);
        decltype(byMacroFunc){}.swap(byMacroFunc);
        decltype(byExceptionalName){}.swap(byExceptionalName);
        decltype(namedFunctions){}.swap(namedFunctions);
        decltype(namedParams){}.swap(namedParams);
    }

    void AppendDiagnostics(const std::string& filePath, std::vector<DiagnosticToken>& diagnostics) const
    {
        for (auto& query : queries) {
            if (query.used || query.kind == LocalSymbolKind::NONE) {
                continue;
            }
            auto& decl = *query.decl;
            auto position = decl.GetIdentifierPos();
            SymbolLocation loc{position, position + CountUnicodeCharacters(decl.identifier), filePath};
            const char* description = query.kind == LocalSymbolKind::VARIABLE ? "Variable" :
                query.kind == LocalSymbolKind::PARAMETER ? "Parameter" : "Enum variant";
            diagnostics.push_back(UnusedSymbolDiag::CreateUnusedDiag(decl.identifier, loc, description));
        }
    }
};
} // namespace

// ---------------------------------------------------------------------------
// Helper: Collect a local variable query.
// ---------------------------------------------------------------------------
static void CollectVarQuery(VarDecl* varDecl,
    UsageSets& usage)
{
    // Skip variables in @Component/@Entry macro-expanded code
    if (IsInComponentMacro(varDecl)) {
        return;
    }

    // Skip @Component/@Entry generated internal variables
    std::string varName(varDecl->identifier);
    if (varName.find(STATE_VAR_DECL_PREFIX) == 0) {
        return;
    }

    if (!IsGlobalOrMember(*varDecl)) {
        usage.Add(*varDecl, LocalSymbolKind::VARIABLE);
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
// Helper: Check if a FuncParam belongs to a macro-generated method.
// Macro-generated methods: get_*, set_*, __*, aboutTo*, purge*, init, updateWithValueParams
// Also includes functions with '.' in name (e.g., bb.1, cc.2) generated by @Component/@Entry
// ---------------------------------------------------------------------------
static bool IsInMacroGeneratedFunc(const FuncParam* funcParam)
{
    if (!funcParam || !funcParam->outerDecl) {
        return false;
    }
    auto outerFunc = DynamicCast<const FuncDecl*>(funcParam->outerDecl.get());
    if (!outerFunc) {
        return false;
    }

    return IsMacroGeneratedFuncName(outerFunc->identifier);
}

// ---------------------------------------------------------------------------
// Helper: Collect a function parameter query.
// ---------------------------------------------------------------------------
static void CollectParamQuery(FuncParam* funcParam,
    UsageSets& usage)
{
    bool isComponentGen = IsComponentGeneratedParam(funcParam);
    bool isInCompMacro = IsInComponentMacro(funcParam);
    bool isInMacroGenFunc = IsInMacroGeneratedFunc(funcParam);

    // Skip @Component/@Entry auto-generated parameters
    if (isComponentGen) {
        return;
    }

    // Skip parameters in @Component/@Entry macro-expanded code,
    // BUT only if they are in macro-generated methods.
    // User-defined methods (like name(dd:String)) should still be checked.
    if (isInCompMacro && isInMacroGenFunc) {
        return;
    }

    usage.Add(*funcParam, LocalSymbolKind::PARAMETER);
}

// ---------------------------------------------------------------------------
// Helper: Collect an enum variant query (enum constructor).
// Enum variants can be FuncDecl (with associated values) or VarDecl
// (without associated values), both stored in EnumDecl::constructors.
// ---------------------------------------------------------------------------
static void CollectEnumQuery(Decl& decl,
    const SymbolIndex* index, UsageSets& usage)
{
    if (index != nullptr) {
        auto symId = GetDeclSymbolID(decl);
        if (symId != INVALID_SYMBOL_ID && index->HasSymbolReference(symId)) {
            return;
        }
    }
    usage.Add(decl, LocalSymbolKind::ENUM_VARIANT);
}

// ---------------------------------------------------------------------------
// Walker visitor for local symbol analysis
// ---------------------------------------------------------------------------
static void HandleMacroCallFuncParam(FuncParam* funcParam,
    UsageSets& usage)
{
    if (!funcParam || funcParam->isMemberParam) {
        return;
    }
    if (!IsInMacroGeneratedFunc(funcParam) && !ShouldSkipFuncParam(funcParam)) {
        CollectParamQuery(funcParam, usage);
    }
}

static void HandleVarDecl(Ptr<Node> node,
    UsageSets& usage)
{
    auto varDecl = DynamicCast<VarDecl*>(node);
    if (varDecl && !DynamicCast<FuncParam*>(node)) {
        CollectVarQuery(varDecl, usage);
    }
}

static void HandleFuncParam(FuncParam* funcParam,
    UsageSets& usage)
{
    // Skip member params (e.g. `public let a: String` in primary constructors).
    // These are class members handled by the index layer (Analyze() checks
    // ShouldExclude -> symbol.isMemberParam), not local symbols.
    // HandleMacroCallFuncParam already checks isMemberParam; this mirrors
    // that check for the non-macro-call path.
    if (!funcParam) {
        return;
    }
    if (!ShouldSkipFuncParam(funcParam)) {
        CollectParamQuery(funcParam, usage);
    }
}

static void HandleEnumDecl(Ptr<Node> node,
    const SymbolIndex* index, UsageSets& usage)
{
    auto enumDecl = DynamicCast<EnumDecl*>(node);
    if (!enumDecl) {
        return;
    }
    for (auto& ctor : enumDecl->constructors) {
        if (ctor && !ctor->isInMacroCall) {
            CollectEnumQuery(*ctor, index, usage);
        }
    }
}

static VisitAction CollectLocalQueries(
    Ptr<Node> node,
    const SymbolIndex* index,
    UsageSets& usage)
{
    auto decl = DynamicCast<Decl*>(node);
    if (!decl || decl->TestAttr(Attribute::IMPLICIT_ADD)) {
        return VisitAction::WALK_CHILDREN;
    }

    if (decl->astKind == ASTKind::PRIMARY_CTOR_DECL) {
        return VisitAction::SKIP_CHILDREN;
    }

    auto funcParam = DynamicCast<FuncParam*>(node);

    if (decl->isInMacroCall) {
        HandleMacroCallFuncParam(funcParam, usage);
        return VisitAction::WALK_CHILDREN;
    }

    HandleVarDecl(node, usage);
    HandleFuncParam(funcParam, usage);
    HandleEnumDecl(node, index, usage);

    return VisitAction::WALK_CHILDREN;
}

// ---------------------------------------------------------------------------
// Walk the package once, preserving FindUsage's pointer, source-location and
// macro-function matches. Exceptional declarations outside the source package
// retain CheckDeclEqual matching through an index of referenced names.
// Named arguments and enum self-position filtering use GetRealNode, just as
// FindDeclUsage does, so desugared expressions retain their source semantics.
// ---------------------------------------------------------------------------
static void BuildUsageSets(Package& package, UsageSets& usage)
{
    std::function<VisitAction(Ptr<Node>)> collector =
        [&usage, &collector](Ptr<Node> node) -> VisitAction {
        // File nodes: recurse into macro-expanded and trashBin subtrees,
        // matching FindUsage's behavior so macro/desugar refs are captured.
        if (auto fileNode = DynamicCast<File*>(node)) {
            for (auto& it : fileNode->originalMacroCallNodes) {
                Walker(it.get(), collector).Walk();
            }
            for (auto& it : fileNode->trashBin) {
                Walker(it.get(), collector).Walk();
            }
        }
        if (auto target = node->GetTarget()) {
            usage.Record(*target, node);
        }
        return VisitAction::WALK_CHILDREN;
    };
    Walker(&package, collector).Walk();
}

// ---------------------------------------------------------------------------
// Local symbol analysis: detect unused local variables and function params
// ---------------------------------------------------------------------------
std::vector<DiagnosticToken> UnusedSymbolDiag::AnalyzeLocalSymbols(
    const File& file,
    Package& package,
    const SymbolIndex* index)
{
    std::vector<DiagnosticToken> diagnostics;

    if (!file.curPackage) {
        return diagnostics;
    }

    // The same two AST walks as the optimized implementation: collect the
    // current file's actual queries first, then resolve all package references.
    UsageSets usage(package);
    std::function<VisitAction(Ptr<Node>)> collector =
        [&](Ptr<Node> node) -> VisitAction {
            return CollectLocalQueries(node, index, usage);
        };
    Walker(const_cast<File*>(&file), collector).Walk();
    if (usage.queries.empty()) {
        return diagnostics;
    }
    usage.BuildIndex();
    BuildUsageSets(package, usage);
    usage.ReleaseIndex();
    usage.AppendDiagnostics(file.filePath, diagnostics);
    UpdateRawUnusedDiagnosticRanges(diagnostics, file.filePath);
    return diagnostics;
}

} // namespace ark
