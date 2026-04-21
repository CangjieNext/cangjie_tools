// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "InlineFunction.h"
#include "../../../CompilerCangjieProject.h"
#include "../TweakRule.h"
#include "../TweakUtils.h"
#include "../../../common/PositionResolver.h"
#include <sstream>
#include <regex>

// LCOV_EXCL_START
namespace ark {

class InlineFunctionSelectionRule : public TweakRule {
    bool Check(const Tweak::Selection &sel, std::map<std::string, std::string> &extraOptions) const override
    {
        auto root = sel.selectionTree.root();

        if (!root || root->selected != SelectionTree::Selection::Complete) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(TweakRule::TweakError::ERROR_AST))));
            return false;
        }

        if (root->node->astKind != ASTKind::CALL_EXPR) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::NOT_CALL_EXPR))));
            return false;
        }

        auto callExpr = DynamicCast<CallExpr*>(root->node.get());
        if (!callExpr) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(TweakRule::TweakError::ERROR_AST))));
            return false;
        }

        if (!callExpr->resolvedFunction) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::FUNC_DECL_NOT_FOUND))));
            return false;
        }

        auto funcDecl = DynamicCast<FuncDecl*>(callExpr->resolvedFunction.get());
        if (!funcDecl || !funcDecl->funcBody) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::FUNC_DECL_NOT_FOUND))));
            return false;
        }

        return true;
    }
};

class InlineFunctionNoRecursiveRule : public TweakRule {
    bool Check(const Tweak::Selection &sel, std::map<std::string, std::string> &extraOptions) const override
    {
        auto root = sel.selectionTree.root();
        if (!root || !root->node || root->node->astKind != ASTKind::CALL_EXPR) {
            return false;
        }

        auto callExpr = DynamicCast<CallExpr*>(root->node.get());
        if (!callExpr || !callExpr->resolvedFunction) {
            return false;
        }

        auto funcDecl = DynamicCast<FuncDecl*>(callExpr->resolvedFunction.get());
        if (!funcDecl || !funcDecl->funcBody || !funcDecl->funcBody->body) {
            return false;
        }

        std::string funcName = funcDecl->identifier;
        bool isRecursive = false;

        Walker(funcDecl->funcBody->body.get(), nullptr, [&funcName, &isRecursive](Ptr<Node> node) -> VisitAction {
            if (node->astKind == ASTKind::CALL_EXPR) {
                auto innerCallExpr = DynamicCast<CallExpr*>(node.get());
                if (innerCallExpr && innerCallExpr->baseFunc) {
                    if (innerCallExpr->baseFunc->ToString() == funcName) {
                        isRecursive = true;
                        return VisitAction::STOP_NOW;
                    }
                }
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();

        if (isRecursive) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::IS_RECURSIVE_FUNC))));
            return false;
        }

        return true;
    }
};

class InlineFunctionNoPrivateAccessRule : public TweakRule {
    bool Check(const Tweak::Selection &sel, std::map<std::string, std::string> &extraOptions) const override
    {
        auto root = sel.selectionTree.root();
        if (!root || !root->node || root->node->astKind != ASTKind::CALL_EXPR) {
            return false;
        }

        auto callExpr = DynamicCast<CallExpr*>(root->node.get());
        if (!callExpr || !callExpr->resolvedFunction) {
            return false;
        }

        auto funcDecl = DynamicCast<FuncDecl*>(callExpr->resolvedFunction.get());
        if (!funcDecl || !funcDecl->funcBody || !funcDecl->funcBody->body) {
            return false;
        }

        bool hasPrivateAccess = false;

        Walker(funcDecl->funcBody->body.get(), nullptr, [&hasPrivateAccess](Ptr<Node> node) -> VisitAction {
            if (node->astKind == ASTKind::REF_EXPR) {
                auto refExpr = DynamicCast<RefExpr*>(node.get());
                if (refExpr) {
                    auto target = refExpr->GetTarget();
                    if (target && target->TestAttr(Attribute::PRIVATE)) {
                        hasPrivateAccess = true;
                        return VisitAction::STOP_NOW;
                    }
                }
            }
            return VisitAction::WALK_CHILDREN;
        }).Walk();

        if (hasPrivateAccess) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::HAS_PRIVATE_ACCESS))));
            return false;
        }

        return true;
    }
};

class InlineFunctionSimpleTypeRule : public TweakRule {
    bool Check(const Tweak::Selection &sel, std::map<std::string, std::string> &extraOptions) const override
    {
        auto root = sel.selectionTree.root();
        if (!root || !root->node || root->node->astKind != ASTKind::CALL_EXPR) {
            return false;
        }

        auto callExpr = DynamicCast<CallExpr*>(root->node.get());
        if (!callExpr || !callExpr->resolvedFunction) {
            return false;
        }

        auto funcDecl = DynamicCast<FuncDecl*>(callExpr->resolvedFunction.get());
        if (!funcDecl || !funcDecl->funcBody) {
            return false;
        }

        if (funcDecl->funcBody->generic ) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::IS_GENERIC_FUNC))));
            return false;
        }

        if (funcDecl->TestAnyAttr(Attribute::IN_CLASSLIKE, Attribute::IN_STRUCT, Attribute::IN_ENUM)) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::IS_MEMBER_FUNC))));
            return false;
        }

        if (funcDecl->TestAttr(Attribute::IN_EXTEND)) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::IS_EXTEND_FUNC))));
            return false;
        }

        if (callExpr->baseFunc && callExpr->baseFunc->astKind == ASTKind::LAMBDA_EXPR) {
            extraOptions.insert(std::make_pair("ErrorCode",
                std::to_string(static_cast<int>(InlineFunction::InlineFunctionError::IS_LAMBDA_CALL))));
            return false;
        }

        return true;
    }
};

bool InlineFunction::Prepare(const Selection &sel)
{
    TweakRuleEngine ruleEngine;

    ruleEngine.AddRule(std::make_unique<InlineFunctionSelectionRule>());
    ruleEngine.AddRule(std::make_unique<InlineFunctionNoRecursiveRule>());
    ruleEngine.AddRule(std::make_unique<InlineFunctionNoPrivateAccessRule>());
    ruleEngine.AddRule(std::make_unique<InlineFunctionSimpleTypeRule>());

    return ruleEngine.CheckRules(sel, extraOptions);
}

CallExpr* InlineFunction::GetCallExpr(const Selection &sel)
{
    auto root = sel.selectionTree.root();
    if (!root || !root->node || root->node->astKind != ASTKind::CALL_EXPR) {
        return nullptr;
    }
    return DynamicCast<CallExpr*>(root->node.get());
}

FuncDecl* InlineFunction::GetFuncDecl(CallExpr* callExpr)
{
    if (!callExpr || !callExpr->resolvedFunction) {
        return nullptr;
    }
    return DynamicCast<FuncDecl*>(callExpr->resolvedFunction.get());
}

bool InlineFunction::HasReturnValue(FuncDecl* funcDecl)
{
    if (!funcDecl || !funcDecl->funcBody) {
        return false;
    }

    if (funcDecl->funcBody->retType) {
        auto retTy = funcDecl->funcBody->retType.get();
        if (retTy && retTy->ty) {
            auto typeStr = retTy->ty->name;
            return typeStr != "void" && typeStr != "Unit";
        }
    }

    return false;
}

std::string InlineFunction::GetSourceCode(Ptr<Node> node, const Selection &sel)
{
    if (!node || !sel.arkAst || !sel.arkAst->sourceManager) {
        return "";
    }
    return sel.arkAst->sourceManager->GetContentBetween(node->begin, node->end);
}

std::string InlineFunction::GetReturnTypeString(FuncDecl* funcDecl)
{
    if (!funcDecl || !funcDecl->funcBody || !funcDecl->funcBody->retType) {
        return "";
    }

    auto retTy = funcDecl->funcBody->retType.get();
    if (!retTy || !retTy->ty) {
        return "";
    }

    return retTy->ty->name;
}

std::map<std::string, std::string> InlineFunction::BuildParamMap(CallExpr* callExpr, FuncDecl* funcDecl, const Selection &sel)
{
    std::map<std::string, std::string> paramMap;

    if (!callExpr || !funcDecl || !funcDecl->funcBody) {
        return paramMap;
    }

    size_t argIndex = 0;
    for (const auto &paramList : funcDecl->funcBody->paramLists) {
        for (const auto &param : paramList->params) {
            if (argIndex < callExpr->args.size()) {
                auto &arg = callExpr->args[argIndex];
                if (arg && arg->expr) {
                    std::string argCode = GetSourceCode(arg->expr, sel);
                    paramMap[param->identifier] = argCode;
                }
            }
            argIndex++;
        }
    }

    return paramMap;
}

std::string InlineFunction::ReplaceParamsInCode(const std::string &code, const std::map<std::string, std::string> &paramMap)
{
    std::string result = code;
    for (const auto &[paramName, paramValue] : paramMap) {
        std::regex paramRegex("\\b" + paramName + "\\b");
        result = std::regex_replace(result, paramRegex, paramValue);
    }
    return result;
}

std::string InlineFunction::GenerateInlineVarName()
{
    return "inline_result";
}

std::string InlineFunction::GenerateParamVarName(const std::string &paramName)
{
    return "inline_arg_" + paramName;
}

bool InlineFunction::IsSimpleArg(Expr* expr)
{
    if (!expr) {
        return true;
    }

    if (expr->astKind == ASTKind::REF_EXPR) {
        return true;
    }

    if (expr->astKind == ASTKind::LIT_CONST_EXPR) {
        return true;
    }

    return false;
}

bool InlineFunction::IsComplexArg(Expr* expr)
{
    return !IsSimpleArg(expr);
}

std::string InlineFunction::GetParamTypeString(FuncParam* param)
{
    if (!param || !param->ty) {
        return "";
    }
    return param->ty->name;
}

InlineFunction::ExtractedParams InlineFunction::ExtractParams(CallExpr* callExpr, FuncDecl* funcDecl, const Selection &sel)
{
    ExtractedParams result;

    if (!callExpr || !funcDecl || !funcDecl->funcBody) {
        return result;
    }

    size_t callArgCount = callExpr->args.size();
    size_t defaultArgCount = callExpr->defaultArgs.size();

    size_t argIndex = 0;
    for (const auto &paramList : funcDecl->funcBody->paramLists) {
        for (const auto &param : paramList->params) {
            std::string paramName = param->identifier;
            std::string paramType = GetParamTypeString(param.get());
            std::string paramValue;
            bool needsExtract = false;

            if (argIndex < callArgCount) {
                auto &arg = callExpr->args[argIndex];
                if (arg && arg->expr) {
                    paramValue = GetSourceCode(arg->expr, sel);
                    needsExtract = IsComplexArg(arg->expr.get());
                }
            } else if (argIndex < callArgCount + defaultArgCount) {
                size_t defaultArgIndex = argIndex - callArgCount;
                auto &defaultArg = callExpr->defaultArgs[defaultArgIndex];
                if (defaultArg && defaultArg->expr) {
                    paramValue = GetSourceCode(defaultArg->expr, sel);
                    needsExtract = true;
                }
            }

            result.params.emplace_back(paramName, paramType, paramValue, needsExtract);
            argIndex++;
        }
    }

    return result;
}

TextEdit InlineFunction::InsertParamDecls(const Selection &sel, CallExpr* callExpr,
    const ExtractedParams &extractedParams, std::string indent)
{
    TextEdit edit;

    std::ostringstream paramDecls;
    bool hasParamDecls = false;

    for (const auto &[paramName, paramType, paramValue, needsExtract] : extractedParams.params) {
        if (needsExtract && !paramValue.empty()) {
            std::string varName = GenerateParamVarName(paramName);
            paramDecls << "var " << varName;
            if (!paramType.empty()) {
                paramDecls << ": " << paramType;
            }
            paramDecls << " = " << paramValue << "\n";
            hasParamDecls = true;
        }
    }

    if (!hasParamDecls) {
        return edit;
    }

    std::string fullIndent;
    Range insertRange = FindInsertPosition(sel, callExpr, fullIndent);
    if (insertRange.end.IsZero()) {
        return edit;
    }

    std::ostringstream insertText;
    std::istringstream declStream(paramDecls.str());
    std::string line;
    while (std::getline(declStream, line)) {
        if (!line.empty()) {
            insertText << fullIndent << line << "\n";
        }
    }
    insertText << fullIndent;

    insertRange = TransformFromChar2IDE(insertRange);
    edit.range = insertRange;
    edit.newText = insertText.str();

    return edit;
}

Range InlineFunction::FindInsertPosition(const Selection &sel, CallExpr* callExpr, std::string &indent)
{
    Range insertRange;
    if (!callExpr || !sel.arkAst || sel.arkAst->tokens.empty()) {
        return insertRange;
    }

    bool isGlobal = false;
    Range callRange = {callExpr->begin, callExpr->end};

    if (sel.selectionTree.OuterInterpExpr()) {
        if (auto expr = DynamicCast<Expr*>(sel.selectionTree.OuterInterpExpr())) {
            if (sel.arkAst->packageInstance && sel.arkAst->packageInstance->ctx) {
                auto block = TweakUtils::FindOuterScopeNode(
                    *sel.arkAst->packageInstance->ctx, *expr, isGlobal, callRange);
                if (block) {
                    for (const auto &subNode : block->body) {
                        if (subNode && subNode->begin <= callRange.start && subNode->end >= callRange.end) {
                            insertRange = {subNode->begin, subNode->begin};
                            indent = GetIndent(sel, subNode->begin);
                            return insertRange;
                        }
                    }
                }
            }
        }
    }

    if (isGlobal && sel.arkAst->file) {
        insertRange = TweakUtils::FindGlobalInsertPos(*sel.arkAst->file, callRange);
        indent = "";
        return insertRange;
    }

    int firstTokenIdx = GetFirstTokenOnCurLine(sel.arkAst->tokens, callExpr->begin.line);
    if (firstTokenIdx >= 0 && firstTokenIdx < static_cast<int>(sel.arkAst->tokens.size())) {
        Token firstToken = sel.arkAst->tokens[firstTokenIdx];
        insertRange = {firstToken.Begin(), firstToken.Begin()};
        indent = GetIndent(sel, firstToken.Begin());
    }

    return insertRange;
}

TextEdit InlineFunction::InsertInlineBody(const Selection &sel, CallExpr* callExpr, FuncDecl* funcDecl,
    const ExtractedParams &extractedParams, const std::string &resultVarName, bool hasReturnValue)
{
    TextEdit edit;

    if (!callExpr || !funcDecl || !funcDecl->funcBody || !funcDecl->funcBody->body) {
        return edit;
    }

    std::string indent;
    Range insertRange = FindInsertPosition(sel, callExpr, indent);
    if (insertRange.end.IsZero()) {
        return edit;
    }

    std::map<std::string, std::string> paramMap;
    for (const auto &[paramName, paramType, paramValue, needsExtract] : extractedParams.params) {
        if (needsExtract) {
            paramMap[paramName] = GenerateParamVarName(paramName);
        } else {
            paramMap[paramName] = paramValue;
        }
    }

    std::string returnType = GetReturnTypeString(funcDecl);

    std::string inlinedBody = TransformFunctionBody(
        funcDecl->funcBody->body.get(), paramMap, resultVarName, hasReturnValue, sel);

    std::ostringstream insertText;

    if (hasReturnValue) {
        insertText << "var " << resultVarName;
        if (!returnType.empty()) {
            insertText << ": " << returnType;
        }
        insertText << "\n";
    }

    if (!inlinedBody.empty()) {
        std::istringstream bodyStream(inlinedBody);
        std::string line;
        while (std::getline(bodyStream, line)) {
            if (!line.empty()) {
                insertText << indent << line << "\n";
            }
        }
    }

    insertText << indent;

    insertRange = TransformFromChar2IDE(insertRange);
    edit.range = insertRange;
    edit.newText = insertText.str();

    return edit;
}

TextEdit InlineFunction::ReplaceCallWithVar(const Selection &sel, CallExpr* callExpr, const std::string &resultVarName)
{
    TextEdit edit;

    if (!callExpr) {
        return edit;
    }

    Range callRange = {callExpr->begin, callExpr->end};
    callRange = TransformFromChar2IDE(callRange);
    edit.range = callRange;
    edit.newText = resultVarName;

    return edit;
}

std::string InlineFunction::GetIndent(const Selection &sel, Position pos)
{
    if (!sel.arkAst || sel.arkAst->tokens.empty()) {
        return "";
    }

    int firstTokenIdx = GetFirstTokenOnCurLine(sel.arkAst->tokens, pos.line);
    if (firstTokenIdx < 0 || firstTokenIdx >= static_cast<int>(sel.arkAst->tokens.size())) {
        return "";
    }

    Token firstToken = sel.arkAst->tokens[firstTokenIdx];
    int column = firstToken.Begin().column;
    return std::string(column > 0 ? column - 1 : 0, ' ');
}

std::string InlineFunction::TransformFunctionBody(Block* block, const std::map<std::string, std::string> &paramMap,
    const std::string &resultVarName, bool hasReturnValue, const Selection &sel)
{
    if (!block) {
        return "";
    }

    std::ostringstream bodyCode;

    for (size_t i = 0; i < block->body.size(); ++i) {
        auto stmt = block->body[i].get();
        if (!stmt) {
            continue;
        }

        std::string stmtCode = GetSourceCode(stmt, sel);
        stmtCode = ReplaceParamsInCode(stmtCode, paramMap);

        if (stmt->astKind == ASTKind::RETURN_EXPR) {
            auto returnExpr = DynamicCast<ReturnExpr*>(stmt);
            if (returnExpr && returnExpr->expr) {
                std::string returnValue = GetSourceCode(returnExpr->expr, sel);
                returnValue = ReplaceParamsInCode(returnValue, paramMap);

                if (hasReturnValue && !resultVarName.empty()) {
                    bodyCode << resultVarName << " = " << returnValue << "\n";
                }
            }
        } else {
            bodyCode << stmtCode;
            if (stmtCode.back() != '\n') {
                bodyCode << "\n";
            }
        }
    }

    return bodyCode.str();
}

std::optional<Tweak::Effect> InlineFunction::Apply(const Selection &sel)
{
    Effect effect;

    if (!sel.arkAst || !sel.arkAst->sourceManager || !sel.arkAst->file) {
        return std::nullopt;
    }

    auto callExpr = GetCallExpr(sel);
    if (!callExpr) {
        return std::nullopt;
    }

    auto funcDecl = GetFuncDecl(callExpr);
    if (!funcDecl || !funcDecl->funcBody || !funcDecl->funcBody->body) {
        return std::nullopt;
    }

    bool hasReturnValue = HasReturnValue(funcDecl);
    std::string resultVarName = GenerateInlineVarName();

    ExtractedParams extractedParams = ExtractParams(callExpr, funcDecl, sel);

    std::vector<TextEdit> textEdits;

    std::string indent = GetIndent(sel, callExpr->begin);
    TextEdit paramDeclsEdit = InsertParamDecls(sel, callExpr, extractedParams, indent);
    if (!paramDeclsEdit.newText.empty()) {
        textEdits.push_back(paramDeclsEdit);
    }

    TextEdit insertBodyEdit = InsertInlineBody(sel, callExpr, funcDecl, extractedParams, resultVarName, hasReturnValue);
    if (!insertBodyEdit.newText.empty()) {
        textEdits.push_back(insertBodyEdit);
    }

    if (hasReturnValue) {
        TextEdit replaceCallEdit = ReplaceCallWithVar(sel, callExpr, resultVarName);
        textEdits.push_back(replaceCallEdit);
    }

    std::string filePath = sel.arkAst->file->filePath;
    std::string uri = URI::URIFromAbsolutePath(filePath).ToString();
    effect.applyEdits.emplace(uri, std::move(textEdits));

    return effect;
}

} // namespace ark