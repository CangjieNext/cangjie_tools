// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Format/DocProcessor/MemberAccessProcessor.h"
#include "Format/ASTToFormatSource.h"

#include <algorithm>

using namespace Cangjie::Format;

namespace {
bool ContainsForcedBreak(const Doc& doc)
{
    switch (doc.type) {
        case DocType::LINE:
        case DocType::HARDLINE:
        case DocType::LINE_DOT:
        case DocType::SEPARATE:
        case DocType::BREAK_PARENT:
        case DocType::LINE_SUFFIX_BOUNDARY:
        case DocType::LITERALLINE:
            return true;
        case DocType::STRING:
            // A raw multiline literal cannot be flattened. It is represented
            // as a STRING rather than a LINE doc, so detect its embedded line
            // ending here as well.
            return doc.value.find_first_of("\r\n") != std::string::npos;
        default:
            break;
    }
    return std::any_of(doc.members.begin(), doc.members.end(), [](const Doc& member) {
        return ContainsForcedBreak(member);
    });
}

void FlattenChain(const Doc& doc, std::vector<Doc>& parts)
{
    if (doc.type == DocType::MEMBER_ACCESS || doc.type == DocType::CONCAT ||
        (doc.type == DocType::GROUP && doc.layout == LayoutPolicy::LEGACY)) {
        for (const auto& member : doc.members) {
            FlattenChain(member, parts);
        }
    } else {
        parts.emplace_back(doc);
    }
}

void AppendCallSuffix(std::vector<Doc>& parts, std::vector<std::pair<Doc, Mode>>& commands)
{
    if (!commands.empty() && commands.back().first.value == "(") {
        int depth = 0;
        do {
            // Comment budgets wrap a delimiter in a transparent CONCAT. Expose
            // it before counting parentheses, otherwise an annotated ')' makes
            // this loop consume following statements and their block endings.
            auto type = commands.back().first.type;
            if (type == DocType::CONCAT ||
                (type == DocType::GROUP && commands.back().first.layout == LayoutPolicy::LEGACY)) {
                auto command = std::move(commands.back());
                commands.pop_back();
                for (auto it = command.first.members.rbegin(); it != command.first.members.rend(); ++it) {
                    commands.emplace_back(std::move(*it), command.second);
                }
                continue;
            }
            auto part = std::move(commands.back().first);
            commands.pop_back();
            depth += part.value == "(" ? 1 : part.value == ")" ? -1 : 0;
            parts.emplace_back(std::move(part));
        } while (!commands.empty() && depth > 0);
    }
    // Keep a trailing closure inside the method segment's ALIGN scope. That
    // scope anchors it before any call arguments wrap, so both '.onClick {'
    // and '.build(longArguments) {' inherit the method's actual base indent.
    if (commands.size() >= 2 && commands.back().first.type == DocType::STRING &&
        commands.back().first.value == " " && commands[commands.size() - 2].first.type == DocType::LAMBDA) {
        parts.emplace_back(std::move(commands.back().first));
        commands.pop_back();
        parts.emplace_back(std::move(commands.back().first));
        commands.pop_back();
    }
}
} // namespace

void MemberAccessProcessor::DocToString(std::string& formatted, int& pos, std::pair<Doc, Mode>& current,
    std::vector<std::pair<Doc, Mode>>& leftCmd)
{
    std::vector<Doc> parts;
    FlattenChain(current.first, parts);
    AppendCallSuffix(parts, leftCmd);
    size_t count = 0;
    for (const auto& part : parts) {
        count += part.type == DocType::DOT || part.type == DocType::LINE_DOT;
    }
    Doc chain(DocType::FILL, current.first.indent, "");
    chain.layout = LayoutPolicy::FILL;
    auto firstDot = std::find_if(parts.begin(), parts.end(), [](const Doc& part) {
        return part.type == DocType::DOT || part.type == DocType::LINE_DOT;
    });
    std::vector<Doc> receiverParts(parts.begin(), firstDot);
    std::vector<Doc> chainParts(firstDot, parts.end());
    Doc receiver(DocType::CONCAT, current.first.indent, std::move(receiverParts));
    Doc suffixChain(DocType::CONCAT, current.first.indent, std::move(chainParts));
    astToFormatSource.DocToString(receiver, pos, formatted);
    int width = MeasureFlat(suffixChain);
    int suffix = MeasureInlineSuffix(leftCmd);
    bool chainHasForcedBreak = ContainsForcedBreak(suffixChain);
    // Print the complete receiver first: calls and closures can wrap while
    // printing. Use its actual final column, independent of source newlines,
    // to decide whether the following chain needs one method per line.
    // An outer expression may already force a break after this chain. In that
    // case its suffix has no usable flat width, but the chain still needs to
    // be checked against the current line by itself.
    int suffixWidth = suffix < 0 ? 0 : suffix;
    bool expand = count > 1 && options.multipleLineMethodChainOverLineLength &&
        (chainHasForcedBreak || (width >= 0 && pos + width + suffixWidth > options.lineLength));
    Doc segment(DocType::ALIGN, current.first.indent, "");
    bool continuation = false;
    for (auto it = firstDot; it != parts.end(); ++it) {
        const auto& part = *it;
        if (part.type == DocType::DOT || part.type == DocType::LINE_DOT) {
            if (continuation) {
                chain.members.emplace_back(std::move(segment));
                segment = Doc(DocType::ALIGN, current.first.indent, "");
            }
            continuation = true;
            bool forced = expand || part.type == DocType::LINE_DOT;
            Doc separator(forced ? DocType::LINE : DocType::SOFTLINE, current.first.indent + 1, "");
            separator.breakKind = BreakKind::MEMBER;
            chain.members.emplace_back(std::move(separator));
            chain.members.emplace_back(DocType::STRING, current.first.indent + 1, ".");
        } else {
            if (continuation) {
                segment.members.emplace_back(part);
            } else {
                chain.members.emplace_back(part);
            }
        }
    }
    if (continuation) {
        chain.members.emplace_back(std::move(segment));
    }
    leftCmd.emplace_back(std::move(chain), Mode::MODE_FLAT);
}
