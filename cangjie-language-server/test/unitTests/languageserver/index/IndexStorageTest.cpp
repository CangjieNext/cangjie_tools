// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>
#include "IndexStorage.cpp"
 
using namespace ark::lsp;
namespace fs = std::filesystem;

namespace
{
static constexpr uint32_t K_ZERO = 0;
static constexpr uint64_t K_ZERO_ID = 0;

static constexpr uint16_t K_KIND_VAL = 2;
static constexpr uint64_t K_CONTAINER_ID_VAL = 42;
static constexpr uint64_t K_TEST_SYMBOL_ID = 101;
static constexpr uint64_t K_RELATION_SUBJECT = 1;
static constexpr uint32_t K_RELATION_PREDICATE = 2;
static constexpr uint64_t K_RELATION_OBJECT = 3;

static constexpr uint32_t K_POS_X = 7;
static constexpr uint32_t K_POS_Y = 8;
static constexpr uint32_t K_POS_Z = 9;
static constexpr uint32_t K_POS_E_X = 1;
static constexpr uint32_t K_POS_E_Y = 2;
static constexpr uint32_t K_POS_E_Z = 3;

static constexpr uint32_t K_SYMBOL_ID_1 = 1;
static constexpr uint32_t K_POS_LINE = 10;
static constexpr uint32_t K_POS_COL = 5;

const std::string K_TEST_PKG = "demo.pkg";
const std::string K_HASH_A = "aaaa";
const std::string K_HASH_B = "bbbb";
}

class IndexStorageTest: public ::testing::Test
{
protected:
    std::string testRoot = "./test_root_" + std::to_string(getpid());
    void SetUp() override
    {
        fs::create_directories(testRoot);
    }
    void TearDown() override
    {
        fs::remove_all(testRoot);
    }
};

TEST_F(IndexStorageTest, SplitFileName001)
{
    auto res = SplitFileName("filename");
    EXPECT_EQ(res.first, "");
    EXPECT_EQ(res.second, "");
}

TEST_F(IndexStorageTest, SplitFileName002)
{
    auto res = SplitFileName("file.txt");
    EXPECT_EQ(res.first, "");
    EXPECT_EQ(res.second, "");
}

TEST_F(IndexStorageTest, SplitFileName003)
{
    auto res = SplitFileName("archive.tar.gz");
    EXPECT_EQ(res.first, "archive");
    EXPECT_EQ(res.second, "tar");
}

TEST_F(IndexStorageTest, MergeFileName001)
{
    EXPECT_EQ(MergeFileName("mypkg", "123abc", "bin"), "mypkg.123abc.bin");
}

TEST_F(IndexStorageTest, ReadRef001)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto ref_off = IdxFormat::CreateRef(fbb, K_ZERO, K_KIND_VAL, K_CONTAINER_ID_VAL, true);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO);
    EXPECT_EQ(out.location.begin.line, K_ZERO);
    EXPECT_EQ(out.location.begin.column, K_ZERO);
    EXPECT_EQ(out.location.end.fileID, K_ZERO);
    EXPECT_EQ(out.location.end.line, K_ZERO);
    EXPECT_EQ(out.location.end.column, K_ZERO);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND_VAL));
    EXPECT_EQ(out.container, K_CONTAINER_ID_VAL);
    EXPECT_TRUE(out.isCjoRef);
}

TEST_F(IndexStorageTest, ReadRef002)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, nullptr, K_ZERO);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND_VAL, K_CONTAINER_ID_VAL, true);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO);
    EXPECT_EQ(out.location.begin.line, K_ZERO);
    EXPECT_EQ(out.location.begin.column, K_ZERO);
    EXPECT_EQ(out.location.end.fileID, K_ZERO);
    EXPECT_EQ(out.location.end.line, K_ZERO);
    EXPECT_EQ(out.location.end.column, K_ZERO);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND_VAL));
    EXPECT_EQ(out.container, K_CONTAINER_ID_VAL);
    EXPECT_TRUE(out.isCjoRef);
}