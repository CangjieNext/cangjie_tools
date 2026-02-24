// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "IndexStorage.cpp"
#include <filesystem>
#include <gtest/gtest.h>

using namespace ark::lsp;
namespace fs = std::filesystem;

namespace {
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
} // namespace

class IndexStorageTest : public ::testing::Test {
protected:
    std::string testRoot = "./test_root_" + std::to_string(getpid());
    void SetUp() override { fs::create_directories(testRoot); }
    void TearDown() override { fs::remove_all(testRoot); }
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

TEST_F(IndexStorageTest, ReadRef003)
{
    flatbuffers::FlatBufferBuilder fbb;
    IdxFormat::Position posB(K_POS_X, K_POS_Y, K_POS_Z);
    auto loc_off = IdxFormat::CreateLocation(fbb, &posB, nullptr, K_ZERO);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND_VAL, K_CONTAINER_ID_VAL, true);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_POS_X);
    EXPECT_EQ(out.location.begin.line, K_POS_Y);
    EXPECT_EQ(out.location.begin.column, K_POS_Z);
    EXPECT_EQ(out.location.end.fileID, K_ZERO);
    EXPECT_EQ(out.location.end.line, K_ZERO);
    EXPECT_EQ(out.location.end.column, K_ZERO);
    EXPECT_TRUE(out.location.fileUri.empty());
}

TEST_F(IndexStorageTest, ReadRef004)
{
    flatbuffers::FlatBufferBuilder fbb;
    IdxFormat::Position posE(K_POS_E_X, K_POS_E_Y, K_POS_E_Z);
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, &posE, K_ZERO);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND_VAL, K_CONTAINER_ID_VAL, true);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO);
    EXPECT_EQ(out.location.begin.line, K_ZERO);
    EXPECT_EQ(out.location.begin.column, K_ZERO);
    EXPECT_EQ(out.location.end.fileID, K_POS_E_X);
    EXPECT_EQ(out.location.end.line, K_POS_E_Y);
    EXPECT_EQ(out.location.end.column, K_POS_E_Z);
    EXPECT_TRUE(out.location.fileUri.empty());
}

TEST_F(IndexStorageTest, ReadRef005)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto uri_off = fbb.CreateString("file://path");
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, nullptr, uri_off);
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
    EXPECT_EQ(out.location.fileUri, "file://path");
}

TEST_F(IndexStorageTest, SplitFileName_EdgeCases)
{
    auto res1 = SplitFileName("filename");
    EXPECT_EQ(res1.first, "");
    EXPECT_EQ(res1.second, "");

    auto res2 = SplitFileName("onlyone.dot");
    EXPECT_EQ(res2.first, "");
    EXPECT_EQ(res2.second, "");

    auto res3 = SplitFileName("");
    EXPECT_EQ(res3.first, "");
    EXPECT_EQ(res3.second, "");

    auto res4 = SplitFileName(".hidden.file.idx");
    EXPECT_EQ(res4.first, ".hidden");
    EXPECT_EQ(res4.second, "file");
}

TEST_F(IndexStorageTest, GetShardPath_PathConversion)
{
    CacheManager cm(testRoot);
    std::string path = cm.GetShardPathFromFilePath("ark/lsp/service", "hash123");
    EXPECT_TRUE(path.find("ark.lsp.service.hash123.idx") != std::string::npos);
}

TEST_F(IndexStorageTest, ReadRelation_Normal)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto rel_off = IdxFormat::CreateRelation(fbb, K_RELATION_SUBJECT, K_RELATION_PREDICATE, K_RELATION_OBJECT);
    fbb.Finish(rel_off);
    auto fb_rel = flatbuffers::GetRoot<IdxFormat::Relation>(fbb.GetBufferPointer());

    Relation out;
    ReadRelation(out, fb_rel);
    EXPECT_EQ(out.subject, K_RELATION_SUBJECT);
    EXPECT_EQ(out.predicate, RelationKind(K_RELATION_PREDICATE));
    EXPECT_EQ(out.object, K_RELATION_OBJECT);
}

TEST_F(IndexStorageTest, InitDir_CreatesNecessaryFolders)
{
    CacheManager cm(testRoot);
    cm.InitDir();
    EXPECT_TRUE(fs::exists(testRoot + "/.cache/index"));
    EXPECT_TRUE(fs::exists(testRoot + "/.cache/astdata"));
}

TEST_F(IndexStorageTest, IsStale_And_UpdateIdMap)
{
    CacheManager cm(testRoot);
    EXPECT_TRUE(cm.IsStale(K_TEST_PKG, K_HASH_A));

    cm.UpdateIdMap(K_TEST_PKG, K_HASH_A);
    EXPECT_FALSE(cm.IsStale(K_TEST_PKG, K_HASH_A));
    EXPECT_TRUE(cm.IsStale(K_TEST_PKG, K_HASH_B));
}

TEST_F(IndexStorageTest, LoadIndexShard_CorruptFile_ReturnsNull)
{
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "bad.file";
    std::string shard = "x";
    std::string path = cm.GetShardPathFromFilePath(pkg, shard);

    std::ofstream ofs(path);
    ofs << "This is not a flatbuffer binary data";
    ofs.close();

    auto res = cm.LoadIndexShard(pkg, shard);
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(IndexStorageTest, UtilityFunctions_Coverage)
{
    auto res1 = SplitFileName("no_dot");
    EXPECT_EQ(res1.first, "");
    EXPECT_EQ(res1.second, "");

    auto res2 = SplitFileName(".only_extension");
    EXPECT_EQ(res2.first, "");
    EXPECT_EQ(res2.second, "");

    auto res3 = SplitFileName("file.name.ext");
    EXPECT_EQ(res3.first, "file");
    EXPECT_EQ(res3.second, "name");

    auto res4 = SplitFileName("file.");
    EXPECT_EQ(res4.first, "");
    EXPECT_EQ(res4.second, "");

    std::string merged = MergeFileName("pkg/path", "hash123", "idx");
    EXPECT_FALSE(merged.empty());
}

TEST_F(IndexStorageTest, CacheManager_IsStale_Coverage)
{
    CacheManager cm(testRoot);
    std::string pkg = "test.package";
    std::string hash = "old_hash";

    cm.UpdateIdMap(pkg, hash);
    EXPECT_FALSE(cm.IsStale(pkg, hash));
    EXPECT_TRUE(cm.IsStale(pkg, "new_hash"));
    EXPECT_TRUE(cm.IsStale("unknown.pkg", "any"));
}

TEST_F(IndexStorageTest, LoadIndexShard_ErrorPaths)
{
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "error.pkg";
    std::string hash = "h1";
    std::string path = cm.GetShardPathFromFilePath(pkg, hash);

    auto res1 = cm.LoadIndexShard(pkg, hash);
    EXPECT_FALSE(res1.has_value());

    std::ofstream ofs(path, std::ios::binary);
    ofs << "This is definitely not a flatbuffer";
    ofs.close();

    auto res2 = cm.LoadIndexShard(pkg, hash);
    EXPECT_FALSE(res2.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(IndexStorageTest, LoadShard_NullSlabs)
{
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "null.slabs";
    std::string hash = "h2";

    flatbuffers::FlatBufferBuilder fbb;
    auto pkg_off = IdxFormat::CreateHashedPackage(fbb, K_ZERO, K_ZERO, K_ZERO, K_ZERO);
    IdxFormat::FinishHashedPackageBuffer(fbb, pkg_off);

    std::string path = cm.GetShardPathFromFilePath(pkg, hash);
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char *>(fbb.GetBufferPointer()), fbb.GetSize());
    ofs.close();

    auto res = cm.LoadIndexShard(pkg, hash);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE((*res)->refs.empty());
    EXPECT_TRUE((*res)->extends.empty());
}

TEST_F(IndexStorageTest, ReadSymbol_OptionalFieldMissing)
{
    flatbuffers::FlatBufferBuilder fbb;
    auto sym_off = IdxFormat::CreateSymbol(fbb, K_TEST_SYMBOL_ID, K_ZERO, K_ZERO, K_ZERO, K_ZERO, K_ZERO, K_ZERO,
        K_ZERO, false, K_ZERO, false, false, K_ZERO, K_ZERO, K_ZERO);
    fbb.Finish(sym_off);

    auto fb_sym = flatbuffers::GetRoot<IdxFormat::Symbol>(fbb.GetBufferPointer());
    Symbol res;
    ReadSymbol(res, fb_sym);

    EXPECT_EQ(res.id, K_TEST_SYMBOL_ID);
    EXPECT_TRUE(res.name.empty());
}

TEST_F(IndexStorageTest, CacheManager_IsStale_FileMissing)
{
    CacheManager cm(testRoot);
    std::string pkg = "missing.file.pkg";
    std::string hash = "hash123";

    cm.UpdateIdMap(pkg, hash);
    EXPECT_TRUE(cm.IsStale(pkg, "new_hash"));
}

TEST_F(IndexStorageTest, StoreSymbol_EmptyFields)
{
    flatbuffers::FlatBufferBuilder builder;
    Symbol sym;
    sym.id = K_SYMBOL_ID_1;

    auto offset = StoreSymbol(builder, sym);
    builder.Finish(offset);

    auto fb_sym = flatbuffers::GetRoot<IdxFormat::Symbol>(builder.GetBufferPointer());
    EXPECT_EQ(fb_sym->id(), K_SYMBOL_ID_1);
    EXPECT_TRUE(fb_sym->name()->str().empty());
}

TEST_F(IndexStorageTest, AstFileHandler_StoreShard_InvalidType)
{
    AstFileHandler handler;
    handler.StoreShard("any.ast", nullptr);

    AstFileOut out;
    out.data = nullptr;
    handler.StoreShard("any.ast", &out);
}