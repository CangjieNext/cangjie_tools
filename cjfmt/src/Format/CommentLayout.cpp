// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Format/CommentLayout.h"
#include "cangjie/Lex/Lexer.h"

#include <algorithm>
#include <map>

namespace Cangjie::Format {
namespace {
struct Leaf {
    Doc* doc;
    size_t begin;
    size_t end;
};

std::string CollectSpelling(Doc& root, std::vector<Leaf>& leaves)
{
    std::string text;
    std::vector<Doc*> pending{&root};
    while (!pending.empty()) {
        auto* doc = pending.back();
        pending.pop_back();
        switch (doc->type) {
            case DocType::LINE_DOT:
                text += "\n";
                [[fallthrough]];
            case DocType::STRING:
            case DocType::DOT:
                leaves.push_back({doc, text.size(), text.size() + doc->value.size()});
                text += doc->value;
                break;
            case DocType::LINE:
            case DocType::HARDLINE:
            case DocType::SEPARATE:
                text += "\n";
                break;
            case DocType::SOFTLINE_WITH_SPACE:
                text += " ";
                break;
            default:
                for (auto it = doc->members.rbegin(); it != doc->members.rend(); ++it) {
                    pending.emplace_back(&*it);
                }
                break;
        }
    }
    return text;
}

bool IsCode(const Token& token)
{
    return token.kind != TokenKind::NL && token.kind != TokenKind::COMMENT;
}

struct Reservation {
    std::string text;
    bool boundary;
};
}

void AddCommentBudgets(Doc& root, const std::vector<Token>& tokens, SourceManager& sm, DiagnosticEngine& diag)
{
    if (std::none_of(tokens.begin(), tokens.end(), [](const Token& token) {
        return token.kind == TokenKind::COMMENT;
    })) {
        return;
    }
    std::vector<Leaf> leaves;
    auto spelling = CollectSpelling(root, leaves);
    auto fileID = sm.AddSource("___commentLayout.cj", spelling);
    auto generatedTokens = Lexer(fileID, spelling, diag, sm).GetTokens();
    std::vector<const Token*> original;
    std::vector<const Token*> generated;
    for (const auto& token : tokens) {
        if (IsCode(token)) {
            original.emplace_back(&token);
        }
    }
    for (const auto& token : generatedTokens) {
        if (IsCode(token)) {
            generated.emplace_back(&token);
        }
    }
    // Strict alignment avoids moving a comment across reordered modifiers,
    // raw macro tokens or a partially formatted region. Recovery stays intact.
    if (original.size() != generated.size()) {
        return;
    }
    for (size_t i = 0; i < original.size(); ++i) {
        if (original[i]->kind != generated[i]->kind || original[i]->Value() != generated[i]->Value()) {
            return;
        }
    }
    std::vector<size_t> lineStarts{0};
    for (size_t i = 0; i < spelling.size(); ++i) {
        if (spelling[i] == '\n') {
            lineStarts.emplace_back(i + 1);
        }
    }
    std::map<Doc*, std::map<size_t, std::vector<Reservation>>> budgets;
    size_t codeIndex = 0;
    size_t leafIndex = 0;
    for (const auto& token : tokens) {
        if (token.kind == TokenKind::NL) {
            continue;
        }
        if (token.kind != TokenKind::COMMENT) {
            ++codeIndex;
            continue;
        }
        if (codeIndex == 0 || token.Begin().line != original[codeIndex - 1]->End().line) {
            continue;
        }
        auto end = generated[codeIndex - 1]->End();
        auto offset = lineStarts.at(static_cast<size_t>(end.line - 1)) + static_cast<size_t>(end.column - 1);
        while (leafIndex < leaves.size() && leaves[leafIndex].end < offset) {
            ++leafIndex;
        }
        if (leafIndex == leaves.size() || offset <= leaves[leafIndex].begin) {
            continue;
        }
        auto text = token.Value();
        bool boundary = text.rfind("//", 0) == 0 || text.find('\n') != std::string::npos;
        if (boundary) {
            text = text.substr(0, text.find_first_of("\r\n"));
        }
        budgets[leaves[leafIndex].doc][offset - leaves[leafIndex].begin].push_back({" " + text, boundary});
    }
    for (const auto& [leaf, atOffsets] : budgets) {
        Doc annotated(DocType::CONCAT, leaf->indent, "");
        size_t previous = 0;
        for (const auto& [offset, reservations] : atOffsets) {
            Doc part = *leaf;
            part.value = leaf->value.substr(previous, offset - previous);
            annotated.members.emplace_back(std::move(part));
            for (const auto& reservation : reservations) {
                annotated.members.emplace_back(DocType::LINE_SUFFIX, leaf->indent, reservation.text);
                if (reservation.boundary) {
                    annotated.members.emplace_back(DocType::LINE_SUFFIX_BOUNDARY, leaf->indent + 1, "");
                }
            }
            previous = offset;
        }
        if (previous < leaf->value.size()) {
            annotated.members.emplace_back(DocType::STRING, leaf->indent, leaf->value.substr(previous));
        }
        *leaf = std::move(annotated);
    }
}
}
