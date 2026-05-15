// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"
#include "cangjie/AST/Node.h"
#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Cangjie::AST;

namespace ark {
std::string ExtractInterfaceGetVisibility(const Cangjie::AST::Decl &decl);
std::string ExtractInterfaceEscapeJsonString(const std::string &input);
bool ExtractInterfaceParseBoolOption(const std::map<std::string, std::string> &options, const std::string &key,
    bool defaultValue);
std::string ExtractInterfaceResolveImplementationClassName(const std::map<std::string, std::string> &options,
    const std::string &fallback);
std::string ExtractInterfaceJoinTypes(const std::vector<std::string> &types);
void ExtractInterfaceAddSelectedMemberSignature(std::unordered_set<std::string> &chosen,
    const std::string &signature);
bool ExtractInterfaceParseSelectedMembersFromJson(const std::string &raw, std::unordered_set<std::string> &chosen);
void ExtractInterfaceParseSelectedMembersFromText(const std::string &raw, std::unordered_set<std::string> &chosen);
std::unordered_set<std::string> ExtractInterfaceParseSelectedMembers(const std::map<std::string, std::string> &options);
std::optional<std::string> ExtractInterfaceParseInheritedMemberSignature(const std::string &signature);
std::string ExtractInterfaceNormalizeTypeNameForCompare(std::string name);
std::string ExtractInterfaceToSnakeCaseFileName(const std::string &name);
std::string ExtractInterfaceResolveTargetFilePath(const std::string &targetPath, const std::string &interfaceName);
bool ExtractInterfaceIsValidPackageName(const std::string &packageName);
} // namespace ark

using namespace ark;

TEST(ExtractInterfaceTest, VisibilityOptionsAndNames)
{
    Decl decl;
    EXPECT_EQ(ExtractInterfaceGetVisibility(decl), "public");
    decl.EnableAttr(Attribute::PUBLIC);
    EXPECT_EQ(ExtractInterfaceGetVisibility(decl), "public");
    decl.DisableAttr(Attribute::PUBLIC);
    decl.EnableAttr(Attribute::PROTECTED);
    EXPECT_EQ(ExtractInterfaceGetVisibility(decl), "protected");
    decl.DisableAttr(Attribute::PROTECTED);
    decl.EnableAttr(Attribute::INTERNAL);
    EXPECT_EQ(ExtractInterfaceGetVisibility(decl), "internal");
    decl.DisableAttr(Attribute::INTERNAL);
    decl.EnableAttr(Attribute::PRIVATE);
    EXPECT_EQ(ExtractInterfaceGetVisibility(decl), "private");

    EXPECT_EQ(ExtractInterfaceEscapeJsonString("a\"b\\c"), "a\\\"b\\\\c");

    std::map<std::string, std::string> options{{"enabled", "true"}, {"disabled", "false"},
        {"implementationClassName", "  PersonImpl  "}};
    EXPECT_TRUE(ExtractInterfaceParseBoolOption(options, "enabled", false));
    EXPECT_FALSE(ExtractInterfaceParseBoolOption(options, "disabled", true));
    EXPECT_TRUE(ExtractInterfaceParseBoolOption(options, "missing", true));
    EXPECT_EQ(ExtractInterfaceResolveImplementationClassName(options, "Fallback"), "PersonImpl");
    EXPECT_EQ(ExtractInterfaceResolveImplementationClassName({}, "  Fallback  "), "Fallback");

    EXPECT_EQ(ExtractInterfaceToSnakeCaseFileName("PersonImpl"), "person_impl");
    EXPECT_EQ(ExtractInterfaceToSnakeCaseFileName("URLParser2"), "url_parser2");
    EXPECT_EQ(ExtractInterfaceToSnakeCaseFileName("person impl"), "person_impl");
    EXPECT_EQ(ExtractInterfaceResolveTargetFilePath("", "Person"), "");
    EXPECT_EQ(ExtractInterfaceResolveTargetFilePath("D:/tmp/person.cj", "Person"), "D:/tmp/person.cj");
    EXPECT_TRUE(ExtractInterfaceResolveTargetFilePath("D:/tmp", "PersonImpl").find("person_impl.cj") !=
        std::string::npos);

    EXPECT_TRUE(ExtractInterfaceIsValidPackageName("ohos.demo"));
    EXPECT_FALSE(ExtractInterfaceIsValidPackageName(""));
    EXPECT_FALSE(ExtractInterfaceIsValidPackageName("default"));
    EXPECT_FALSE(ExtractInterfaceIsValidPackageName("a/b"));
    EXPECT_FALSE(ExtractInterfaceIsValidPackageName("a\\b"));
    EXPECT_FALSE(ExtractInterfaceIsValidPackageName("a:b"));
}

TEST(ExtractInterfaceTest, SelectedMembersAndTypes)
{
    EXPECT_EQ(ExtractInterfaceJoinTypes({}), "");
    EXPECT_EQ(ExtractInterfaceJoinTypes({"Base"}), "Base");
    EXPECT_EQ(ExtractInterfaceJoinTypes({"Base", "Serializable", "Debug"}), "Base & Serializable & Debug");

    std::unordered_set<std::string> chosen;
    ExtractInterfaceAddSelectedMemberSignature(chosen, "  func area(radius: Float64): Float64  ");
    EXPECT_TRUE(chosen.count("func area(radius: Float64): Float64") > 0);

    std::unordered_set<std::string> jsonChosen;
    EXPECT_FALSE(ExtractInterfaceParseSelectedMembersFromJson("", jsonChosen));
    EXPECT_FALSE(ExtractInterfaceParseSelectedMembersFromJson("{\"bad\":true}", jsonChosen));
    EXPECT_TRUE(ExtractInterfaceParseSelectedMembersFromJson("[\"func f(): Int64\", 1, \"func g(): Unit\"]",
        jsonChosen));
    EXPECT_TRUE(jsonChosen.count("func f(): Int64") > 0);
    EXPECT_TRUE(jsonChosen.count("func g(): Unit") > 0);

    std::unordered_set<std::string> textChosen;
    ExtractInterfaceParseSelectedMembersFromText("func a(): Int64, func b(): String", textChosen);
    EXPECT_TRUE(textChosen.count("func a(): Int64") > 0);
    EXPECT_TRUE(textChosen.count("func b(): String") > 0);

    auto parsedFromJson = ExtractInterfaceParseSelectedMembers({{"selectedMembers", "[\"func c(): Bool\"]"}});
    EXPECT_TRUE(parsedFromJson.count("func c(): Bool") > 0);
    auto parsedFromText = ExtractInterfaceParseSelectedMembers({{"selectedMembers", "func d(): Unit\nfunc e(): Rune"}});
    EXPECT_TRUE(parsedFromText.count("func d(): Unit") > 0);
    EXPECT_TRUE(parsedFromText.count("func e(): Rune") > 0);
    EXPECT_TRUE(ExtractInterfaceParseSelectedMembers({}).empty());

    EXPECT_FALSE(ExtractInterfaceParseInheritedMemberSignature("func f(): Unit").has_value());
    auto inherited = ExtractInterfaceParseInheritedMemberSignature("__inherited_type__: pkg.Base<Int64>");
    ASSERT_TRUE(inherited.has_value());
    EXPECT_EQ(*inherited, "pkg.Base<Int64>");
    EXPECT_EQ(ExtractInterfaceNormalizeTypeNameForCompare(" pkg.Base<Int64> "), "Base");
    EXPECT_EQ(ExtractInterfaceNormalizeTypeNameForCompare("Simple"), "Simple");
    EXPECT_EQ(ExtractInterfaceNormalizeTypeNameForCompare(""), "");
}
