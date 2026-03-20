// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "ExtractInterface.h"
#include "../../../logger/Logger.h"
#include "../../../common/ItemResolverUtil.h"
#include "../TweakUtils.h"

namespace ark {
using namespace Cangjie;
using namespace AST;

static bool IsPublicMember(const Cangjie::AST::Decl &decl)
{
    // Minimal visibility rule: explicit PUBLIC is eligible
    // If more nuanced scoping is required, extend here.
    return decl.TestAttr(Cangjie::AST::Attribute::PUBLIC);
}

std::string ExtractInterface::SuggestInterfaceName(const std::string &typeName)
{
    if (typeName.empty()) { return "IExtracted"; }
    // Prefix with 'I' if not already
    if (typeName[0] == 'I') { return typeName; }
    return std::string("I") + typeName;
}

bool ExtractInterface::Prepare(const Selection &sel)
{
    auto top = sel.selectionTree.TopDecl();
    if (!top) {
        extraOptions.insert({"ErrorCode", std::to_string(static_cast<int>(ExtractInterfaceError::INVALID_TARGET))});
        return false;
    }

    bool ok = false;
    Meta::match(*top.get())(
        [&](Cangjie::AST::ClassDecl &decl) {
            ok = true;
            extraOptions["suggestName"] = SuggestInterfaceName(decl.identifier);
            return 0;
        },
        [&](Cangjie::AST::StructDecl &decl) {
            ok = true;
            extraOptions["suggestName"] = SuggestInterfaceName(decl.identifier);
            return 0;
        },
        [&](auto &) {
            ok = false;
            return 0;
        }
    );

    if (!ok) {
        extraOptions.insert({"ErrorCode", std::to_string(static_cast<int>(ExtractInterfaceError::INVALID_TARGET))});
    }
    return ok;
}

void ExtractInterface::CollectFromClass(Cangjie::AST::ClassDecl &decl, const Selection &sel, InterfaceInfo &info)
{
    if (!decl.body) { return; }
    SourceManager *sm = sel.arkAst ? sel.arkAst->sourceManager : nullptr;
    for (auto &member : decl.body->decls) {
        if (!member || member->astKind != ASTKind::FUNC_DECL) { continue; }
        auto *func = dynamic_cast<FuncDecl*>(member.get());
        if (!func || func->identifier.Val() == "init") { continue; }
        if (!IsPublicMember(*func)) { continue; }
        std::string sig = ItemResolverUtil::ResolveSignatureByNode(*func, sm);
        if (func->funcBody && func->funcBody->retType) {
            std::string ret = ItemResolverUtil::FetchTypeString(*func->funcBody->retType);
            if (!ret.empty()) { sig += ": " + ret; }
        }
        info.members.push_back(std::move(sig));
    }
}

void ExtractInterface::CollectFromStruct(Cangjie::AST::StructDecl &decl, const Selection &sel, InterfaceInfo &info)
{
    if (!decl.body) { return; }
    SourceManager *sm = sel.arkAst ? sel.arkAst->sourceManager : nullptr;
    for (auto &member : decl.body->decls) {
        if (!member || member->astKind != ASTKind::FUNC_DECL) { continue; }
        auto *func = dynamic_cast<FuncDecl*>(member.get());
        if (!func || func->identifier.Val() == "init") { continue; }
        if (!IsPublicMember(*func)) { continue; }
        std::string sig = ItemResolverUtil::ResolveSignatureByNode(*func, sm);
        if (func->funcBody && func->funcBody->retType) {
            std::string ret = ItemResolverUtil::FetchTypeString(*func->funcBody->retType);
            if (!ret.empty()) { sig += ": " + ret; }
        }
        info.members.push_back(std::move(sig));
    }
}

TextEdit ExtractInterface::InsertInterfaceDecl(const Selection &sel, const InterfaceInfo &info)
{
    TextEdit edit;
    if (!sel.arkAst || !sel.arkAst->file) { return edit; }

    Range insertRange = TweakUtils::FindGlobalInsertPos(*sel.arkAst->file, sel.range);
    insertRange = TransformFromChar2IDE(insertRange);

    std::string text;
    text += "\n\n";
    text += "interface ";
    text += info.name;
    text += " {\n";
    for (const auto &m : info.members) {
        text += "    func ";
        text += m;
        text += "\n";
    }
    text += "}\n";

    edit.range = insertRange;
    edit.newText = std::move(text);
    return edit;
}

std::optional<Tweak::Effect> ExtractInterface::Apply(const Selection &sel)
{
    Effect effect;
    if (!sel.arkAst || !sel.arkAst->sourceManager || !sel.arkAst->file) {
        return std::nullopt;
    }

    InterfaceInfo info;
    // Resolve interface name from extraOptions or suggest
    auto itName = sel.extraOptions.find("interfaceName");
    if (itName != sel.extraOptions.end()) {
        info.name = itName->second;
    } else if ((itName = sel.extraOptions.find("suggestName")) != sel.extraOptions.end()) {
        info.name = itName->second;
    }

    auto top = sel.selectionTree.TopDecl();
    if (!top) { return std::nullopt; }

    std::string typeName;
    Meta::match(*top.get())(
        [&](Cangjie::AST::ClassDecl &decl) {
            if (info.name.empty()) { info.name = SuggestInterfaceName(decl.identifier); }
            typeName = decl.identifier;
            CollectFromClass(decl, sel, info);
            return 0;
        },
        [&](Cangjie::AST::StructDecl &decl) {
            if (info.name.empty()) { info.name = SuggestInterfaceName(decl.identifier); }
            typeName = decl.identifier;
            CollectFromStruct(decl, sel, info);
            return 0;
        },
        [&](auto &) { return 0; }
    );

    // If nothing to extract, still create an empty interface to allow manual edits
    auto edit = InsertInterfaceDecl(sel, info);

    std::string uri = URI::URIFromAbsolutePath(sel.arkAst->file->filePath).ToString();
    effect.applyEdits.emplace(uri, std::vector<TextEdit>{std::move(edit)});
    return std::move(effect);
}

} // namespace ark