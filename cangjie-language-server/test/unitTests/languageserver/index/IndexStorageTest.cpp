// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>
#include "IndexStorage.cpp"
#include <filesystem>

using namespace ark::lsp;
namespace fs = std::filesystem;

// --- Constants for testing ---
static constexpr uint16_t K_KIND = 2;
static constexpr uint64_t K_CONTAINER_ID = 42;
static constexpr bool K_IS_CJO_REF = true;

// Zero constants for positions and IDs
static constexpr uint32_t K_ZERO_U = 0u;
static constexpr uint64_t K_ZERO_ULL = 0;

// Coordinate constants
static constexpr uint32_t K_POS_B_FILE_ID = 7u;
static constexpr uint32_t K_POS_B_LINE = 8u;
static constexpr uint32_t K_POS_B_COL = 9u;

static constexpr uint32_t K_POS_E_FILE_ID = 1u;
static constexpr uint32_t K_POS_E_LINE = 2u;
static constexpr uint32_t K_POS_E_COL = 3u;

// Identity and Relation constants
static constexpr uint64_t K_SYMBOL_ID_DEFAULT = 101u;
static constexpr uint64_t K_SUBJECT_ID = 1u;
static constexpr uint16_t K_PREDICATE_ID = 2;
static constexpr uint64_t K_OBJECT_ID = 3u;

class IndexStorageTest: public ::testing::Test {
protected:
    std::string testRoot = "./test_root_" + std::to_string(getpid());
    void SetUp() override {
        fs::create_directories(testRoot);
    }
    void TearDown() override {
        fs::remove_all(testRoot);
    }
};

TEST_F(IndexStorageTest, SplitFileName001) {
    auto res = SplitFileName("filename");
    EXPECT_EQ(res.first, "");
    EXPECT_EQ(res.second, "");
}

TEST_F(IndexStorageTest, SplitFileName002) {
    auto res = SplitFileName("file.txt");
    EXPECT_EQ(res.first, "");
    EXPECT_EQ(res.second, "");
}

TEST_F(IndexStorageTest, SplitFileName003) {
    auto res = SplitFileName("archive.tar.gz");
    EXPECT_EQ(res.first, "archive");
    EXPECT_EQ(res.second, "tar");
}

TEST_F(IndexStorageTest, MergeFileName001) {
    EXPECT_EQ(MergeFileName("mypkg", "123abc", "bin"), "mypkg.123abc.bin");
}

TEST_F(IndexStorageTest, ReadRef001) {
    flatbuffers::FlatBufferBuilder fbb;
    auto ref_off = IdxFormat::CreateRef(fbb, K_ZERO_U, K_KIND, K_CONTAINER_ID, K_IS_CJO_REF);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.begin.line, K_ZERO_U);
    EXPECT_EQ(out.location.begin.column, K_ZERO_U);
    EXPECT_EQ(out.location.end.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.end.line, K_ZERO_U);
    EXPECT_EQ(out.location.end.column, K_ZERO_U);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND));
    EXPECT_EQ(out.container, K_CONTAINER_ID);
    EXPECT_EQ(out.isCjoRef, K_IS_CJO_REF);
}

TEST_F(IndexStorageTest, ReadRef002) {
    flatbuffers::FlatBufferBuilder fbb;
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, nullptr, K_ZERO_ULL);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND, K_CONTAINER_ID, K_IS_CJO_REF);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.begin.line, K_ZERO_U);
    EXPECT_EQ(out.location.begin.column, K_ZERO_U);
    EXPECT_EQ(out.location.end.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.end.line, K_ZERO_U);
    EXPECT_EQ(out.location.end.column, K_ZERO_U);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND));
    EXPECT_EQ(out.container, K_CONTAINER_ID);
    EXPECT_EQ(out.isCjoRef, K_IS_CJO_REF);
}

TEST_F(IndexStorageTest, ReadRef003) {
    flatbuffers::FlatBufferBuilder fbb;
    IdxFormat::Position posB(K_POS_B_FILE_ID, K_POS_B_LINE, K_POS_B_COL);
    auto loc_off = IdxFormat::CreateLocation(fbb, &posB, nullptr, K_ZERO_ULL);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND, K_CONTAINER_ID, K_IS_CJO_REF);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_POS_B_FILE_ID);
    EXPECT_EQ(out.location.begin.line, K_POS_B_LINE);
    EXPECT_EQ(out.location.begin.column, K_POS_B_COL);
    EXPECT_EQ(out.location.end.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.end.line, K_ZERO_U);
    EXPECT_EQ(out.location.end.column, K_ZERO_U);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND));
    EXPECT_EQ(out.container, K_CONTAINER_ID);
    EXPECT_EQ(out.isCjoRef, K_IS_CJO_REF);
}

TEST_F(IndexStorageTest, ReadRef004) {
    flatbuffers::FlatBufferBuilder fbb;
    IdxFormat::Position posE(K_POS_E_FILE_ID, K_POS_E_LINE, K_POS_E_COL);
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, &posE, K_ZERO_ULL);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND, K_CONTAINER_ID, K_IS_CJO_REF);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.begin.line, K_ZERO_U);
    EXPECT_EQ(out.location.begin.column, K_ZERO_U);
    EXPECT_EQ(out.location.end.fileID, K_POS_E_FILE_ID);
    EXPECT_EQ(out.location.end.line, K_POS_E_LINE);
    EXPECT_EQ(out.location.end.column, K_POS_E_COL);
    EXPECT_TRUE(out.location.fileUri.empty());
    EXPECT_EQ(out.kind, RefKind(K_KIND));
    EXPECT_EQ(out.container, K_CONTAINER_ID);
    EXPECT_EQ(out.isCjoRef, K_IS_CJO_REF);
}

TEST_F(IndexStorageTest, ReadRef005) {
    flatbuffers::FlatBufferBuilder fbb;
    auto uri_off = fbb.CreateString("file://path");
    auto loc_off = IdxFormat::CreateLocation(fbb, nullptr, nullptr, uri_off);
    auto ref_off = CreateRef(fbb, loc_off, K_KIND, K_CONTAINER_ID, K_IS_CJO_REF);
    fbb.Finish(ref_off);
    auto fb_ref = flatbuffers::GetRoot<IdxFormat::Ref>(fbb.GetBufferPointer());
    Ref out;
    ReadRef(out, fb_ref);
    EXPECT_EQ(out.location.begin.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.begin.line, K_ZERO_U);
    EXPECT_EQ(out.location.begin.column, K_ZERO_U);
    EXPECT_EQ(out.location.end.fileID, K_ZERO_U);
    EXPECT_EQ(out.location.end.line, K_ZERO_U);
    EXPECT_EQ(out.location.end.column, K_ZERO_U);
    EXPECT_EQ(out.location.fileUri, "file://path");
    EXPECT_EQ(out.kind, RefKind(K_KIND));
    EXPECT_EQ(out.container, K_CONTAINER_ID);
    EXPECT_EQ(out.isCjoRef, K_IS_CJO_REF);
}

TEST_F(IndexStorageTest, SplitFileName_EdgeCases) {
    // Case: No dot in filename
    auto res1 = SplitFileName("filename");
    EXPECT_EQ(res1.first, "");
    EXPECT_EQ(res1.second, "");

    // Case: Only one dot (SplitFileName requires at least two dots to extract second/first)
    auto res2 = SplitFileName("onlyone.dot");
    EXPECT_EQ(res2.first, "");
    EXPECT_EQ(res2.second, "");

    // Case: Empty string
    auto res3 = SplitFileName("");
    EXPECT_EQ(res3.first, "");
    EXPECT_EQ(res3.second, "");

    // Case: Hidden files or dots at start
    auto res4 = SplitFileName(".hidden.file.idx");
    EXPECT_EQ(res4.first, ".hidden");
    EXPECT_EQ(res4.second, "file");
}

TEST_F(IndexStorageTest, GetShardPath_PathConversion) {
    CacheManager cm(testRoot);
    // Ensure package names with slashes are converted to dots in shard path
    std::string path = cm.GetShardPathFromFilePath("ark/lsp/service", "hash123");
    EXPECT_TRUE(path.find("ark.lsp.service.hash123.idx") != std::string::npos);
}

TEST_F(IndexStorageTest, ReadRelation_Normal) {
    flatbuffers::FlatBufferBuilder fbb;
    auto rel_off = IdxFormat::CreateRelation(fbb, K_SUBJECT_ID, K_PREDICATE_ID, K_OBJECT_ID);
    fbb.Finish(rel_off);
    auto fb_rel = flatbuffers::GetRoot<IdxFormat::Relation>(fbb.GetBufferPointer());

    Relation out;
    ReadRelation(out, fb_rel);
    EXPECT_EQ(out.subject, K_SUBJECT_ID);
    EXPECT_EQ(out.predicate, RelationKind(K_PREDICATE_ID));
    EXPECT_EQ(out.object, K_OBJECT_ID);
}

TEST_F(IndexStorageTest, InitDir_CreatesNecessaryFolders) {
    CacheManager cm(testRoot);
    cm.InitDir();
    // Verify internal cache directory structure
    EXPECT_TRUE(fs::exists(testRoot + "/.cache/index"));
    EXPECT_TRUE(fs::exists(testRoot + "/.cache/astdata"));
}

TEST_F(IndexStorageTest, IsStale_And_UpdateIdMap) {
    CacheManager cm(testRoot);
    std::string pkg = "demo.pkg";
    std::string hash1 = "aaaa";
    std::string hash2 = "bbbb";

    // Not in map: should be stale
    EXPECT_TRUE(cm.IsStale(pkg, hash1));

    // After update: should be fresh
    cm.UpdateIdMap(pkg, hash1);
    EXPECT_FALSE(cm.IsStale(pkg, hash1));
    EXPECT_TRUE(cm.IsStale(pkg, hash2));
}

TEST_F(IndexStorageTest, LoadIndexShard_CorruptFile_ReturnsNull) {
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "bad.file";
    std::string shard = "x";
    std::string path = cm.GetShardPathFromFilePath(pkg, shard);

    // Create a file with invalid Flatbuffer content
    std::ofstream ofs(path);
    ofs << "This is not a flatbuffer binary data";
    ofs.close();

    // Verification should fail and remove the corrupt file
    auto res = cm.LoadIndexShard(pkg, shard);
    EXPECT_FALSE(res.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(IndexStorageTest, UtilityFunctions_Coverage) {
    auto res1 = SplitFileName("no_dot");
    EXPECT_EQ(res1.first, "");
    EXPECT_EQ(res1.second, "");

    auto res3 = SplitFileName("file.name.ext");
    EXPECT_EQ(res3.first, "file");
    EXPECT_EQ(res3.second, "name");

    std::string merged = MergeFileName("pkg/path", "hash123", "idx");
    EXPECT_FALSE(merged.empty());
}

TEST_F(IndexStorageTest, CacheManager_IsStale_Coverage) {
    CacheManager cm(testRoot);
    std::string pkg = "test.package";
    std::string hash = "old_hash";

    cm.UpdateIdMap(pkg, hash);
    EXPECT_FALSE(cm.IsStale(pkg, hash));
    EXPECT_TRUE(cm.IsStale(pkg, "new_hash"));
}

TEST_F(IndexStorageTest, LoadIndexShard_ErrorPaths) {
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "error.pkg";
    std::string hash = "h1";
    std::string path = cm.GetShardPathFromFilePath(pkg, hash);

    // File missing path
    auto res1 = cm.LoadIndexShard(pkg, hash);
    EXPECT_FALSE(res1.has_value());

    // Invalid format path
    std::ofstream ofs(path, std::ios::binary);
    ofs << "Corrupt data";
    ofs.close();

    auto res2 = cm.LoadIndexShard(pkg, hash);
    EXPECT_FALSE(res2.has_value());
    EXPECT_FALSE(fs::exists(path));
}

TEST_F(IndexStorageTest, LoadShard_NullSlabs) {
    CacheManager cm(testRoot);
    cm.InitDir();
    std::string pkg = "null.slabs";
    std::string hash = "h2";

    flatbuffers::FlatBufferBuilder fbb;
    auto pkg_off = IdxFormat::CreateHashedPackage(fbb, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL);
    IdxFormat::FinishHashedPackageBuffer(fbb, pkg_off);

    std::string path = cm.GetShardPathFromFilePath(pkg, hash);
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    ofs.close();

    auto res = cm.LoadIndexShard(pkg, hash);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE((*res)->refs.empty());
    EXPECT_TRUE((*res)->extends.empty());
}

TEST_F(IndexStorageTest, ReadSymbol_OptionalFieldMissing) {
    flatbuffers::FlatBufferBuilder fbb;
    auto sym_off = IdxFormat::CreateSymbol(fbb, K_SYMBOL_ID_DEFAULT, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL, K_ZERO_ULL,
                                          false, K_ZERO_U, false, false, K_ZERO_U,
                                          K_ZERO_U, K_ZERO_U);
    fbb.Finish(sym_off);

    auto fb_sym = flatbuffers::GetRoot<IdxFormat::Symbol>(fbb.GetBufferPointer());
    Symbol res;
    ReadSymbol(res, fb_sym);

    EXPECT_EQ(res.id, K_SYMBOL_ID_DEFAULT);
    EXPECT_TRUE(res.name.empty());
}

TEST_F(IndexStorageTest, CacheManager_IsStale_FileMissing) {
    CacheManager cm(testRoot);
    std::string pkg = "missing.file.pkg";
    std::string hash = "hash123";

    cm.UpdateIdMap(pkg, hash);
    // Requesting a new hash should trigger staleness check
    EXPECT_TRUE(cm.IsStale(pkg, "new_hash"));
}

TEST_F(IndexStorageTest, StoreSymbol_EmptyFields) {
    flatbuffers::FlatBufferBuilder builder;
    Symbol sym;
    sym.id = K_SUBJECT_ID;

    auto offset = StoreSymbol(builder, sym);
    builder.Finish(offset);

    auto fb_sym = flatbuffers::GetRoot<IdxFormat::Symbol>(builder.GetBufferPointer());
    EXPECT_EQ(fb_sym->id(), K_SUBJECT_ID);
    EXPECT_TRUE(fb_sym->name()->str().empty());
}

TEST_F(IndexStorageTest, AstFileHandler_StoreShard_InvalidType) {
    AstFileHandler handler;
    handler.StoreShard("any.ast", nullptr);

    AstFileOut out;
    out.data = nullptr;
    handler.StoreShard("any.ast", &out);
}