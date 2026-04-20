// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "LSPDiagObserver.h"
#include "../../CompilerCangjieProject.h"
#include <algorithm>

namespace ark {
using namespace Cangjie;

static bool IsSameDiagId(DiagId lhs, DiagId rhs)
{
    return lhs.Val() == rhs.Val();
}

static std::string_view GetCategory(DiagCategory category)
{
    switch (category) {
        case DiagCategory::LEX: {
            return "lex";
        }
        case DiagCategory::PARSE: {
            return "parse";
        }
        case DiagCategory::PARSE_QUERY: {
            return "parse_query";
        }
        case DiagCategory::CONDITIONAL_COMPILATION: {
            return "conditional_compilation";
        }
        case DiagCategory::IMPORT_PACKAGE: {
            return "import";
        }
        case DiagCategory::MODULE: {
            return "module";
        }
        case DiagCategory::MACRO_EXPAND: {
            return "macro";
        }
        case DiagCategory::SEMA: {
            return "sema";
        }
        case DiagCategory::CHIR: {
            return "chir";
        }
        case DiagCategory::OTHER:
        default:
            return "other";
    }
}

static constexpr std::string_view DK_SEMA_UNDECLARED_IDENTIFIER = "sema_undeclared_identifier";
static constexpr std::string_view DK_SEMA_UNDECLARED_TYPE_NAME = "sema_undeclared_type_name";
static constexpr std::string_view DK_MACRO_UNDECLARED_IDENTIFIER = "macro_undeclared_identifier";
static constexpr std::string_view RK_PARSE_EXPECTED_DECL = "parse_expected_decl";
static constexpr std::string_view RK_LEX_EXPECTED_IDENTIFIER = "lex_expected_identifier";
static constexpr std::string_view RK_SEMA_UNDECLARED_IDENTIFIER = "sema_undeclared_identifier";
static constexpr std::string_view RK_MODULE_READ_FILE_TO_BUFFER_FAILED = "module_read_file_to_buffer_failed";
static constexpr std::string_view RK_SEMA_MISMATCHED_TYPES = "sema_mismatched_types";
static constexpr std::string_view RK_SEMA_MISMATCHED_TYPES_BECAUSE = "sema_mismatched_types_because";
static constexpr std::string_view RK_PACKAGE_UNSUPPORT_SAVE = "package_unsupport_save";
static constexpr std::string_view RK_MODULE_VERSION_NOT_IDENTICAL = "module_version_not_identical";
static constexpr std::string_view RK_MODULE_LOADED_AST_FAILED = "module_loaded_ast_failed";
static constexpr std::string_view RK_SEMA_UNUSED_IMPORT = "sema_unused_import";

static bool IsDiagMessage(const Diagnostic& diagnostic, std::string_view key)
{
    return diagnostic.diagMessage == key;
}

LSPDiagObserver::LSPDiagObserver(Callbacks *c, Cangjie::DiagnosticEngine &engine)
    : Cangjie::DiagnosticHandler(engine, Cangjie::DiagHandlerKind::LSP_HANDLER), callback(c)
{
    diag.SetIsDumpErrCnt(false);
    diag.SetIsEmitter(false);
}
LSPDiagObserver::~LSPDiagObserver() = default;
DiagLSPSeverity GetSeverity(DiagSeverity l)
{
    switch (l) {
        case DiagSeverity::DS_HINT:
            return DiagLSPSeverity::HINT;   // Hint
        case DiagSeverity::DS_NOTE:
            return DiagLSPSeverity::INFO;   // Info
        case DiagSeverity::DS_WARNING:
            return DiagLSPSeverity::WARNING;   // Warning
        case DiagSeverity::DS_ERROR:
            return DiagLSPSeverity::ERROR_DIAG;   // Error
        default:
            return DiagLSPSeverity::DEFAULT_DIAG;   // default
    }
}

void SetDiagMessage(Cangjie::Diagnostic &diagnostic, DiagnosticToken &diagToken)
{
    if (IsDiagMessage(diagnostic, RK_MODULE_READ_FILE_TO_BUFFER_FAILED)) {
        diagToken.message = "Empty file can not be compiled";
    } else if (IsDiagMessage(diagnostic, RK_SEMA_MISMATCHED_TYPES) ||
               IsDiagMessage(diagnostic, RK_SEMA_MISMATCHED_TYPES_BECAUSE)) {
        diagToken.message = diagnostic.GetErrorMessage() + " " + diagnostic.mainHint.str;
    } else {
        diagToken.message = diagnostic.GetErrorMessage();
    }
}

void LSPDiagObserver::HandleDiagnose(Cangjie::Diagnostic &diagnostic)
{
    // LSP client can't process negative
    if (!diagnostic.IsValid() || IsDiagMessage(diagnostic, RK_PACKAGE_UNSUPPORT_SAVE)) {
        return;
    }

    // Blocks crashes caused by binary mismatches in Release
#ifndef DEBUG
    if (IsDiagMessage(diagnostic, RK_MODULE_VERSION_NOT_IDENTICAL) ||
        IsDiagMessage(diagnostic, RK_MODULE_LOADED_AST_FAILED)) {
        if (!MessageHeaderEndOfLine::GetIsDeveco()) {
            // Ensure that the server can exit normally.
            CompilerCangjieProject::GetInstance()->isIdentical = false;
            // Prompt the user through the client.
            callback->ReportCjoVersionErr(diagnostic.errorMessage);
        }
    }
#endif

    if (!diagnostic.isRefactor) {
        if (HasPrevDiag(diagnostic.start, diagnostic.diagMessage)) {
            return;
        }
        SetPrevDiag(diagnostic.start, diagnostic.diagMessage);
    }

    if (diagnostic.diagSeverity == DiagSeverity::DS_ERROR) {
        diag.IncreaseErrorCount(diagnostic.diagCategory);
    }
    DiagnosticToken diagToken;
    diagToken.severity = static_cast<int>(GetSeverity(diagnostic.diagSeverity));
    auto start = diagnostic.GetBegin();
    auto end = diagnostic.GetEnd();
    diagToken.range = {{start.fileID, start.line - 1, start.column - 1}, {end.fileID, end.line - 1, end.column - 1}};
    diagToken.source = "Cangjie";
    diagToken.code = static_cast<int>(diagnostic.GetDiagKind());
    SetDiagMessage(diagnostic, diagToken);
    diagToken.category = GetCategory(diagnostic.diagCategory);
    if (IsDiagMessage(diagnostic, RK_SEMA_UNUSED_IMPORT)) {
        diagToken.tags = {1};
        diagToken.severity = static_cast<int>(DiagLSPSeverity::HINT);
    }

    // diagToken.category is enhanced features of clangd, GetCategory(diagnostic.diagCategory) can get it
    std::string filePath = diag.GetSourceManager().GetSource(diagnostic.GetBegin().fileID).path;
    CollectQuickFix(diagnostic, diagToken);
    // Add diagnostic details
    // If the position is incorrect, the error cause is located in the current position.
    std::vector<DiagnosticRelatedInformation> relatedInformation;
    std::set<char> endPunctuation = {',', '.', '?', '!', ':'};
    for (auto &subDiag : diagnostic.subDiags) {
        if (subDiag.mainHint.range.begin.column < 0) {
            FormatDiags(diagToken, subDiag, endPunctuation);
            continue;
        }

        DiagnosticRelatedInformation info;
        info.message = subDiag.subDiagMessage;
        info.location.range = {{subDiag.mainHint.range.begin.fileID, subDiag.mainHint.range.begin.line - 1,
                                   subDiag.mainHint.range.begin.column - 1},
            {subDiag.mainHint.range.end.fileID, subDiag.mainHint.range.end.line - 1,
                subDiag.mainHint.range.end.column - 1}};
        info.location.uri.file =
            URI::URIFromAbsolutePath(diag.GetSourceManager().GetSource(subDiag.mainHint.range.begin.fileID).path)
                .ToString();
        relatedInformation.emplace_back(info);
    }

    // add note Info
    AddNoteInfo(diagnostic, relatedInformation);

    diagToken.relatedInformation.emplace(relatedInformation);
    callback->UpdateDiagnostic(filePath, diagToken);
    DealMacroDiags(diagnostic, diagToken);
}

void LSPDiagObserver::AddNoteInfo(Cangjie::Diagnostic &diagnostic,
    std::vector<DiagnosticRelatedInformation> &relatedInformation)
{
    for (auto &note : diagnostic.notes) {
        DiagnosticRelatedInformation info;
        info.message = note.GetErrorMessage();
        auto noteStart = note.GetBegin();
        auto noteEnd = note.GetEnd();
        auto noteFilePath = diag.GetSourceManager().GetSource(noteStart.fileID).path;
        if (FileUtil::FileExist(noteFilePath)) {
            info.location.range = {{noteStart.fileID, noteStart.line - 1, noteStart.column - 1},
                {noteEnd.fileID, noteEnd.line - 1, noteEnd.column - 1}};
            info.location.uri.file =
                URI::URIFromAbsolutePath(diag.GetSourceManager().GetSource(noteStart.fileID).path).ToString();
            relatedInformation.emplace_back(info);
            continue;
        }
        std::size_t pos = noteFilePath.find_last_of(CONSTANTS::FILE_SEPARATOR);
        if (pos != std::string::npos) {
            auto notePackage = noteFilePath.substr(0, pos);
            auto fileName = noteFilePath.substr(pos + 1);
            auto pkgPath = CompilerCangjieProject::GetInstance()->GetPathFromPkg(notePackage);
            if (!pkgPath.empty()) {
                info.location.uri.file =
                    URI::URIFromAbsolutePath(pkgPath + CONSTANTS::FILE_SEPARATOR + fileName).ToString();
                info.location.range = {{noteStart.fileID, noteStart.line - 1, noteStart.column - 1},
                    {noteEnd.fileID, noteEnd.line - 1, noteEnd.column - 1}};
            }
        }
        relatedInformation.emplace_back(info);
    }
}

void LSPDiagObserver::FormatDiags(DiagnosticToken &diagToken, SubDiagnostic &subDiag, std::set<char> endPunctuation)
{
    if (!diagToken.message.empty() && RTrim(diagToken.message).back() == '.') {
        diagToken.message = RTrim(diagToken.message);
        if (!diagToken.message.empty()) {
            diagToken.message.pop_back();
        }
    }
    bool containEndChar = endPunctuation.find(Trim(diagToken.message).back()) != endPunctuation.end();
    diagToken.message = containEndChar ? (diagToken.message + " " + subDiag.subDiagMessage)
                                       : (diagToken.message + ", " + subDiag.subDiagMessage);
}

void LSPDiagObserver::DealMacroDiags(Cangjie::Diagnostic &diagnostic, const DiagnosticToken &token)
{
    std::string filePath = diag.GetSourceManager().GetSource(diagnostic.GetBegin().fileID).path;
    if (FileUtil::GetFileExtension(filePath) != "macrocall" || diagnostic.subDiags.empty()) {
        return;
    }
    // LCOV_EXCL_START
    DiagnosticToken diagToken = token;
    for (auto &subDiag : diagnostic.subDiags) {
        std::string subDiagPath =
            diag.GetSourceManager().GetSource(subDiag.mainHint.range.begin.fileID).path;
        if (FileUtil::GetFileExtension(subDiagPath) == "cj") {
            diagToken.range = {subDiag.mainHint.range.begin, subDiag.mainHint.range.end};
            diagToken.range = TransformFromChar2IDE(diagToken.range);
            callback->UpdateDiagnostic(subDiagPath, diagToken);
            return;
        }
    }
    // LCOV_EXCL_STOP
}

void LSPDiagObserver::CollectQuickFix(Cangjie::Diagnostic &diagnostic, DiagnosticToken &diagToken)
{
    // provide add import quick fix
    bool needAddImport = IsDiagMessage(diagnostic, DK_SEMA_UNDECLARED_IDENTIFIER) ||
        IsDiagMessage(diagnostic, DK_SEMA_UNDECLARED_TYPE_NAME) ||
        IsDiagMessage(diagnostic, DK_MACRO_UNDECLARED_IDENTIFIER) ||
        IsDiagMessage(diagnostic, RK_PARSE_EXPECTED_DECL) ||
        IsDiagMessage(diagnostic, RK_LEX_EXPECTED_IDENTIFIER) ||
        IsDiagMessage(diagnostic, RK_SEMA_UNDECLARED_IDENTIFIER);
    if (needAddImport) {
        diagToken.diaFix->addImport = true;
    }
    // provide remove unused import quick fix
    if (IsDiagMessage(diagnostic, RK_SEMA_UNUSED_IMPORT)) {
        diagToken.diaFix->removeImport = true;
    }
}
} // namespace ark
