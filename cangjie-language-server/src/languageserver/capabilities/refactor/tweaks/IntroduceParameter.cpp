// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "IntroduceParameter.h"
#include "../../../common/Utils.h"
#include "../TweakRule.h"
#include "../TweakUtils.h"
#include <algorithm>
#include <cstddef>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <cangjie/AST/Walker.h>

namespace ark {
const std::unordered_set<Cangjie::AST::ASTKind> CANNOT_INTRODUCE_PARAMETER_EXPR = {
    ASTKind::BLOCK, ASTKind::STR_INTERPOLATION_EXPR, ASTKind::INTERPOLATION_EXPR
};

static bool CanBuildArgumentAtCallSites(const Tweak::Selection &sel)
{
    auto root = sel.selectionTree.root();
    if (!root || !root->node) {
        return false;
    }
    auto range = TweakUtils::GetCompleteExprRange(sel.selectionTree);
    if (range.start == Position{0, 0, 0} || range.start == range.end) {
        return false;
    }

    bool isValid = true;
    Walker(root->node.get(), [&range, &isValid](auto node) {
        if (!node || !isValid) {
            return VisitAction::STOP_NOW;
        }
        if (node->begin < range.start || node->end > range.end) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->astKind != ASTKind::REF_EXPR) {
            return VisitAction::WALK_CHILDREN;
        }
        auto refExpr = DynamicCast<Cangjie::AST::RefExpr *>(node.get());
        auto target = refExpr ? refExpr->GetTarget() : nullptr;
        if (target && target->astKind == ASTKind::VAR_DECL &&
            !target->TestAnyAttr(Attribute::GLOBAL, Attribute::IN_CLASSLIKE, Attribute::IN_STRUCT, Attribute::IN_ENUM)) {
            isValid = false;
            return VisitAction::STOP_NOW;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
    return isValid;
}

static bool HasLocalVariableReference(const Tweak::Selection &sel)
{
    auto root = sel.selectionTree.root();
    if (!root || !root->node) {
        return false;
    }
    auto range = TweakUtils::GetCompleteExprRange(sel.selectionTree);
    if (range.start == Position{0, 0, 0} || range.start == range.end) {
        return false;
    }

    bool hasLocalRef = false;
    Walker(root->node.get(), [&range, &hasLocalRef](auto node) {
        if (!node || hasLocalRef) {
            return VisitAction::STOP_NOW;
        }
        if (node->begin < range.start || node->end > range.end) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->astKind != ASTKind::REF_EXPR) {
            return VisitAction::WALK_CHILDREN;
        }
        auto refExpr = DynamicCast<Cangjie::AST::RefExpr *>(node.get());
        auto target = refExpr ? refExpr->GetTarget() : nullptr;
        if (target && target->astKind == ASTKind::VAR_DECL &&
            !target->TestAnyAttr(Attribute::GLOBAL, Attribute::IN_CLASSLIKE, Attribute::IN_STRUCT, Attribute::IN_ENUM)) {
            hasLocalRef = true;
            return VisitAction::STOP_NOW;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
    return hasLocalRef;
}

static std::vector<TextEdit> RemoveReplacedParameters(const Tweak::Selection &sel, Cangjie::AST::FuncDecl &funcDecl,
    Range &range, std::string &paramName, std::string &typeName, bool &insertedParameter,
    std::vector<std::size_t> &removedParamIndices);

class IntroduceParameterRule : public TweakRule {
    bool Check(const Tweak::Selection &sel, std::map<std::string, std::string> &extraOptions) const override
    {
        auto root = sel.selectionTree.root();
        if (!root || !root->node) {
            return false;
        }
        if (root->selected != SelectionTree::Selection::Complete || !TweakUtils::CheckValidExpr(*root)) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(IntroduceParameter::IntroduceParameterError::FAIL_GET_ROOT_EXPR))));
            return false;
        }
        if (CANNOT_INTRODUCE_PARAMETER_EXPR.count(root->node->astKind)) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(IntroduceParameter::IntroduceParameterError::INVALID_CODE_SEGMENT))));
            return false;
        }
        if (!root->node->IsExpr()) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(IntroduceParameter::IntroduceParameterError::INVALID_EXPR))));
            return false;
        }
        if (!IntroduceParameter::GetTargetFunc(sel)) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(IntroduceParameter::IntroduceParameterError::INVALID_SCOPE))));
            return false;
        }
        auto range = TweakUtils::GetCompleteExprRange(sel.selectionTree);
        if (TweakUtils::GetSelectedExprTypeName(sel.selectionTree, range).empty()) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(IntroduceParameter::IntroduceParameterError::INVALID_TYPE))));
            return false;
        }
        return true;
    }
};

bool IntroduceParameter::Prepare(const Selection &sel)
{
    TweakRuleEngine ruleEngine;
    ruleEngine.AddRule(std::make_unique<IntroduceParameterRule>());
    ruleEngine.CheckRules(sel, extraOptions);
    return true;
}

std::optional<Tweak::Effect> IntroduceParameter::Apply(const Selection &sel)
{
    Effect effect;
    if (!sel.arkAst || !sel.arkAst->file || !sel.arkAst->sourceManager) {
        return std::nullopt;
    }
    auto funcDecl = GetTargetFunc(sel);
    if (!funcDecl) {
        return std::nullopt;
    }

    std::string paramName = "newParameter";
    auto nameIt = sel.extraOptions.find("suggestName");
    if (nameIt != sel.extraOptions.end()) {
        paramName = nameIt->second;
    }

    Range range = TweakUtils::GetCompleteExprRange(sel.selectionTree);
    if (range.start == Position{0, 0, 0} || range.start == range.end) {
        return std::nullopt;
    }

    std::string typeName = TweakUtils::GetSelectedExprTypeName(sel.selectionTree, range);
    if (typeName.empty()) {
        return std::nullopt;
    }
    std::vector<TextEdit> textEdits;
    bool insertedParameter = false;
    std::vector<std::size_t> removedParamIndices;
    auto removeParamEdits = RemoveReplacedParameters(
        sel, *funcDecl, range, paramName, typeName, insertedParameter, removedParamIndices);
    textEdits.insert(textEdits.end(), removeParamEdits.begin(), removeParamEdits.end());
    if (!insertedParameter) {
        textEdits.push_back(InsertParameter(*funcDecl, paramName, typeName));
    }
    textEdits.push_back(ReplaceExprWithParam(sel, range, paramName));

    std::string uri = URI::URIFromAbsolutePath(sel.arkAst->file->filePath).ToString();
    effect.applyEdits.emplace(uri, std::move(textEdits));
    std::string argumentText = sel.arkAst->sourceManager->GetContentBetween(range.start, range.end);
    if (!HasLocalVariableReference(sel) && CanBuildArgumentAtCallSites(sel)) {
        UpdateCallSites(sel, *funcDecl, range, argumentText, paramName, removedParamIndices, effect.applyEdits);
    }
    return std::move(effect);
}

std::map<std::string, std::string> IntroduceParameter::ExtraOptions()
{
    return extraOptions;
}

Ptr<Cangjie::AST::FuncDecl> IntroduceParameter::GetTargetFunc(const Selection &sel)
{
    return TweakUtils::GetTargetFunc(
        sel.selectionTree, sel.arkAst, TweakUtils::GetCompleteExprRange(sel.selectionTree));
}

TextEdit IntroduceParameter::InsertParameter(
    Cangjie::AST::FuncDecl &funcDecl, std::string &paramName, std::string &typeName)
{
    TextEdit textEdit;
    if (!funcDecl.funcBody || funcDecl.funcBody->paramLists.empty() || !funcDecl.funcBody->paramLists.front()) {
        return textEdit;
    }

    auto paramList = funcDecl.funcBody->paramLists.front().get();
    Range insertRange = {paramList->rightParenPos, paramList->rightParenPos};
    textEdit.range = TransformFromChar2IDE(insertRange);
    std::ostringstream insertText;
    if (!paramList->params.empty()) {
        insertText << ", ";
    }
    insertText << paramName << ": " << typeName;
    textEdit.newText = insertText.str();
    return textEdit;
}

static bool IsFuncParamUsedOutsideRange(Cangjie::AST::FuncDecl &funcDecl, Cangjie::AST::FuncParam &param, Range &range)
{
    if (!funcDecl.funcBody || !funcDecl.funcBody->body) {
        return true;
    }

    bool isUsed = false;
    Walker(funcDecl.funcBody->body.get(), [&param, &range, &isUsed](auto node) {
        if (!node || isUsed) {
            return VisitAction::STOP_NOW;
        }
        if (!(node->begin < range.start) && !(range.end < node->end)) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->astKind != ASTKind::REF_EXPR) {
            return VisitAction::WALK_CHILDREN;
        }
        auto refExpr = DynamicCast<Cangjie::AST::RefExpr *>(node.get());
        if (refExpr && refExpr->GetTarget().get() == &param) {
            isUsed = true;
            return VisitAction::STOP_NOW;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();
    return isUsed;
}

static bool GetParamRemovalRange(Cangjie::AST::FuncParamList &paramList, Cangjie::AST::FuncParam &param, Range &range)
{
    for (std::size_t index = 0; index < paramList.params.size(); ++index) {
        if (!paramList.params[index] || paramList.params[index].get() != &param) {
            continue;
        }
        if (paramList.params.size() == 1) {
            range.start = param.begin;
            range.end = param.end;
            return true;
        }
        if (index + 1 < paramList.params.size() && paramList.params[index + 1]) {
            range.start = param.begin;
            range.end = paramList.params[index + 1]->begin;
            return true;
        }
        if (index > 0 && paramList.params[index - 1]) {
            range.start = paramList.params[index - 1]->end;
            range.end = param.end;
            return true;
        }
        return false;
    }
    return false;
}

static std::vector<std::size_t> CollectRemovableParamIndices(
    const Tweak::Selection &sel, Cangjie::AST::FuncDecl &funcDecl, Cangjie::AST::FuncParamList &paramList, Range &range)
{
    std::vector<std::size_t> indices;
    auto root = sel.selectionTree.root();
    if (!root || !root->node) {
        return indices;
    }

    Walker(root->node.get(), [&range, &funcDecl, &paramList, &indices](auto node) {
        if (!node) {
            return VisitAction::STOP_NOW;
        }
        if (node->begin < range.start || node->end > range.end) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->astKind != ASTKind::REF_EXPR) {
            return VisitAction::WALK_CHILDREN;
        }
        auto refExpr = DynamicCast<Cangjie::AST::RefExpr *>(node.get());
        auto target = refExpr ? DynamicCast<Cangjie::AST::FuncParam *>(refExpr->GetTarget().get()) : nullptr;
        if (!target || IsFuncParamUsedOutsideRange(funcDecl, *target, range)) {
            return VisitAction::WALK_CHILDREN;
        }
        for (std::size_t index = 0; index < paramList.params.size(); ++index) {
            if (!paramList.params[index] || paramList.params[index].get() != target ||
                std::find(indices.begin(), indices.end(), index) != indices.end()) {
                continue;
            }
            indices.push_back(index);
            break;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();

    std::sort(indices.begin(), indices.end());
    return indices;
}

static std::vector<TextEdit> RemoveReplacedParameters(
    const Tweak::Selection &sel, Cangjie::AST::FuncDecl &funcDecl, Range &range, std::string &paramName,
    std::string &typeName, bool &insertedParameter, std::vector<std::size_t> &removedParamIndices)
{
    std::vector<TextEdit> edits;
    if (!funcDecl.funcBody || funcDecl.funcBody->paramLists.empty() || !funcDecl.funcBody->paramLists.front()) {
        return edits;
    }

    auto paramList = funcDecl.funcBody->paramLists.front().get();
    removedParamIndices = CollectRemovableParamIndices(sel, funcDecl, *paramList, range);
    if (removedParamIndices.empty()) {
        return edits;
    }

    if (removedParamIndices.size() == paramList->params.size()) {
        TextEdit textEdit;
        Range replaceRange = {paramList->params.front()->begin, paramList->params.back()->end};
        textEdit.range = TransformFromChar2IDE(replaceRange);
        textEdit.newText = paramName + ": " + typeName;
        insertedParameter = true;
        edits.push_back(textEdit);
        return edits;
    }

    std::size_t replacedIndex = removedParamIndices.front();
    if (replacedIndex >= paramList->params.size() || !paramList->params[replacedIndex]) {
        return edits;
    }
    TextEdit replaceEdit;
    replaceEdit.range = TransformFromChar2IDE({paramList->params[replacedIndex]->begin, paramList->params[replacedIndex]->end});
    replaceEdit.newText = paramName + ": " + typeName;
    edits.push_back(replaceEdit);
    insertedParameter = true;

    for (auto it = removedParamIndices.rbegin(); it != removedParamIndices.rend(); ++it) {
        if (*it == replacedIndex || *it >= paramList->params.size() || !paramList->params[*it]) {
            continue;
        }
        Range removeRange;
        if (!GetParamRemovalRange(*paramList, *paramList->params[*it], removeRange)) {
            continue;
        }
        TextEdit textEdit;
        textEdit.range = TransformFromChar2IDE(removeRange);
        textEdit.newText = "";
        edits.push_back(textEdit);
    }
    return edits;
}

TextEdit IntroduceParameter::ReplaceExprWithParam(const Selection &sel, Range &range, std::string &paramName)
{
    TextEdit textEdit;
    textEdit.newText = paramName;
    std::string charContent = sel.arkAst->sourceManager->GetContentBetween(
        {range.start.fileID, range.start.line, 1}, range.start);
    int size = range.end.column - range.start.column;
    range.start.column = CountUnicodeCharacters(charContent) + 1;
    if (range.end.line == range.start.line) {
        range.end.column = range.start.column + size;
    }
    textEdit.range = TransformFromChar2IDE(range);
    return textEdit;
}

void IntroduceParameter::UpdateCallSites(const Selection &sel, Cangjie::AST::FuncDecl &funcDecl, Range &range,
    const std::string &argumentText, const std::string &paramName, const std::vector<std::size_t> &removedParamIndices,
    std::map<std::string, std::vector<TextEdit>> &applyEdits)
{
    if (!sel.arkAst || !sel.arkAst->file || !sel.arkAst->file->curPackage) {
        return;
    }

    for (auto &file : sel.arkAst->file->curPackage->files) {
        if (!file) {
            continue;
        }
        std::vector<TextEdit> callSiteEdits;
        Walker(file.get(), [&sel, &funcDecl, &range, &argumentText, &paramName, &removedParamIndices,
                            &callSiteEdits](auto node) {
            if (!node || node->astKind != ASTKind::CALL_EXPR) {
                return VisitAction::WALK_CHILDREN;
            }
            auto callExpr = DynamicCast<Cangjie::AST::CallExpr *>(node.get());
            if (!callExpr || callExpr->resolvedFunction.get() != &funcDecl) {
                return VisitAction::WALK_CHILDREN;
            }
            auto textEdit = InsertArgumentAtCallSite(
                sel, funcDecl, *callExpr, range, argumentText, paramName, removedParamIndices);
            if (textEdit) {
                callSiteEdits.push_back(*textEdit);
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();

        if (callSiteEdits.empty()) {
            continue;
        }
        std::string uri = URI::URIFromAbsolutePath(file->filePath).ToString();
        auto &edits = applyEdits[uri];
        edits.insert(edits.end(), callSiteEdits.begin(), callSiteEdits.end());
    }
}

std::optional<TextEdit> IntroduceParameter::InsertArgumentAtCallSite(const Selection &sel,
    Cangjie::AST::FuncDecl &funcDecl, Cangjie::AST::CallExpr &callExpr, Range &range,
    const std::string &argumentText, const std::string &paramName, const std::vector<std::size_t> &removedParamIndices)
{
    if (callExpr.rightParenPos.IsZero()) {
        return std::nullopt;
    }

    bool hasNamedArg = false;
    for (const auto &arg : callExpr.args) {
        if (arg && !arg->name.Empty()) {
            hasNamedArg = true;
            break;
        }
    }

    TextEdit textEdit;
    std::string newArgument = BuildCallSiteArgumentText(sel, funcDecl, callExpr, range, argumentText);
    if (!removedParamIndices.empty() && funcDecl.funcBody && !funcDecl.funcBody->paramLists.empty() &&
        funcDecl.funcBody->paramLists.front()) {
        auto paramList = funcDecl.funcBody->paramLists.front().get();
        std::vector<Range> argRanges;
        for (auto index : removedParamIndices) {
            if (index >= paramList->params.size() || !paramList->params[index]) {
                return std::nullopt;
            }
            auto arg = FindCallSiteArgByParam(callExpr, *paramList->params[index], index);
            if (!arg) {
                return std::nullopt;
            }
            argRanges.push_back({arg->begin, arg->end});
        }
        std::sort(argRanges.begin(), argRanges.end(),
            [](const auto &left, const auto &right) { return left.start < right.start; });
        Range replaceRange = {argRanges.front().start, argRanges.back().end};
        textEdit.range = TransformFromChar2IDE(replaceRange);
        std::ostringstream replaceText;
        if (hasNamedArg) {
            replaceText << paramName << ": ";
        }
        replaceText << newArgument;
        textEdit.newText = replaceText.str();
        return textEdit;
    }

    Range insertRange = {callExpr.rightParenPos, callExpr.rightParenPos};
    textEdit.range = TransformFromChar2IDE(insertRange);
    std::ostringstream insertText;
    if (!callExpr.args.empty()) {
        insertText << ", ";
    }
    if (hasNamedArg) {
        insertText << paramName << ": ";
    }
    insertText << newArgument;
    textEdit.newText = insertText.str();
    return textEdit;
}

std::string IntroduceParameter::BuildCallSiteArgumentText(const Selection &sel, Cangjie::AST::FuncDecl &funcDecl,
    Cangjie::AST::CallExpr &callExpr, Range &range, const std::string &argumentText)
{
    if (!sel.arkAst || !sel.arkAst->sourceManager || !funcDecl.funcBody || funcDecl.funcBody->paramLists.empty() ||
        !funcDecl.funcBody->paramLists.front()) {
        return argumentText;
    }

    auto paramList = funcDecl.funcBody->paramLists.front().get();
    std::vector<std::pair<Range, std::string>> replacements;
    auto root = sel.selectionTree.root();
    if (!root || !root->node) {
        return argumentText;
    }
    Walker(root->node.get(), [&range, &paramList, &callExpr, &replacements, &sel](auto node) {
        if (!node) {
            return VisitAction::STOP_NOW;
        }
        if (node->begin < range.start || node->end > range.end) {
            return VisitAction::SKIP_CHILDREN;
        }
        if (node->astKind != ASTKind::REF_EXPR) {
            return VisitAction::WALK_CHILDREN;
        }
        auto refExpr = DynamicCast<Cangjie::AST::RefExpr *>(node.get());
        auto target = refExpr ? DynamicCast<Cangjie::AST::FuncParam *>(refExpr->GetTarget().get()) : nullptr;
        if (!target) {
            return VisitAction::WALK_CHILDREN;
        }

        for (std::size_t index = 0; index < paramList->params.size(); ++index) {
            if (!paramList->params[index] || paramList->params[index].get() != target) {
                continue;
            }
            auto arg = IntroduceParameter::FindCallSiteArgByParam(callExpr, *paramList->params[index], index);
            if (!arg || !arg->expr) {
                return VisitAction::WALK_CHILDREN;
            }
            Range refRange = {refExpr->begin, refExpr->end};
            if (refRange.start < range.start || refRange.end > range.end) {
                return VisitAction::WALK_CHILDREN;
            }
            auto argExpr = arg->expr.get();
            std::string argText = sel.arkAst->sourceManager->GetContentBetween(argExpr->begin, argExpr->end);
            replacements.emplace_back(refRange, argText);
            return VisitAction::WALK_CHILDREN;
        }
        return VisitAction::WALK_CHILDREN;
    }).Walk();

    if (replacements.empty()) {
        return argumentText;
    }

    std::sort(replacements.begin(), replacements.end(),
        [](const auto &left, const auto &right) { return left.first.start < right.first.start; });

    std::ostringstream result;
    Position cursor = range.start;
    for (const auto &[replaceRange, replaceText] : replacements) {
        result << sel.arkAst->sourceManager->GetContentBetween(cursor, replaceRange.start);
        result << replaceText;
        cursor = replaceRange.end;
    }
    result << sel.arkAst->sourceManager->GetContentBetween(cursor, range.end);
    return result.str();
}

Ptr<Cangjie::AST::FuncArg> IntroduceParameter::FindCallSiteArgByParam(
    Cangjie::AST::CallExpr &callExpr, Cangjie::AST::FuncParam &param, std::size_t paramIndex)
{
    for (auto &arg : callExpr.args) {
        if (arg && !arg->name.Empty() && arg->name == param.identifier) {
            return arg.get();
        }
    }
    if (paramIndex < callExpr.args.size()) {
        return callExpr.args[paramIndex].get();
    }
    return nullptr;
}
} // namespace ark
