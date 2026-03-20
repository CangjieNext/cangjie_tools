// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_LSP_EXTRACTINTERFACE_H
#define CANGJIE_LSP_EXTRACTINTERFACE_H

#include "../Tweak.h"

namespace ark {

class ExtractInterface : public Tweak {
public:
    enum class ExtractInterfaceError {
        INVALID_TARGET = 2,
        EMPTY_METHODS
    };

    const std::string Id() const override
    {
        return "ExtractInterface";
    };

    std::string Title() const override
    {
        return "Extract to interface";
    }

    std::string Kind() const override
    {
        return CodeAction::REFACTOR_KIND;
    }

    bool Prepare(const Selection &sel) override;

    std::optional<Effect> Apply(const Selection &sel) override;

private:
    struct InterfaceInfo {
        std::string name;
        std::vector<std::string> members;
    };

    void CollectFromClass(Cangjie::AST::ClassDecl &decl, const Selection &sel, InterfaceInfo &info);

    void CollectFromStruct(Cangjie::AST::StructDecl &decl, const Selection &sel, InterfaceInfo &info);

    static std::string SuggestInterfaceName(const std::string &typeName);

    TextEdit InsertInterfaceDecl(const Selection &sel, const InterfaceInfo &info);
};
} // namespace ark

#endif // CANGJIE_LSP_EXTRACTINTERFACE_H