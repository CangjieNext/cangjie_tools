// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "Checker.h"
#include <string>
#include <thread>
#include <vector>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include "common/CommonFunc.h"
#include "common/ConfigContext.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_CHK_01/DataflowRuleGCHK01Check.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_FIO_01/DataflowRuleGFIO01Check.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_OTH_01/DataflowRuleGOTH01Check.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_SER_03/DataflowRuleGSER03Checker.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_VAR_01/DataflowRuleVAR01Check.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_G_CHK_03/DataflowRuleGCHK03Check.h"
#include "rules/dataflow_rule_analysis/dataflow_rule_P_02/DataflowRuleP02Check.h"
#include "rules/structural_rule_analysis/StructuralRuleGCHK02/StructuralRuleGCHK02.h"
#include "rules/structural_rule_analysis/StructuralRuleGCHK04/StructuralRuleGCHK04.h"
#include "rules/structural_rule_analysis/StructuralRuleGCLS01/StructuralRuleGCLS01.h"
#include "rules/structural_rule_analysis/StructuralRuleGCON01/StructuralRuleGCON01.h"
#include "rules/structural_rule_analysis/StructuralRuleGCON02/StructuralRuleGCON02.h"
#include "rules/structural_rule_analysis/StructuralRuleGCON03/StructuralRuleGCON03.h"
#include "rules/structural_rule_analysis/StructuralRuleGDCL01/StructuralRuleGDCL01.h"
#include "rules/structural_rule_analysis/StructuralRuleGDCL02/StructuralRuleGDCL02.h"
#include "rules/structural_rule_analysis/StructuralRuleGENU01/StructuralRuleGENU01.h"
#include "rules/structural_rule_analysis/StructuralRuleGENU02/StructuralRuleGENU02.h"
#include "rules/structural_rule_analysis/StructuralRuleGERR01/StructuralRuleGERR01.h"
#include "rules/structural_rule_analysis/StructuralRuleGERR02/StructuralRuleGERR02.h"
#include "rules/structural_rule_analysis/StructuralRuleGERR03/StructuralRuleGERR03.h"
#include "rules/structural_rule_analysis/StructuralRuleGERR04/StructuralRuleGERR04.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP01/StructuralRuleGEXP01.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP02/StructuralRuleGEXP02.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP03/StructuralRuleGEXP03.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP04/StructuralRuleGEXP04.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP05/StructuralRuleGEXP05.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP06/StructuralRuleGEXP06.h"
#include "rules/structural_rule_analysis/StructuralRuleGEXP07/StructuralRuleGEXP07.h"
#include "rules/structural_rule_analysis/StructuralRuleGFMT01/StructuralRuleGFMT01.h"
#include "rules/structural_rule_analysis/StructuralRuleGFMT13/StructuralRuleGFMT13.h"
#include "rules/structural_rule_analysis/StructuralRuleGFMT15/StructuralRuleGFMT15.h"
#include "rules/structural_rule_analysis/StructuralRuleGFUN01/StructuralRuleGFUN01.h"
#include "rules/structural_rule_analysis/StructuralRuleGFUN02/StructuralRuleGFUN02.h"
#include "rules/structural_rule_analysis/StructuralRuleGFUNC03/StructuralRuleGFUNC03.h"
#include "rules/structural_rule_analysis/StructuralRuleGITF01/StructuralRuleGITF01.h"
#include "rules/structural_rule_analysis/StructuralRuleGITF02/StructuralRuleGITF02.h"
#include "rules/structural_rule_analysis/StructuralRuleGITF03/StructuralRuleGITF03.h"
#include "rules/structural_rule_analysis/StructuralRuleGITF04/StructuralRuleGITF04.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM01/StructuralRuleGNAM01.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM02/StructuralRuleGNAM02.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM03/StructuralRuleGNAM03.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM04/StructuralRuleGNAM04.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM05/StructuralRuleGNAM05.h"
#include "rules/structural_rule_analysis/StructuralRuleGNAM06/StructuralRuleGNAM06.h"
#include "rules/structural_rule_analysis/StructuralRuleGOPR01/StructuralRuleGOPR01.h"
#include "rules/structural_rule_analysis/StructuralRuleGOPR02/StructuralRuleGOPR02.h"
#include "rules/structural_rule_analysis/StructuralRuleGOTH02/StructuralRuleGOTH02.h"
#include "rules/structural_rule_analysis/StructuralRuleGOTH03/StructuralRuleGOTH03.h"
#include "rules/structural_rule_analysis/StructuralRuleGOTH04/StructuralRuleGOTH04.h"
#include "rules/structural_rule_analysis/StructuralRuleGPKG01/StructuralRuleGPKG01.h"
#include "rules/structural_rule_analysis/StructuralRuleGSEC01/StructuralRuleGSEC01.h"
#include "rules/structural_rule_analysis/StructuralRuleGSER01/StructuralRuleGSER01.h"
#include "rules/structural_rule_analysis/StructuralRuleGSER02/StructuralRuleGSER02.h"
#include "rules/structural_rule_analysis/StructuralRuleGSYN01/StructuralRuleGSYN01.h"
#include "rules/structural_rule_analysis/StructuralRuleGSYN01/StructuralRuleGSYN0101.h"
#include "rules/structural_rule_analysis/StructuralRuleGTYP03/StructuralRuleGTYP03.h"
#include "rules/structural_rule_analysis/StructuralRuleGVAR02/StructuralRuleGVAR02.h"
#include "rules/structural_rule_analysis/StructuralRuleGVAR03/StructuralRuleGVAR03.h"
#include "rules/structural_rule_analysis/StructuralRuleP01/StructuralRuleP01.h"
#include "rules/structural_rule_analysis/StructuralRuleFFIC7/StructuralRuleFFIC7.h"
#include "rules/structural_rule_analysis/StructuralRuleP03/StructuralRuleP03.h"

using namespace Cangjie;
using namespace Cangjie::CodeCheck;
using Json = nlohmann::json;

namespace {
#ifdef _WIN32
const std::string LIB_EXTENSION = ".dll";
#else
const std::string LIB_EXTENSION = ".so";
#endif
const std::string CACHED_EXTENSION = "_Cache";
const std::string CJLINT_RULE_LIST = "/config/cjlint_rule_list.json";
static const std::vector<std::string> g_fileExtension{
    ".cjo", ".bc", ".cachedast", ".iast", LIB_EXTENSION, ".o", ".bchir", ".cbc", ".pdba"};
static const std::map<std::string, std::string> ruleConfig = {{"G.OTH.01", "/config/dataflow_rule_G_OTH_01.json"},
    {"G.CHK.02", "/config/structural_rule_G_CHK_02.json"}, {"G.CHK.04", "/config/structural_rule_G_CHK_04.json"},
    {"G.ERR.02", "/config/structural_rule_G_ERR_02.json"}, {"G.OTH.02", "/config/structural_rule_G_OTH_02.json"},
    {"G.OTH.04", "/config/structural_rule_G_OTH_04.json"}, {"G.SEC.01", "/config/structural_rule_G_SEC_01.json"},
    {"G.SER.01", "/config/structural_rule_G_SER_01.json"}, {"G.SYN.01", "/config/structural_rule_G_SYN_01.json"}};
} // namespace

Checker::Checker(CodeCheckDiagnosticEngine* diagEngine) : diagEngine(diagEngine), rulesMap(RulesMap())
{
    ConfigContext& configContext = ConfigContext::GetInstance();
    fileDir = configContext.GetSrcFileDir()[0];
    modulesDir = configContext.GetModulesDir();
    RegisterRule<DataflowRuleGCHK01Check>("G.CHK.01", CompileStage::CHIR);
    RegisterRule<DataflowRuleGFIO01Check>("G.FIO.01", CompileStage::CHIR);
    RegisterRule<DataflowRuleGOTH01Check>("G.OTH.01", CompileStage::CHIR);
    RegisterRule<DataflowRuleGSER03Checker>("G.SER.03", CompileStage::CHIR);
    RegisterRule<DataflowRuleP02Check>("P.02", CompileStage::CHIR);
    RegisterRule<DataflowRuleVAR01Check>("G.VAR.01", CompileStage::CHIR);
    RegisterRule<DataflowRuleGCHK03Check>("G.CHK.03", CompileStage::CHIR);
    RegisterRule<StructuralRuleGENU01>("G.ENU.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP01>("G.EXP.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP02>("G.EXP.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGENU02>("G.ENU.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP03>("G.EXP.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGDCL01>("G.DCL.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGDCL02>("G.DCL.02", CompileStage::PARSE);
    RegisterRule<StructuralRuleGEXP04>("G.EXP.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFMT01>("G.FMT.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFMT13>("G.FMT.13", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFMT15>("G.FMT.15", CompileStage::SEMA);
    RegisterRule<StructuralRuleGOTH02>("G.OTH.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGOTH03>("G.OTH.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGOTH04>("G.OTH.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCON01>("G.CON.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCON03>("G.CON.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFUN01>("G.FUN.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFUN02>("G.FUN.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGFUNC03>("G.FUN.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM01>("G.NAM.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM02>("G.NAM.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM03>("G.NAM.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM04>("G.NAM.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM05>("G.NAM.05", CompileStage::SEMA);
    RegisterRule<StructuralRuleGNAM06>("G.NAM.06", CompileStage::SEMA);
    RegisterRule<StructuralRuleGSEC01>("G.SEC.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGSER01>("G.SER.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGSER02>("G.SER.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCHK02>("G.CHK.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCHK04>("G.CHK.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGERR01>("G.ERR.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGERR02>("G.ERR.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGERR04>("G.ERR.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCON02>("G.CON.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleP01>("P.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleP03>("P.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGVAR02>("G.VAR.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGVAR03>("G.VAR.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGPKG01>("G.PKG.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGERR03>("G.ERR.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP05>("G.EXP.05", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP06>("G.EXP.06", CompileStage::SEMA);
    RegisterRule<StructuralRuleGEXP07>("G.EXP.07", CompileStage::SEMA);
    RegisterRule<StructuralRuleGOPR01>("G.OPR.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGOPR02>("G.OPR.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGITF01>("G.ITF.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGITF02>("G.ITF.02", CompileStage::SEMA);
    RegisterRule<StructuralRuleGCLS01>("G.CLS.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGITF03>("G.ITF.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleGITF04>("G.ITF.04", CompileStage::SEMA);
    RegisterRule<StructuralRuleGSYN01>("G.SYN.01", CompileStage::PARSE);
    RegisterRule<StructuralRuleGSYN0101>("G.SYN.01", CompileStage::SEMA);
    RegisterRule<StructuralRuleGTYP03>("G.TYP.03", CompileStage::SEMA);
    RegisterRule<StructuralRuleFFIC7>("FFI.C.7", CompileStage::SEMA);
}

void Checker::CreateRuleThreads(Json jsonInfo, const std::unique_ptr<CJLintCompilerInstance>& compilerInstance,
    CompileStage stage, int& readJsonCode)
{
    std::vector<std::thread> threads;
    if (!jsonInfo.contains("RuleList")) {
        readJsonCode = JSON_READ_ERR;
        return;
    }

    std::set<std::string> checkedLst;
    for (const auto& item : jsonInfo["RuleList"]) {
        std::string upperStr = item.get<std::string>();
        (void)std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(), toupper);
        auto res = checkedLst.insert(upperStr);
        if (!res.second) {
            continue;
        }
        auto rules = rulesMap[stage];
        if (rules.count(upperStr) == 0) {
            continue;
        }
        if (ruleConfig.count(upperStr) == 0) {
            threads.emplace_back(&DoAnalysis, rules[upperStr].get(), compilerInstance.get());
            continue;
        }
        Json ruleJsonInfo;
        if (CommonFunc::ReadJsonFileToJsonInfo(ruleConfig.find(upperStr)->second,
            ConfigContext::GetInstance(), ruleJsonInfo) == ERR) {
            readJsonCode = JSON_READ_ERR;
        } else {
            threads.emplace_back(&DoAnalysis, rules[upperStr].get(), compilerInstance.get());
        }
    }
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

static void RemoveExtensionFileAndDir(std::set<std::string>& macroPath)
{
    for (auto& filePath : macroPath) {
        auto pos = filePath.rfind(PATH_SEPARATOR, filePath.length());
        auto dir = filePath.substr(0, pos + 1);
        auto pkgName = filePath.substr(pos + 1, filePath.length());
        auto dirPath = dir + "." + pkgName + CACHED_EXTENSION;
        auto macroLib = dir + PATH_SEPARATOR + "lib-macro_" + pkgName + LIB_EXTENSION;
        if (FileUtil::FileExist(macroLib)) {
            (void)FileUtil::Remove(macroLib);
        }
        for (auto& extension : g_fileExtension) {
            std::string file = filePath + extension;
            std::string fileInDir = dirPath + PATH_SEPARATOR + pkgName + extension;
            if (FileUtil::FileExist(file)) {
                (void)FileUtil::Remove(file);
            }
            if (FileUtil::FileExist(fileInDir)) {
                (void)FileUtil::Remove(fileInDir);
            }
        }
        (void)FileUtil::Remove(dirPath);
    }
}

#ifndef _WIN32
static bool WritePipeMessage(int fd, const std::string& data)
{
    uint32_t len = static_cast<uint32_t>(data.size());
    if (write(fd, &len, sizeof(len)) != static_cast<ssize_t>(sizeof(len))) {
        return false;
    }
    size_t totalWritten = 0;
    while (totalWritten < data.size()) {
        ssize_t n = write(fd, data.c_str() + totalWritten, data.size() - totalWritten);
        if (n <= 0) {
            return false;
        }
        totalWritten += static_cast<size_t>(n);
    }
    return true;
}

static bool ReadPipeMessage(int fd, std::string& data)
{
    uint32_t len = 0;
    ssize_t bytesRead = read(fd, &len, sizeof(len));
    if (bytesRead != static_cast<ssize_t>(sizeof(len)) || len == 0) {
        return false;
    }
    data.resize(len);
    size_t totalRead = 0;
    while (totalRead < len) {
        ssize_t n = read(fd, &data[totalRead], len - totalRead);
        if (n <= 0) {
            return false;
        }
        totalRead += static_cast<size_t>(n);
    }
    return true;
}

static std::string GetStageName(CompileStage stage)
{
    switch (stage) {
        case CompileStage::PARSE:
            return "PARSE";
        case CompileStage::SEMA:
            return "SEMA";
        case CompileStage::CHIR:
            return "CHIR";
        default:
            return "UNKNOWN";
    }
}
void Checker::SendStageResult(int writeFd, CompileStage stage, int readJsonCode, 
                              std::unordered_set<std::string>& sentDiagnostics)
{
    nlohmann::ordered_json msg;
    msg["stage"] = GetStageName(stage);
    msg["readJsonCode"] = readJsonCode;
    msg["errorCount"] = CodeCheckDiagnosticEngine::GetStaticErrorCount();
    msg["warningCount"] = CodeCheckDiagnosticEngine::GetStaticWarningCount();

    nlohmann::ordered_json allDiagnostics = diagEngine->SerializeDiagnostics();
    nlohmann::ordered_json stageDiagnostics = nlohmann::ordered_json::array();
    for (const auto& diagItem : allDiagnostics) {
        auto serialized = diagItem.dump();
        if (sentDiagnostics.insert(serialized).second) {
            stageDiagnostics.push_back(diagItem);
        }
    }
    msg["diagnostics"] = stageDiagnostics;
    WritePipeMessage(writeFd, msg.dump());
}


void Checker::ExecuteStage(const nlohmann::json& jsonInfo, const std::unique_ptr<CJLintCompilerInstance>& compilerInstance, 
                           CompileStage stage, int& readJsonCode, bool& isSuccess)
{
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        if (stage == CompileStage::PARSE) {
            isSuccess = compilerInstance->Compile(stage);
        } else {
            isSuccess = isSuccess && compilerInstance->Compile(stage);
        }
        CreateRuleThreads(jsonInfo, compilerInstance, stage, readJsonCode);
#ifndef CANGJIE_ENABLE_GCOV
    } catch (const std::out_of_range& e) {
        Errorln("Out of range exception caught: ", e.what());
    } catch (const std::exception& e) {
        Errorln("Exception caught: ", e.what());
    } catch (...) {
        Errorln("Unknown exception caught.");
    }
#endif
}
 
void Checker::RunWorker(int writeFd)
{
    DiagnosticEngine diag;
#ifdef NDEBUG
    diag.SetIsEmitter(false);
    diag.SetIsDumpErrCnt(false);
#endif
    Json jsonInfo;
    int readJsonCode = OK;
    bool isSuccess = true;

    if (CommonFunc::ReadJsonFileToJsonInfo(CJLINT_RULE_LIST, ConfigContext::GetInstance(), jsonInfo) == ERR) {
        CJLintCompilerInvocation::GetInstance().DesInitRuntime();
        close(writeFd);
        _exit(1);
    }

    auto compilerInstance = CJLintCompilerInvocation::GetInstance().PrePareCompilerInstance(diag);
    diagEngine->SetSourceManager(&compilerInstance->GetSourceManager());

    std::unordered_set<std::string> sentDiagnostics;
    static constexpr CompileStage STAGES[] = {CompileStage::PARSE, CompileStage::SEMA, CompileStage::CHIR};
    for (auto stage : STAGES) {
        ExecuteStage(jsonInfo, compilerInstance, stage, readJsonCode, isSuccess);
        SendStageResult(writeFd, stage, readJsonCode, sentDiagnostics);
    }

    CJLintCompilerInvocation::GetInstance().DesInitRuntime();
    RemoveExtensionFileAndDir(compilerInstance->macroPath);
    close(writeFd);
    _exit(0);
}
 
int Checker::ReadStageResults(int readFd)
{
    int readJsonCode = OK;
    int stagesReceived = 0;
    static const char* STAGE_NAMES[] = {"PARSE", "SEMA", "CHIR"};
    constexpr int TOTAL_STAGES = 3;

    for (int i = 0; i < TOTAL_STAGES; ++i) {
        std::string stageData;
        if (!ReadPipeMessage(readFd, stageData)) {
            Errorln("Process isolation: child process crashed during ", STAGE_NAMES[i],
                " stage, recovered diagnostics from ", stagesReceived, " completed stage(s)");
            break;
        }
        auto msg = nlohmann::ordered_json::parse(stageData);
        readJsonCode = msg["readJsonCode"].get<int>();
        diagEngine->ImportSerializedDiagnostics(msg["diagnostics"]);
        ++stagesReceived;
    }
    return readJsonCode;
}
 
 
int Checker::CheckCode()
{
    int pipeFd[2]; 
    if (pipe(pipeFd) == -1) {
        Errorln("Failed to create pipe");
        return ERR;
    }
 
    pid_t pid = fork();
    if (pid == -1) {
        close(pipeFd[0]);
        close(pipeFd[1]);
        Errorln("Failed to fork child process");
        return ERR;
    }
 
    if (pid == 0) {
        close(pipeFd[0]);
        RunWorker(pipeFd[1]);
    }
 
    close(pipeFd[1]);
    int readJsonCode = ReadStageResults(pipeFd[0]);
    close(pipeFd[0]);
 
    int status = 0;
    waitpid(pid, &status, 0);
    return readJsonCode;
}
//int Checker::CheckCode()
//{
//    int pipeFd[2];
//    if (pipe(pipeFd) == -1) {
//        Errorln("Failed to create pipe");
//        return ERR;
//    }
//
//    pid_t pid = fork();
//    if (pid == -1) {
//        close(pipeFd[0]);
//        close(pipeFd[1]);
//        Errorln("Failed to fork child process");
//        return ERR;
//    }
//
//    if (pid == 0) {
//        close(pipeFd[0]);
//        int writeFd = pipeFd[1];
//
//        DiagnosticEngine diag;
//#ifdef NDEBUG
//        diag.SetIsEmitter(false);
//        diag.SetIsDumpErrCnt(false);
//#endif
//        Json jsonInfo;
//        int readJsonCode = OK;
//        bool isSuccess = true;
//        if (CommonFunc::ReadJsonFileToJsonInfo(CJLINT_RULE_LIST, ConfigContext::GetInstance(), jsonInfo) == ERR) {
//            CJLintCompilerInvocation::GetInstance().DesInitRuntime();
//            close(writeFd);
//            _exit(1);
//        }
//
//        auto compilerInstance = CJLintCompilerInvocation::GetInstance().PrePareCompilerInstance(diag);
//        diagEngine->SetSourceManager(&compilerInstance->GetSourceManager());
//	//size_t prevDiagCount = 0;
//
//        std::unordered_set<std::string> sentDiagnostics;
//	
//        auto sendStageResult = [&](CompileStage stage) {
//            nlohmann::ordered_json msg;
//            msg["stage"] = GetStageName(stage);
//            msg["readJsonCode"] = readJsonCode;
//            msg["errorCount"] = CodeCheckDiagnosticEngine::GetStaticErrorCount();
//            msg["warningCount"] = CodeCheckDiagnosticEngine::GetStaticWarningCount();
//            //msg["diagnostics"] = diagEngine->SerializeDiagnosticsFrom(prevDiagCount);
//            //prevDiagCount = diagEngine->GetDiagnosticCount();
//            nlohmann::ordered_json allDiagnostics = diagEngine->SerializeDiagnostics();
//            nlohmann::ordered_json stageDiagnostics = nlohmann::ordered_json::array();
//            for (const auto& diagItem : allDiagnostics) {
//                auto serialized = diagItem.dump();
//                if (sentDiagnostics.insert(serialized).second) {
//                    stageDiagnostics.push_back(diagItem);
//                }
//            }
//            msg["diagnostics"] = stageDiagnostics;
//	    WritePipeMessage(writeFd, msg.dump());
//        };
//
//#ifndef CANGJIE_ENABLE_GCOV
//        try {
//#endif
//            isSuccess = compilerInstance->Compile(CompileStage::PARSE);
//            CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::PARSE, readJsonCode);
//#ifndef CANGJIE_ENABLE_GCOV
//        } catch (...) {
//            CJLintCompilerInvocation::GetInstance().DesInitRuntime();
//            RemoveExtensionFileAndDir(compilerInstance->macroPath);
//            close(writeFd);
//            _exit(1);
//        }
//#endif
//        sendStageResult(CompileStage::PARSE);
//
//#ifndef CANGJIE_ENABLE_GCOV
//        try {
//#endif
//            isSuccess = isSuccess && compilerInstance->Compile(CompileStage::SEMA);
//            CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::SEMA, readJsonCode);
//#ifndef CANGJIE_ENABLE_GCOV
//        } catch (...) {
//            CJLintCompilerInvocation::GetInstance().DesInitRuntime();
//            RemoveExtensionFileAndDir(compilerInstance->macroPath);
//            close(writeFd);
//            _exit(1);
//        }
//#endif
//        sendStageResult(CompileStage::SEMA);
//
//#ifndef CANGJIE_ENABLE_GCOV
//        try {
//#endif
//            isSuccess = isSuccess && compilerInstance->Compile(CompileStage::CHIR);
//            if (compilerInstance->diag.GetErrorCount() == 0) {
//                CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::CHIR, readJsonCode);
//            }
//#ifndef CANGJIE_ENABLE_GCOV
//        } catch (...) {
//            CJLintCompilerInvocation::GetInstance().DesInitRuntime();
//            RemoveExtensionFileAndDir(compilerInstance->macroPath);
//            close(writeFd);
//            _exit(1);
//        }
//#endif
//        sendStageResult(CompileStage::CHIR);
//
//        CJLintCompilerInvocation::GetInstance().DesInitRuntime();
//        RemoveExtensionFileAndDir(compilerInstance->macroPath);
//        close(writeFd);
//        _exit(0);
//    }
//
//    close(pipeFd[1]);
//    int readFd = pipeFd[0];
//
//    int readJsonCode = OK;
//    unsigned latestErrorCount = 0;
//    unsigned latestWarningCount = 0;
//    int stagesReceived = 0;
//    std::vector<std::string> stageName = {"PARSE", "SEMA", "CHIR"};
//    constexpr int TOTAL_STAGES = 3;
//    for (int i = 0; i < TOTAL_STAGES; ++i) {
//        std::string stageData;
//        if (!ReadPipeMessage(readFd, stageData)) {
//            Errorln("Process isolation: child process crashed during ", stageName[i],
//                " stage, recovered diagnostics from ", stagesReceived, " completed stage(s)");
//            break;
//        }
//
//        auto msg = nlohmann::ordered_json::parse(stageData);
//        readJsonCode = msg["readJsonCode"].get<int>();
//        latestErrorCount = msg["errorCount"].get<unsigned>();
//        latestWarningCount = msg["warningCount"].get<unsigned>();
//        diagEngine->ImportSerializedDiagnostics(msg["diagnostics"]);
//        ++stagesReceived;
//    }
//
//    close(readFd);
//
//    int status = 0;
//    waitpid(pid, &status, 0);
//
//    if (WIFSIGNALED(status)) {
//        Errorln("Process isolation: child process terminated by signal ", WTERMSIG(status));
//    }
//
//    //CodeCheckDiagnosticEngine::AddToErrorCount(latestErrorCount);
//    //CodeCheckDiagnosticEngine::AddToWarningCount(latestWarningCount);
//
//    return readJsonCode;
//}

#else // _WIN32

// Convert ANSI (local codepage) string to UTF-8 for JSON serialization
static std::string AnsiToUtf8(const std::string& ansi)
{
    if (ansi.empty()) {
        return ansi;
    }
    // First convert ANSI to wide string
    int wideLen = MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, NULL, 0);
    if (wideLen <= 0) {
        return ansi;  // Fallback to original
    }
    std::wstring wide(wideLen, 0);
    MultiByteToWideChar(CP_ACP, 0, ansi.c_str(), -1, &wide[0], wideLen);

    // Then convert wide string to UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) {
        return ansi;  // Fallback to original
    }
    std::string utf8(utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Len, NULL, NULL);
    // Remove trailing null
    if (!utf8.empty() && utf8.back() == '\0') {
        utf8.pop_back();
    }
    return utf8;
}

// Convert UTF-8 string back to ANSI (local codepage) after JSON deserialization
static std::string Utf8ToAnsi(const std::string& utf8)
{
    if (utf8.empty()) {
        return utf8;
    }
    // First convert UTF-8 to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
    if (wideLen <= 0) {
        return utf8;  // Fallback to original
    }
    std::wstring wide(wideLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wide[0], wideLen);

    // Then convert wide string to ANSI
    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (ansiLen <= 0) {
        return utf8;  // Fallback to original
    }
    std::string ansi(ansiLen, 0);
    WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &ansi[0], ansiLen, NULL, NULL);
    // Remove trailing null
    if (!ansi.empty() && ansi.back() == '\0') {
        ansi.pop_back();
    }
    return ansi;
}

static bool WritePipeMessage(HANDLE hPipe, const std::string& data)
{
    uint32_t len = static_cast<uint32_t>(data.size());
    DWORD bytesWritten = 0;
    if (!WriteFile(hPipe, &len, sizeof(len), &bytesWritten, NULL) ||
        bytesWritten != sizeof(len)) {
        return false;
    }
    size_t totalWritten = 0;
    while (totalWritten < data.size()) {
        DWORD n = 0;
        if (!WriteFile(hPipe, data.c_str() + totalWritten,
                       static_cast<DWORD>(data.size() - totalWritten), &n, NULL) || n == 0) {
            return false;
        }
        totalWritten += n;
    }
    return true;
}

static bool ReadPipeMessage(HANDLE hPipe, std::string& data)
{
    uint32_t len = 0;
    DWORD bytesRead = 0;
    if (!ReadFile(hPipe, &len, sizeof(len), &bytesRead, NULL) ||
        bytesRead != sizeof(len) || len == 0) {
        return false;
    }
    data.resize(len);
    size_t totalRead = 0;
    while (totalRead < len) {
        DWORD n = 0;
        if (!ReadFile(hPipe, &data[totalRead], static_cast<DWORD>(len - totalRead), &n, NULL) || n == 0) {
            return false;
        }
        totalRead += n;
    }
    return true;
}

static const char* GetStageName(CompileStage stage)
{
    switch (stage) {
        case CompileStage::PARSE:
            return "PARSE";
        case CompileStage::SEMA:
            return "SEMA";
        case CompileStage::CHIR:
            return "CHIR";
        default:
            return "UNKNOWN";
    }
}

// Windows worker process entry point - called from main.cpp when --cjlint-worker is passed
// Takes two pipe handles: configReadPipe (to read config from parent) and resultWritePipe (to send results)
int Checker::WorkerMain(void* configReadHandle, void* resultWriteHandle)
{
    HANDLE configReadPipe = static_cast<HANDLE>(configReadHandle);
    HANDLE resultWritePipe = static_cast<HANDLE>(resultWriteHandle);

    // First, read configuration JSON from parent process
    std::string configData;
    if (!ReadPipeMessage(configReadPipe, configData)) {
        CloseHandle(configReadPipe);
        CloseHandle(resultWritePipe);
        return 1;
    }
    CloseHandle(configReadPipe);

    // Parse and apply configuration
    auto configJson = nlohmann::ordered_json::parse(configData, nullptr, false);
    if (configJson.is_discarded()) {
        CloseHandle(resultWritePipe);
        return 1;
    }

    ConfigContext& configContext = ConfigContext::GetInstance();

    // Apply all configuration from parent process (convert UTF-8 back to ANSI)
    if (configJson.contains("cangjieHome")) {
        configContext.SetCangjieHome(Utf8ToAnsi(configJson["cangjieHome"].get<std::string>()));
    }
    if (configJson.contains("srcFileDirs")) {
        for (const auto& dir : configJson["srcFileDirs"]) {
            configContext.AddSrcFileDir(Utf8ToAnsi(dir.get<std::string>()));
        }
    }
    if (configJson.contains("srcFileList")) {
        std::vector<std::string> srcFiles;
        for (const auto& file : configJson["srcFileList"]) {
            srcFiles.push_back(Utf8ToAnsi(file.get<std::string>()));
        }
        configContext.SetSrcFileList(srcFiles);
    }
    if (configJson.contains("modulesDir")) {
        configContext.SetModulesDir(Utf8ToAnsi(configJson["modulesDir"].get<std::string>()));
    }
    if (configJson.contains("configFileDir")) {
        configContext.SetConfigFileDir(Utf8ToAnsi(configJson["configFileDir"].get<std::string>()));
    }
    if (configJson.contains("cjoPath")) {
        configContext.SetCjoPath(Utf8ToAnsi(configJson["cjoPath"].get<std::string>()));
    }
    if (configJson.contains("reportFile")) {
        configContext.SetReportFile(Utf8ToAnsi(configJson["reportFile"].get<std::string>()));
    }
    if (configJson.contains("reportFormat")) {
        configContext.SetReportFormat(Utf8ToAnsi(configJson["reportFormat"].get<std::string>()));
    }

    // Now proceed with compilation using the configuration from parent
    SourceManager sm;
    CodeCheckDiagnosticEngine diagEngine;
    diagEngine.SetSourceManager(&sm);

    DiagnosticEngine diag;
#ifdef NDEBUG
    diag.SetIsEmitter(false);
    diag.SetIsDumpErrCnt(false);
#endif
    Json jsonInfo;
    int readJsonCode = OK;
    bool isSuccess = true;

    if (CommonFunc::ReadJsonFileToJsonInfo(CJLINT_RULE_LIST, ConfigContext::GetInstance(), jsonInfo) == ERR) {
        CJLintCompilerInvocation::GetInstance().DesInitRuntime();
        CloseHandle(resultWritePipe);
        return 1;
    }

    auto compilerInstance = CJLintCompilerInvocation::GetInstance().PrePareCompilerInstance(diag);
    diagEngine.SetSourceManager(&compilerInstance->GetSourceManager());

    // Create a temporary Checker just for CreateRuleThreads
    Checker checker(&diagEngine);

    //size_t prevDiagCount = 0;
    std::unordered_set<std::string> sentDiagnostics;
    auto sendStageResult = [&](CompileStage stage) {
        nlohmann::ordered_json msg;
        msg["stage"] = GetStageName(stage);
        msg["readJsonCode"] = readJsonCode;
        msg["errorCount"] = CodeCheckDiagnosticEngine::GetStaticErrorCount();
        msg["warningCount"] = CodeCheckDiagnosticEngine::GetStaticWarningCount();
        //msg["diagnostics"] = diagEngine.SerializeDiagnosticsFrom(prevDiagCount);
        //prevDiagCount = diagEngine.GetDiagnosticCount();
        nlohmann::ordered_json allDiagnostics = diagEngine.SerializeDiagnostics();
        nlohmann::ordered_json stageDiagnostics = nlohmann::ordered_json::array();
        for (const auto& diagItem : allDiagnostics) {
            auto serialized = diagItem.dump();
            if (sentDiagnostics.insert(serialized).second) {
                stageDiagnostics.push_back(diagItem);
            }
        }
        msg["diagnostics"] = stageDiagnostics;
	WritePipeMessage(resultWritePipe, msg.dump());
    };

    // --- PARSE stage ---
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        isSuccess = compilerInstance->Compile(CompileStage::PARSE);
        checker.CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::PARSE, readJsonCode);
#ifndef CANGJIE_ENABLE_GCOV
    } catch (const std::out_of_range& e) {
        Errorln("Out of range exception caught: ", e.what());
    } catch (const std::exception& e) {
        Errorln("Exception caught: ", e.what());
    } catch (...) {
        Errorln("Unknown exception caught.");
    }
#endif
    sendStageResult(CompileStage::PARSE);

    // --- SEMA stage ---
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        isSuccess = isSuccess && compilerInstance->Compile(CompileStage::SEMA);
        checker.CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::SEMA, readJsonCode);
#ifndef CANGJIE_ENABLE_GCOV
    } catch (const std::out_of_range& e) {
        Errorln("Out of range exception caught: ", e.what());
    } catch (const std::exception& e) {
        Errorln("Exception caught: ", e.what());
    } catch (...) {
        Errorln("Unknown exception caught.");
    }
#endif
    sendStageResult(CompileStage::SEMA);

    // --- CHIR stage ---
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        isSuccess = isSuccess && compilerInstance->Compile(CompileStage::CHIR);
        checker.CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::CHIR, readJsonCode);
#ifndef CANGJIE_ENABLE_GCOV
    } catch (const std::out_of_range& e) {
        Errorln("Out of range exception caught: ", e.what());
    } catch (const std::exception& e) {
        Errorln("Exception caught: ", e.what());
    } catch (...) {
        Errorln("Unknown exception caught.");
    }
#endif
    sendStageResult(CompileStage::CHIR);

    CJLintCompilerInvocation::GetInstance().DesInitRuntime();
    RemoveExtensionFileAndDir(compilerInstance->macroPath);
    CloseHandle(resultWritePipe);
    _exit(0);  // Must use _exit to avoid diagEngine destructor outputting diagnostics again
    return 0;
}

int Checker::CheckCode()
{
    HANDLE hConfigReadPipe = NULL;
    HANDLE hConfigWritePipe = NULL;
    HANDLE hResultReadPipe = NULL;
    HANDLE hResultWritePipe = NULL;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&hConfigReadPipe, &hConfigWritePipe, &sa, 0) ||
        !CreatePipe(&hResultReadPipe, &hResultWritePipe, &sa, 0)) {
        if (hConfigReadPipe) CloseHandle(hConfigReadPipe);
        if (hConfigWritePipe) CloseHandle(hConfigWritePipe);
        if (hResultReadPipe) CloseHandle(hResultReadPipe);
        if (hResultWritePipe) CloseHandle(hResultWritePipe);
        return ERR;
    }

    SetHandleInformation(hConfigWritePipe, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hResultReadPipe, HANDLE_FLAG_INHERIT, 0);

    char exePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
        CloseHandle(hConfigReadPipe);
        CloseHandle(hConfigWritePipe);
        CloseHandle(hResultReadPipe);
        CloseHandle(hResultWritePipe);
        return ERR;
    }

    std::string cmdLine = std::string("\"") + exePath + "\" --cjlint-worker " +
                          std::to_string(reinterpret_cast<uintptr_t>(hConfigReadPipe)) + " " +
                          std::to_string(reinterpret_cast<uintptr_t>(hResultWritePipe));

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hConfigReadPipe);
        CloseHandle(hConfigWritePipe);
        CloseHandle(hResultReadPipe);
        CloseHandle(hResultWritePipe);
        return ERR;
    }

    // Parent: close child-side handles
    CloseHandle(hConfigReadPipe);
    CloseHandle(hResultWritePipe);

    // Send complete configuration to worker via JSON
    ConfigContext& configContext = ConfigContext::GetInstance();
    nlohmann::ordered_json configJson;

    // Serialize all ConfigContext values (convert ANSI to UTF-8 for JSON)
    nlohmann::json srcFileDirsUtf8 = nlohmann::json::array();
    for (const auto& dir : configContext.GetSrcFileDir()) {
        srcFileDirsUtf8.push_back(AnsiToUtf8(dir));
    }
    configJson["srcFileDirs"] = srcFileDirsUtf8;

    nlohmann::json srcFileListUtf8 = nlohmann::json::array();
    for (const auto& file : configContext.GetSrcFileList()) {
        srcFileListUtf8.push_back(AnsiToUtf8(file));
    }
    configJson["srcFileList"] = srcFileListUtf8;

    configJson["modulesDir"] = AnsiToUtf8(configContext.GetModulesDir());
    configJson["configFileDir"] = AnsiToUtf8(configContext.GetConfigFileDir());
    configJson["cangjieHome"] = AnsiToUtf8(configContext.GetCangjieHome());
    configJson["cjoPath"] = AnsiToUtf8(configContext.GetCjoPath());
    configJson["reportFile"] = AnsiToUtf8(configContext.GetReportFile());
    configJson["reportFormat"] = AnsiToUtf8(configContext.GetReportFormat());

    WritePipeMessage(hConfigWritePipe, configJson.dump());
    CloseHandle(hConfigWritePipe);

    // Read results from worker
    int readJsonCode = OK;
    int stagesCompleted = 0;
    static const char* STAGE_NAMES[] = {"PARSE", "SEMA", "CHIR"};
    constexpr int TOTAL_STAGES = 3;

    for (int i = 0; i < TOTAL_STAGES; ++i) {
        std::string stageData;
        if (!ReadPipeMessage(hResultReadPipe, stageData)) {
            break;
        }
        auto msg = nlohmann::ordered_json::parse(stageData, nullptr, false);
        if (!msg.is_discarded()) {
            if (msg.contains("readJsonCode")) {
                readJsonCode = msg["readJsonCode"].get<int>();
            }
            if (msg.contains("diagnostics")) {
                diagEngine->ImportSerializedDiagnostics(msg["diagnostics"]);
            }
            if (msg.contains("errorCount")) {
                diagEngine->AddToErrorCount(msg["errorCount"].get<int>());
            }
            if (msg.contains("warningCount")) {
                diagEngine->AddToWarningCount(msg["warningCount"].get<int>());
            }
        }
        ++stagesCompleted;
    }

    CloseHandle(hResultReadPipe);

    // Wait for child process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (stagesCompleted < TOTAL_STAGES) {
        Errorln("Process isolation: worker process crashed during ",
                STAGE_NAMES[stagesCompleted], " stage, recovered diagnostics from ",
                stagesCompleted, " completed stage(s)");
    }

    return readJsonCode;
}
#endif // _WIN32
