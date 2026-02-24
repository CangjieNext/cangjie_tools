// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "gtest/gtest.h"
#include <iostream>
#include <vector>
#include "Utils.h"

using namespace ark;

// A minimal Decl subclass to control astKind
class FakeDecl : public Cangjie::AST::Decl
{
public:
    explicit FakeDecl(ASTKind kind) : Decl(kind)
    {}

    FakeDecl(ASTKind kind,
        bool add,
        bool isCloned,
        bool primConstructor) : Decl(kind)
    {
        add_ = add;
        isCloned_ = isCloned;
        primCtor_ = primConstructor;
    }

    bool TestAttr(Cangjie::AST::Attribute attr) const
    {
        switch (attr)
        {
            case Cangjie::AST::Attribute::COMPILER_ADD:
                return add_;
            case Cangjie::AST::Attribute::IS_CLONED_SOURCE_CODE:
                return isCloned_;
            case Cangjie::AST::Attribute::PRIMARY_CONSTRUCTOR:
                return primCtor_;
            default:
                return false;
        }
    }

    void SetBegin(Position p)
    {
        begin = p;
    }

    Position GetIdentifierPos()
    {
        return identifierPos;
    }

    void setIdentifierPos(Position p)
    {
        identifierPos = p;
    }

    SrcIdentifier name;

private:
    bool add_;
    bool isCloned_;
    bool primCtor_;
    Position identifierPos;
};

class FakeExpr : public Expr
{
public:
    FakeExpr(ASTKind kind) : Expr(kind)
    {
    }
    std::vector<OwnedPtr<FuncArg>> args;
};

// Test 087: nullptr input should yield empty set
TEST(UtilsTest, UtilsTest087)
{
    auto decls = GetInheritDecls(nullptr);
    EXPECT_TRUE(decls.empty());
}

// Test 088: non-FUNC_DECL/PROP_DECL kind yields empty set
TEST(UtilsTest, UtilsTest088)
{
    FakeDecl varDecl{Cangjie::AST::ASTKind::VAR_DECL};
    auto decls = GetInheritDecls(&varDecl);
    EXPECT_TRUE(decls.empty());
}

// Test 089: FUNC_DECL path invokes TestFakeInheritDeclUtil and returns its result
TEST(UtilsTest, UtilsTest089)
{
    FakeDecl funcDecl{Cangjie::AST::ASTKind::FUNC_DECL};
    auto decls = GetInheritDecls(&funcDecl);
}

// Test 090: PROP_DECL path also invokes TestFakeInheritDeclUtil
TEST(UtilsTest, UtilsTest090)
{
    FakeDecl propDecl{Cangjie::AST::ASTKind::PROP_DECL};
    FakeDecl dummyDecl{Cangjie::AST::ASTKind::FUNC_DECL};
    auto decls = GetInheritDecls(&propDecl);
}

// Test 091: nullptr node → false
TEST(UtilsTest, UtilsTest091)
{
    bool rv = IsFromSrcOrNoSrc(nullptr);
    EXPECT_FALSE(rv);
}

// Test 092: node present, curFile nullptr → false
TEST(UtilsTest, UtilsTest092)
{
    Node node;
    node.curFile = nullptr;

    bool rv = IsFromSrcOrNoSrc(&node);
    EXPECT_FALSE(rv);
}

// Test 093: node and curFile present, curPackage nullptr → false
TEST(UtilsTest, UtilsTest093)
{
    File file;
    file.curPackage = nullptr;

    Node node;
    node.curFile = &file;

    bool rv = IsFromSrcOrNoSrc(&node);
    EXPECT_FALSE(rv);
}

// Test 095: all pointers valid, singleton returns false → false
TEST(UtilsTest, UtilsTest095)
{
    Ptr<const Node> filePrt(new File());
    // Configure fake to return false
    bool rv = IsFromSrcOrNoSrc(filePrt);
    EXPECT_FALSE(rv);
}

// Range GetRangeFromNode(Ptr<const Node> p, const std::vector<Cangjie::Token> &tokens)
TEST(UtilsTest, UtilsTest096)
{
    Ptr<const Node> p = nullptr;
    std::vector<Token> tokens;

    ark::Range r = GetRangeFromNode(p, tokens);

    EXPECT_EQ(r.end.line, 0);
    EXPECT_EQ(r.end.column, 0);
}

TEST(UtilsTest, UtilsTest097)
{
    Ptr<QualifiedType> p(new QualifiedType());
    std::vector<Token> tokens;

    ark::Range r = GetRangeFromNode(p, tokens);

    EXPECT_EQ(r.end.line, 0);
    EXPECT_EQ(r.end.column, 0);
}

TEST(UtilsTest, UtilsTest098)
{
    struct NodeWithTypeArgs : public Node
    {
        NodeWithTypeArgs() : Node() {}
        Position GetBegin() const { return {1, 1}; }
        Position GetEnd() const { return {2, 2}; }
    };
    Ptr<NodeWithTypeArgs> p(new NodeWithTypeArgs());
    std::vector<Token> tokens;
    ark::Range r = GetRangeFromNode(p, tokens);
    EXPECT_EQ(r.end.line, 0);
    EXPECT_EQ(r.end.column, 0);
}

TEST(UtilsTest, UtilsTest099)
{
    struct ZeroRangeNode : public Node
    {
        ZeroRangeNode() : Node() {}
        Position GetBegin() const { return {0, 0}; }
        Position GetEnd() const { return {0, 0}; }
    };

    Ptr<ZeroRangeNode> p(new ZeroRangeNode());
    std::vector<Token> tokens;
    ark::Range r = GetRangeFromNode(p, tokens);

    EXPECT_EQ(r.end.line, 0);
    EXPECT_EQ(r.end.column, 0);
}

TEST(UtilsTest, UtilsTest100)
{
    ASTKind invalidKind = static_cast<ASTKind>(-1);
    EXPECT_EQ(GetSymbolKind(invalidKind), SymbolKind::NULL_KIND);
}

TEST(UtilsTest, UtilsTest101)
{
    Ptr<const Decl> d = nullptr;
    EXPECT_FALSE(InValidDecl(d));
}

TEST(UtilsTest, UtilsTest102)
{
    Ptr<FakeDecl> d(new FakeDecl(ASTKind::INVALID_TYPE, true, true, false));
    EXPECT_FALSE(InValidDecl(d));
}

TEST(UtilsTest, UtilsTest103)
{
    Ptr<FakeDecl> d(new FakeDecl(ASTKind::INVALID_TYPE, true, false, true));
    EXPECT_FALSE(InValidDecl(d));
}

TEST(UtilsTest, UtilsTest104)
{
    Ptr<FakeDecl> d(new FakeDecl(ASTKind::EXTEND_DECL, true, false, false));
    EXPECT_FALSE(InValidDecl(d));
}

TEST(UtilsTest, UtilsTest105)
{
    std::string pkg = GetPkgNameFromNode(nullptr);
    EXPECT_EQ(pkg, "");
}

TEST(UtilsTest, UtilsTest106)
{
    Ptr<Node> node(new Node());
    node->curFile = nullptr;
    std::string pkg = GetPkgNameFromNode(node);
    EXPECT_EQ(pkg, "");
}

TEST(UtilsTest, UtilsTest107)
{
    SetHeadByFilePath("any/path.cpp");
}

TEST(UtilsTest, UtilsTest108)
{
    SetHeadByFilePath("source/file.cpp");
}

TEST(UtilsTest, UtilsTest109)
{
    SetHeadByFilePath("irrelevant.cpp");
}

TEST(UtilsTest, UtilsTest113)
{
    Decl decl;
    auto users = GetOnePkgUsers(decl, "", "/tmp/file.cpp", false, "unused");
    EXPECT_TRUE(users.empty());
}

TEST(UtilsTest, ConvertCarriageToSpace001)
{
    std::string str;
    ConvertCarriageToSpace(str);
    EXPECT_EQ(str, "");
}

TEST(UtilsTest, ConvertCarriageToSpace002)
{
    std::string str = "a\nb";
    ConvertCarriageToSpace(str);
    EXPECT_EQ(str, "a b");
}

TEST(UtilsTest, GetSingleConditionCompile001)
{
    nlohmann::json initOpts = {{CONSTANTS::SINGLE_CONDITION_COMPILE_OPTION, {{".pkg", {{"customKey", "customVal"}}}}}};
    std::unordered_map<std::string, std::string> globalConds = {{"g1", "gv1"}};
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> modulesConds = {
        {".pkg", {{"g1", "overwritten"}, {"m2", "modVal2"}}}};
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> outConds;
    GetSingleConditionCompile(initOpts, globalConds, modulesConds, outConds);
    ASSERT_EQ(outConds.size(), 1u);
    auto &pkgMap = outConds[".pkg"];
    EXPECT_EQ(pkgMap["g1"], "overwritten");
    EXPECT_EQ(pkgMap["m2"], "modVal2");
    EXPECT_EQ(pkgMap["customKey"], "customVal");
}

TEST(UtilsTest, Digest001)
{
    const std::string pkg = "nonexistent_file.cj";
    EXPECT_FALSE(FileUtil::FileExist(pkg));
    EXPECT_EQ(Digest(pkg), "");
}