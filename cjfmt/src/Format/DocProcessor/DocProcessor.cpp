// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Format/DocProcessor/DocProcessor.h"
#include "Format/ASTToFormatSource.h"
#include "cangjie/AST/Node.h"

namespace Cangjie::Format {
using namespace Cangjie::AST;
namespace {
constexpr int tabStopWidth = 8;
constexpr int logicalBreakPenalty = 10;
constexpr int binaryBreakPenalty = 20;
constexpr int listBreakPenalty = 30;
constexpr int assignmentBreakPenalty = 40;
constexpr int memberBreakPenalty = 50;
constexpr int openBreakPenalty = 60;
constexpr int genericBreakPenalty = 100;
}

void DocProcessor::RebaseToOutputLine(Doc& root, const std::string& formatted, int indentWidth)
{
    if (indentWidth == 0) {
        return;
    }
    auto newline = formatted.find_last_of('\n');
    size_t begin = newline == std::string::npos ? 0 : newline + 1;
    size_t end = begin;
    while (end < formatted.size() && (formatted[end] == ' ' || formatted[end] == '\t')) {
        ++end;
    }
    int base = AdvanceColumn(formatted.substr(begin, end - begin), 0) / indentWidth;
    int offset = base - root.indent;
    std::vector<Doc*> pending{&root};
    while (!pending.empty()) {
        auto* doc = pending.back();
        pending.pop_back();
        doc->indent += offset;
        for (auto& member : doc->members) {
            pending.emplace_back(&member);
        }
    }
}

bool DocProcessor::Fits(const std::pair<Doc, Mode>& next, int rem)
{
    return Fits(std::vector<std::pair<Doc, Mode>>{next}, rem);
}

int ConsumeFitText(const Doc& doc, int& rem, const FormattingOptions& options)
{
    const auto& value = doc.value;
    auto newline = value.find_first_of("\r\n");
    rem = options.lineLength - DocProcessor::AdvanceColumn(value.substr(0, newline), options.lineLength - rem);
    if (newline != std::string::npos) {
        return rem >= 0 ? 1 : -1;
    }
    return 0;
}

void ExpandFitDoc(const Doc& doc, Mode mode, std::vector<std::pair<const Doc*, Mode>>& fitCmd)
{
    for (auto it = doc.members.rbegin(); it != doc.members.rend(); ++it) {
        fitCmd.emplace_back(&*it, mode);
    }
}

int DocProcessor::ConsumeFitCommand(const std::pair<const Doc*, Mode>& current, int& rem,
    std::vector<std::pair<const Doc*, Mode>>& fitCmd)
{
    switch (current.first->type) {
        case DocType::LINE_SUFFIX:
        case DocType::STRING:
        case DocType::DOT:
            return ConsumeFitText(*current.first, rem, options);
        case DocType::FUNC_ARG:
        case DocType::LAMBDA:
        case DocType::ARGS:
        case DocType::CONCAT:
        case DocType::GROUP:
        case DocType::CHOICE:
        case DocType::FILL:
        case DocType::MEMBER_ACCESS:
        case DocType::INDENT:
        case DocType::ALIGN:
            ExpandFitDoc(*current.first, current.second, fitCmd);
            break;
        case DocType::LINE:
        case DocType::HARDLINE:
        case DocType::LINE_DOT:
        case DocType::SEPARATE:
        case DocType::LINE_SUFFIX_BOUNDARY:
        case DocType::LAMBDA_BODY:
            return 1;
        case DocType::SOFTLINE:
            if (current.second == Mode::MODE_BREAK) {
                return 1;
            }
            break;
        case DocType::SOFTLINE_WITH_SPACE:
            if (current.second == Mode::MODE_BREAK) {
                return 1;
            }
            rem -= 1;
            break;
        case DocType::BREAK_PARENT:
        default:
            break;
    }
    return 0;
}

bool DocProcessor::Fits(const std::vector<std::pair<Doc, Mode>>& commands, int rem)
{
    // The first pending Doc is a complete flat candidate. Include its following suffix up to the next
    // outer break opportunity, so closing delimiters and postfix operators cannot overflow the line.
    std::vector<std::pair<const Doc*, Mode>> fitCmd;
    size_t rest = commands.size();
    while (rem >= 0) {
        if (fitCmd.empty()) {
            if (rest == 0) {
                return true;
            }
            const auto& next = commands[--rest];
            if (rest + 1 != commands.size() && (next.first.type == DocType::SOFTLINE ||
                next.first.type == DocType::SOFTLINE_WITH_SPACE)) {
                return true;
            }
            fitCmd.emplace_back(&next.first, next.second);
        }
        auto current = fitCmd.back();
        fitCmd.pop_back();
        auto result = ConsumeFitCommand(current, rem, fitCmd);
        if (result != 0) {
            return result > 0;
        }
    }
    return false;
}

int DocProcessor::CalculateDocLen(Doc& doc)
{
    int len = 0;
    std::vector<const Doc*> pending{&doc};
    while (!pending.empty()) {
        const auto* current = pending.back();
        pending.pop_back();
        if (current->type == DocType::LINE || current->type == DocType::HARDLINE ||
            current->type == DocType::SEPARATE || current->type == DocType::LINE_DOT) {
            break;
        }
        len += current->type == DocType::SOFTLINE_WITH_SPACE ? 1 :
            static_cast<int>(DisplayWidth(current->value));
        for (auto it = current->members.rbegin(); it != current->members.rend(); ++it) {
            pending.emplace_back(&*it);
        }
    }
    return len;
}

int DocProcessor::MeasureFlat(const Doc& doc)
{
    int width = 0;
    std::vector<const Doc*> pending{&doc};
    while (!pending.empty()) {
        const auto* current = pending.back();
        pending.pop_back();
        switch (current->type) {
            case DocType::LINE:
            case DocType::HARDLINE:
            case DocType::LINE_DOT:
            case DocType::SEPARATE:
            case DocType::BREAK_PARENT:
            case DocType::LINE_SUFFIX_BOUNDARY:
                return -1;
            case DocType::SOFTLINE_WITH_SPACE:
                ++width;
                break;
            case DocType::LINE_SUFFIX:
            case DocType::STRING:
            case DocType::DOT:
                // Tabs depend on the starting column, and literal newlines
                // cannot be flattened. Their existing printer is the fallback.
                if (current->value.find_first_of("\t\r\n") != std::string::npos) {
                    return -1;
                }
                width += static_cast<int>(DisplayWidth(current->value));
                break;
            default:
                break;
        }
        for (auto it = current->members.rbegin(); it != current->members.rend(); ++it) {
            pending.emplace_back(&*it);
        }
    }
    return width;
}

int DocProcessor::MeasureInlineSuffix(const std::vector<std::pair<Doc, Mode>>& commands)
{
    int width = 0;
    std::vector<const Doc*> pending;
    for (const auto& command : commands) {
        pending.emplace_back(&command.first);
    }
    while (!pending.empty()) {
        const auto* doc = pending.back();
        pending.pop_back();
        switch (doc->type) {
            case DocType::SOFTLINE:
            case DocType::SOFTLINE_WITH_SPACE:
            case DocType::LINE:
            case DocType::HARDLINE:
            case DocType::LINE_DOT:
            case DocType::SEPARATE:
            case DocType::LINE_SUFFIX_BOUNDARY:
            case DocType::LAMBDA_BODY:
                return width;
            case DocType::LINE_SUFFIX:
            case DocType::STRING:
            case DocType::DOT:
                if (doc->value.find('\t') != std::string::npos) {
                    return -1;
                }
                if (doc->value.find_first_of("\r\n") != std::string::npos) {
                    return -1;
                }
                width += static_cast<int>(DisplayWidth(doc->value));
                break;
            default:
                for (auto it = doc->members.rbegin(); it != doc->members.rend(); ++it) {
                    pending.emplace_back(&*it);
                }
                break;
        }
    }
    return width;
}

int DocProcessor::DefaultBreakPenalty(BreakKind kind)
{
    switch (kind) {
        case BreakKind::LOGICAL:
            return logicalBreakPenalty;
        case BreakKind::BINARY:
            return binaryBreakPenalty;
        case BreakKind::LIST:
            return listBreakPenalty;
        case BreakKind::ASSIGNMENT:
            return assignmentBreakPenalty;
        case BreakKind::MEMBER:
            return memberBreakPenalty;
        case BreakKind::OPEN:
            return openBreakPenalty;
        case BreakKind::GENERIC:
        default:
            return genericBreakPenalty;
    }
}

int DocProcessor::AdvanceColumn(const std::string& text, int column)
{
    // Use conventional 8-column tab stops without changing literal contents.
    size_t begin = 0;
    while (begin < text.size()) {
        auto end = text.find_first_of("\t\r\n", begin);
        column += static_cast<int>(DisplayWidth(text.substr(begin, end - begin)));
        if (end == std::string::npos) {
            break;
        }
        column = text[end] == '\t' ? (column / tabStopWidth + 1) * tabStopWidth : 0;
        begin = end + 1;
    }
    return column;
}
} // namespace Cangjie::Format
