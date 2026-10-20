// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <algorithm>
#include <vector>
#include "cangjie/AST/Node.h"
#include "Format/ASTToFormatSource.h"
#include "Format/NodeFormatter/Node/FileFormatter.h"

namespace Cangjie::Format {
using namespace Cangjie::AST;

namespace {
std::vector<Cangjie::Position> GetWhenOrForeignPositions(const AST::Decl& decl)
{
    std::vector<Cangjie::Position> result;
    for (auto& ann : decl.annotations) {
        result.push_back(ann->begin);
    }
    for (auto& mod : decl.modifiers) {
        if (mod.modifier == TokenKind::FOREIGN) {
            result.push_back(mod.begin);
        }
    }
    return result;
}

// Flattening a singleton block must not merge a block annotation with a
// member annotation of the same name. Their source positions distinguish
// the two scopes even when the block has no second member to share them.
std::vector<Position> GetSingleForeignBlockPositions(const Decl& decl,
    const std::unordered_map<Position, int, PositionHasher>& positions)
{
    auto foreign = std::find_if(decl.modifiers.begin(), decl.modifiers.end(),
        [](const auto& modifier) { return modifier.modifier == TokenKind::FOREIGN; });
    if (foreign == decl.modifiers.end()) {
        return {};
    }
    auto count = positions.find(foreign->begin);
    if (count == positions.end() || count->second != 1) {
        return {};
    }
    std::vector<Position> prefix;
    bool needsBlock = false;
    for (const auto& annotation : decl.annotations) {
        if (!(annotation->begin < foreign->begin)) {
            continue;
        }
        prefix.emplace_back(annotation->begin);
        needsBlock = needsBlock || std::any_of(decl.annotations.begin(), decl.annotations.end(),
            [&annotation, &foreign](const auto& member) {
                return foreign->begin < member->begin && annotation->kind == member->kind &&
                    annotation->identifier == member->identifier;
            });
    }
    if (!needsBlock) {
        return {};
    }
    prefix.emplace_back(foreign->begin);
    return prefix;
}

void AddPackageSpec(Doc& doc, const Cangjie::AST::File& file, ASTToFormatSource& astToFormatSource, int level)
{
    if (file.package) {
        doc.members.emplace_back(astToFormatSource.ASTToDoc(file.package.get(), level));
    }
}

void AddImportSpecs(Doc& doc, const Cangjie::AST::File& file, ASTToFormatSource& astToFormatSource, int level)
{
    if (file.imports.empty()) {
        return;
    }

    if (file.package || file.feature) {
        doc.members.emplace_back(DocType::SEPARATE, level, "");
        doc.members.emplace_back(DocType::SEPARATE, level, "");
    }

    for (unsigned int i = 0; i < file.imports.size(); i++) {
        auto importSpec = file.imports[i].get();
        if (i != 0 && !importSpec->TestAttr(Attribute::COMPILER_ADD)) {
            doc.members.emplace_back(DocType::LINE, level, "");
        }
        doc.members.emplace_back(astToFormatSource.ASTToDoc(importSpec, level));
    }
}

bool HasHeader(const Cangjie::AST::File& file)
{
    return file.feature || file.package || !file.imports.empty();
}

void AddFileHeader(Doc& doc, const Cangjie::AST::File& file, ASTToFormatSource& astToFormatSource, int level)
{
    if (file.feature) {
        doc.members.emplace_back(astToFormatSource.ASTToDoc(file.feature.get(), level));
    }

    if (file.feature && file.package) {
        doc.members.emplace_back(DocType::LINE, level, "");
    }

    AddPackageSpec(doc, file, astToFormatSource, level);
    AddImportSpecs(doc, file, astToFormatSource, level);

    if (HasHeader(file)) {
        doc.members.emplace_back(DocType::SEPARATE, level, "");
        doc.members.emplace_back(DocType::SEPARATE, level, "");
    }
}

void AddBlockPrefix(const OwnedPtr<Decl>& decl, std::vector<Position>& beforeBlockAnnotationsPositions, Doc& doc,
    ASTToFormatSource& astToFormatSource, int level)
{
    std::vector<Ptr<Annotation>> annotationsToRender;
    for (auto& ann : decl->annotations) {
        for (auto beginPos : beforeBlockAnnotationsPositions) {
            if (ann->begin == beginPos) {
                annotationsToRender.push_back(ann.get());
            }
        }
    }

    std::set<Modifier> modifiersToRender;
    for (auto& mod : decl->modifiers) {
        for (auto beginPos : beforeBlockAnnotationsPositions) {
            if (mod.begin == beginPos) {
                (void)modifiersToRender.insert(mod);
            }
        }
    }

    for (auto it = annotationsToRender.begin(); it != annotationsToRender.end(); ++it) {
        if (it != annotationsToRender.begin()) {
            doc.members.emplace_back(DocType::LINE, level, "");
        }
        doc.members.emplace_back(astToFormatSource.ASTToDoc(*it));
    }
    if (!annotationsToRender.empty() && !modifiersToRender.empty()) {
        doc.members.emplace_back(DocType::LINE, level, "");
    }

    astToFormatSource.AddModifier(doc, modifiersToRender, level);

    if (modifiersToRender.empty()) {
        doc.members.emplace_back(DocType::STRING, level, " ");
    }

    doc.members.emplace_back(DocType::STRING, level, "{");
    doc.members.emplace_back(DocType::LINE, level + 1, "");
}

void AddBlockSuffix(Doc& doc, int level)
{
    doc.members.emplace_back(DocType::LINE, level, "");
    doc.members.emplace_back(DocType::STRING, level, "}");
}

void ShouldLastBlockADDSpearate(
    Doc& doc, int level, const File& file, std::vector<OwnedPtr<Cangjie::AST::Decl>>::const_iterator it)
{
    if (IsVariableDecl(it->get()) && it != file.decls.end() - 1 && IsVariableDecl(std::next(it)->get())) {
        doc.members.emplace_back(DocType::SEPARATE, level, "");
    }
}

void EraseModifiersAndAnnotationsForDecl(
    const OwnedPtr<Decl>& decl, std::vector<Position>& beforeBlockAnnotationsPositions)
{
    for (auto& beginPos : beforeBlockAnnotationsPositions) {
        auto& annotations = decl->annotations;
        auto annotationToErase = std::find_if(
            annotations.begin(), annotations.end(), [beginPos](auto& ann) { return ann->begin == beginPos; });
        if (annotationToErase != annotations.end()) {
            (void)annotations.erase(annotationToErase);
        }

        auto& modifiers = decl->modifiers;
        auto modifierToErase =
            std::find_if(modifiers.begin(), modifiers.end(), [beginPos](auto& m) { return m.begin == beginPos; });
        if (modifierToErase != modifiers.end()) {
            (void)modifiers.erase(modifierToErase);
        }
    }
}
} // namespace

void FileFormatter::AddFile(Doc& doc, const Cangjie::AST::File& file, int level)
{
    doc.type = DocType::CONCAT;
    doc.indent = level;
    AddFileHeader(doc, file, astToFormatSource, level);

    std::unordered_map<Cangjie::Position, int, PositionHasher> modifierOrAnnoToPosMap;
    SetModifierOrAnnoToPosMap(file, modifierOrAnnoToPosMap);

    Ptr<Decl> previousDecl = nullptr;
    bool preDeclIsLastInBlock = false;
    auto mapCopy(modifierOrAnnoToPosMap);
    for (auto it = file.decls.begin(); it != file.decls.end(); ++it) {
        if (it->get()->TestAttr(Attribute::COMPILER_ADD)) {
            continue;
        }

        bool isFirstInBlock = false;
        bool isLastInBlock = false;

        auto singleBlockPositions = GetSingleForeignBlockPositions(*it->get(), modifierOrAnnoToPosMap);
        std::vector<Cangjie::Position> beforeBlockAnnotationsPositions;
        for (auto pos : GetWhenOrForeignPositions(*it->get())) {
            if (std::find(singleBlockPositions.begin(), singleBlockPositions.end(), pos) != singleBlockPositions.end()) {
                beforeBlockAnnotationsPositions.emplace_back(pos);
                isFirstInBlock = true;
                isLastInBlock = true;
                continue;
            }
            if (modifierOrAnnoToPosMap.find(pos) != modifierOrAnnoToPosMap.end() && modifierOrAnnoToPosMap[pos] > 1) {
                beforeBlockAnnotationsPositions.emplace_back(pos);
                if (mapCopy[pos] == modifierOrAnnoToPosMap[pos]) {
                    isFirstInBlock = true;
                }
                mapCopy[pos]--;
                if (mapCopy[pos] == 0) {
                    isLastInBlock = true;
                }
            }
        }

        auto declarationLevel = beforeBlockAnnotationsPositions.empty() ? level : level + 1;
        if (!isFirstInBlock) {
            // Shared annotations and modifiers belong to the block, not this member's source range.
            EraseModifiersAndAnnotationsForDecl(*it, beforeBlockAnnotationsPositions);
        }
        if (previousDecl) {
            // Block boundaries keep their existing separators; only preserve gaps within the same scope.
            bool shouldSeparate = !IsVariableDecl(previousDecl) || !IsVariableDecl(it->get()) ||
                (!isFirstInBlock && !preDeclIsLastInBlock &&
                    astToFormatSource.ShouldAddEmptyLineBetween(previousDecl, it->get()));
            if (shouldSeparate) {
                doc.members.emplace_back(DocType::SEPARATE, level, "");
            }
            doc.members.emplace_back(DocType::LINE, isFirstInBlock ? level : declarationLevel, "");
        }

        if (isFirstInBlock) {
            AddBlockPrefix(*it, beforeBlockAnnotationsPositions, doc, astToFormatSource, level);
            EraseModifiersAndAnnotationsForDecl(*it, beforeBlockAnnotationsPositions);
        }
        previousDecl = it->get();
        preDeclIsLastInBlock = isLastInBlock;
        doc.members.emplace_back(astToFormatSource.ASTToDoc(*it, declarationLevel));
        if (isLastInBlock) {
            AddBlockSuffix(doc, level);
            ShouldLastBlockADDSpearate(doc, level, file, it);
        }
    }
}

void FileFormatter::ASTToDoc(Doc& doc, Ptr<Cangjie::AST::Node> node, int level, FuncOptions&)
{
    auto file = As<ASTKind::FILE>(node);
    AddFile(doc, *file, level);
}

void FileFormatter::SetModifierOrAnnoToPosMap(
    const Cangjie::AST::File& file, std::unordered_map<Cangjie::Position, int, PositionHasher>& modifierOrAnnoToPosMap)
{
    for (auto& decl : file.decls) {
        for (auto pos : GetWhenOrForeignPositions(*decl)) {
            ++modifierOrAnnoToPosMap[pos];
        }
    }
}
} // namespace Cangjie::Format
