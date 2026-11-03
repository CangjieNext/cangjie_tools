// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "ArkASTWorker.h"
#include "CompilerCangjieProject.h"

#if defined(__has_include)
#if __has_include(<filesystem>)
#include <filesystem>
namespace refreshfs = std::filesystem;
#else
#include <experimental/filesystem>
namespace refreshfs = std::experimental::filesystem;
#endif
#else
#include <filesystem>
namespace refreshfs = std::filesystem;
#endif

namespace {
using namespace ark;
using namespace Cangjie;
using namespace Cangjie::AST;

class RefreshCallbacks : public Callbacks {
public:
    std::map<std::string, std::string> contents;
    std::set<std::string> needsReparse;
    std::map<std::string, std::set<DiagnosticToken, DiagnosticCompare>> diagnostics;
    std::map<std::string, size_t> publications;

    std::vector<DiagnosticToken> GetDiagsOfCurFile(std::string file) override
    {
        const auto &items = diagnostics[file];
        return {items.begin(), items.end()};
    }
    void UpdateDiagnostic(std::string file, DiagnosticToken diagnostic) override
    {
        diagnostics[file].insert(std::move(diagnostic));
    }
    void RemoveDiagnostic(std::string file, DiagnosticToken diagnostic) override
    {
        diagnostics[file].erase(diagnostic);
    }
    void RemoveDiagOfCurFile(const std::string &file) override
    {
        diagnostics.erase(file);
    }
    void RemoveDiagOfCurPkg(const std::string &) override {}
    void RemoveUnusedDiagsOfCurFile(const std::string &) override {}
    void ReadyForDiagnostics(std::string file, int64_t, std::vector<DiagnosticToken>) override
    {
        ++publications[file];
    }
    std::string GetContentsByFile(const std::string &file) override
    {
        return contents[file];
    }
    int64_t GetVersionByFile(const std::string &) override
    {
        return 1;
    }
    void RemoveDocByFile(const std::string &file) override
    {
        diagnostics.erase(file);
    }
    bool NeedReParser(const std::string &file) override
    {
        return needsReparse.count(file) != 0;
    }
    void UpdateDocNeedReparse(const std::string &, int64_t, bool) override {}
    void AddDocWhenInitCompile(const std::string &) override {}
    void ReportCjoVersionErr(std::string) override {}
    void PublishCompletionTip(const CompletionTip &) override {}
    std::string CreateProgressToken() override
    {
        return {};
    }
    void SendWorkDoneProgressCreate(const WorkDoneProgressCreateParams &) override {}
    void SendWorkDoneProgressBegin(const std::string &, const WorkDoneProgressBegin &) override {}
    void SendWorkDoneProgressReport(const std::string &, const WorkDoneProgressReport &) override {}
    void SendWorkDoneProgressEnd(const std::string &, const WorkDoneProgressEnd &) override {}
};

class DiagnosticWorkerScope {
public:
    explicit DiagnosticWorkerScope(Callbacks &callbacks)
        : worker(ArkASTWorker::Create(runner, barrier, &callbacks))
    {
    }

    ~DiagnosticWorkerScope()
    {
        if (worker) {
            worker->Stop();
        }
        runner.Wait();
    }

    Semaphore barrier{1};
    AsyncTaskRunner runner;
    ArkASTWorker *worker;
};

class RefreshScenario {
public:
    ~RefreshScenario()
    {
        if (!root.empty()) {
            refreshfs::remove_all(root);
        }
    }

    void Initialize()
    {
        const auto *sdk = std::getenv("CANGJIE_HOME");
        ASSERT_NE(sdk, nullptr);
        const char *arguments[] = {"internal-import-refresh-test", "--test"};
        ark::Options::GetInstance().Parse(2, arguments);
        const auto id = std::chrono::steady_clock::now().time_since_epoch().count();
        root = refreshfs::temp_directory_path();
        root.append("lsp_internal_import_refresh_" + std::to_string(id));
        auto source = root;
        source.append("src");
        refreshfs::create_directories(source);
        mainFile = Write("main.cj", "package src\ninternal import std.fs.File\nmain(): Int64 { 0 }\n");
        helperFile = Write("helper.cj", "package src\ninternal import std.fs.Directory\n");
        childFile = Write("child/child.cj", CHILD_SOURCE);
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        Environment environment;
        environment.cangjieHome = sdk;
        environment.runtimePath = Cangjie::FileUtil::JoinPath(sdk, "runtime/lib");
        const auto uri = URI::URIFromAbsolutePath(root.string()).ToString();
        nlohmann::json options;
        options["multiModuleOption"][uri] = {{"name", "src"}, {"requires", nlohmann::json::object()}};
        CompilerCangjieProject::InitInstance(&callbacks, nullptr);
        project = CompilerCangjieProject::GetInstance();
        ASSERT_TRUE(project->Compiler(uri, options, environment));
        ASSERT_TRUE(project->pLRUCache->HasCache("src"));
        parent = project->pLRUCache->Get("src").get();
        ASSERT_NE(parent, nullptr);
        parentPackage = parent->GetSourcePackages().front();
        originalNoSubPkg = parent->invocation.globalOptions.noSubPkg;
        auto data = project->GetCjoManager()->GetData("src");
        ASSERT_NE(data, nullptr);
        originalCjo = *data;
        ASSERT_FALSE(parentPackage->noSubPkg);
        AssertNoErrors(mainFile);
        AssertNoErrors(helperFile);
        ASSERT_EQ(Unused(mainFile), 0u);
    }

    void VerifyDeferredRefresh()
    {
        Initialize();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        callbacks.needsReparse.insert(mainFile);
        callbacks.contents[mainFile] += "\n// pending edit\n";
        project->GetCjoManager()->UpdateStatus({"src"}, DataStatus::WEAKSTALE);
        DeleteChild();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(Unused(mainFile), 0u);
        EXPECT_EQ(callbacks.publications[mainFile], 0u);
        ExpectParentUnchanged();

        project->GetCjoManager()->UpdateStatus({"src"}, DataStatus::FRESH);
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(callbacks.publications[mainFile], 0u);
        callbacks.contents[mainFile] = project->GetContentByFile(mainFile);
        callbacks.needsReparse.insert(helperFile);
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(Unused(mainFile), 1u);
        EXPECT_EQ(Unused(helperFile), 1u);
        EXPECT_EQ(callbacks.publications[mainFile], 1u);
        EXPECT_EQ(callbacks.publications[helperFile], 1u);
        EXPECT_TRUE(callbacks.NeedReParser(mainFile));
        EXPECT_TRUE(callbacks.NeedReParser(helperFile));
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(callbacks.publications[mainFile], 1u);
        ExpectParentUnchanged();
        VerifyNewParseUsesCurrentTopology();
    }

    void VerifyMissingFileAndRecreatedChild()
    {
        Initialize();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        ASSERT_TRUE(refreshfs::remove(refreshfs::path(helperFile)));
        DeleteChild();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(callbacks.publications[mainFile], 0u);
        Write("helper.cj", callbacks.contents[helperFile]);
        Write("child/child.cj", CHILD_SOURCE);
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(Unused(mainFile), 0u);
        ExpectParentUnchanged();
    }

    void VerifyCompletionRestoresPendingDiagnostics()
    {
        Initialize();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        DeleteChild();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        project->pLRUCache->EraseCache("src");
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(callbacks.publications[helperFile], 0u);
        {
            DiagnosticWorkerScope ordinary(callbacks);
            ASSERT_NE(ordinary.worker, nullptr);
            std::unique_lock<Semaphore> gate(ordinary.barrier);
            Semaphore completionBarrier(1);
            ArkASTWorker completion(completionBarrier, &callbacks, ordinary.worker);
            completion.DoCompletionWithASTCache("Completion", mainFile, {0, 1, 1}, [](InputsAndAST) {});
            EXPECT_EQ(callbacks.publications[helperFile], 0u);
        }
        EXPECT_EQ(callbacks.publications[helperFile], 1u);
        EXPECT_EQ(Unused(helperFile), 1u);
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(callbacks.publications[helperFile], 1u);
    }

    void VerifyRecreatedChildAfterParentCompile()
    {
        Initialize();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        DeleteChild();
        ASSERT_FALSE(testing::Test::HasFatalFailure());
        project->IncrementTempPkgCompile("src");
        ASSERT_TRUE(project->pLRUCache->HasCache("src"));
        const auto &compiled = project->pLRUCache->Get("src");
        ASSERT_NE(compiled, nullptr);
        ASSERT_EQ(compiled->GetSourcePackages().size(), 1u);
        EXPECT_TRUE(compiled->GetSourcePackages().front()->noSubPkg);
        EXPECT_EQ(compiled->invocation.globalOptions.noSubPkg, originalNoSubPkg);
        AssertNoErrors(mainFile);

        Write("child/child.cj", "package src.child\nimport src.File\nfunc retain(file: File): File { file }\n");
        project->CompilerOneFile(childFile, callbacks.contents[childFile]);
        ASSERT_TRUE(project->pLRUCache->HasCache("src.child"));
        ASSERT_TRUE(project->pLRUCache->HasCache("src"));
        const auto &recompiled = project->pLRUCache->Get("src");
        ASSERT_NE(recompiled, nullptr);
        ASSERT_EQ(recompiled->GetSourcePackages().size(), 1u);
        EXPECT_FALSE(recompiled->GetSourcePackages().front()->noSubPkg);
        EXPECT_EQ(recompiled->invocation.globalOptions.noSubPkg, originalNoSubPkg);
        AssertNoErrors(childFile);
        AssertNoErrors(mainFile);
        project->RefreshPendingInternalImportDiagnostics();
        EXPECT_EQ(Unused(mainFile), 0u);
    }

private:
    static constexpr const char *CHILD_SOURCE = "package src.child\npublic func childValue(): Int64 { 1 }\n";

    std::string Write(const std::string &relative, const std::string &content)
    {
        auto path = root;
        path.append("src");
        path.append(relative);
        refreshfs::create_directories(path.parent_path());
        std::ofstream stream(path.string(), std::ios::binary);
        if (!stream.good()) {
            ADD_FAILURE() << "Cannot create fixture " << path.string();
            return {};
        }
        stream << content;
        stream.close();
        const auto normalized = FileStore::NormalizePath(path.string());
        callbacks.contents[normalized] = content;
        return normalized;
    }

    void DeleteChild()
    {
        ASSERT_TRUE(refreshfs::remove(refreshfs::path(childFile)));
        project->IncrementForFileDelete(childFile);
    }

    size_t Unused(const std::string &file)
    {
        const auto items = callbacks.GetDiagsOfCurFile(file);
        return std::count_if(items.begin(), items.end(), [](const auto &item) {
            return item.diagFix && item.diagFix->removeImport;
        });
    }

    void AssertNoErrors(const std::string &file)
    {
        for (const auto &diagnostic : callbacks.GetDiagsOfCurFile(file)) {
            EXPECT_NE(diagnostic.severity, static_cast<int>(DiagLSPSeverity::ERROR_DIAG)) << diagnostic.message;
        }
    }

    void ExpectParentUnchanged()
    {
        ASSERT_EQ(project->pLRUCache->Get("src").get(), parent);
        EXPECT_EQ(parent->GetSourcePackages().front(), parentPackage);
        EXPECT_EQ(parent->invocation.globalOptions.noSubPkg, originalNoSubPkg);
        const auto data = project->GetCjoManager()->GetData("src");
        ASSERT_NE(data, nullptr);
        EXPECT_EQ(*data, originalCjo);
    }

    void VerifyNewParseUsesCurrentTopology()
    {
        std::unique_ptr<ModuleManager> moduleManager;
        auto engine = std::make_unique<DiagnosticEngine>();
        LSPCompilerInstance parsed(&callbacks, parent->invocation, std::move(engine), "src", moduleManager);
        parsed.loadSrcFilesFromCache = true;
        parsed.SetBufferCache({{mainFile, callbacks.contents[mainFile]}, {helperFile, callbacks.contents[helperFile]}});
        parsed.PreCompileProcess();
        ASSERT_EQ(parsed.GetSourcePackages().size(), 1u);
        EXPECT_TRUE(parsed.GetSourcePackages().front()->noSubPkg);
        EXPECT_EQ(parent->invocation.globalOptions.noSubPkg, originalNoSubPkg);
        EXPECT_EQ(parentPackage->noSubPkg, originalNoSubPkg);
    }

    RefreshCallbacks callbacks;
    CompilerCangjieProject *project = nullptr;
    LSPCompilerInstance *parent = nullptr;
    Ptr<Package> parentPackage;
    SerializedT originalCjo;
    bool originalNoSubPkg = false;
    refreshfs::path root;
    std::string mainFile;
    std::string helperFile;
    std::string childFile;
};

TEST(InternalImportRefreshDeathTest, RetriesValidCacheWithoutRecompilingParent)
{
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    ASSERT_EXIT({
        {
            RefreshScenario scenario;
            scenario.VerifyDeferredRefresh();
        }
        std::exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        }, ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(InternalImportRefreshDeathTest, RechecksFilesystemAfterMissingFileDeferral)
{
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    ASSERT_EXIT({
        {
            RefreshScenario scenario;
            scenario.VerifyMissingFileAndRecreatedChild();
        }
        std::exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        }, ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(InternalImportRefreshDeathTest, ReexportsInternalImportWhenChildReturnsAfterParentCompile)
{
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    ASSERT_EXIT({
        {
            RefreshScenario scenario;
            scenario.VerifyRecreatedChildAfterParentCompile();
        }
        std::exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        }, ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST(InternalImportRefreshDeathTest, CompletionRestoresDiagnosticsThroughOrdinaryWorker)
{
    ::testing::GTEST_FLAG(death_test_style) = "threadsafe";
    ASSERT_EXIT({
        {
            RefreshScenario scenario;
            scenario.VerifyCompletionRestoresPendingDiagnostics();
        }
        std::exit(::testing::Test::HasFailure() ? EXIT_FAILURE : EXIT_SUCCESS);
        }, ::testing::ExitedWithCode(EXIT_SUCCESS), "");
}
} // namespace
