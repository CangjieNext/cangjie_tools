
## 1. 优化详情
###  **1.1 优化背景**
cjlint是通过调用cjc实例的Compile方法分别在PARSE、SEMA和CHIR三个阶段编译源代码，并在每个阶段完成后运行对应的规则检查线程。
```C++
#ifndef CANGJIE_ENABLE_GCOV
    try {
#endif
        isSuccess = compilerInstance->Compile(CompileStage::PARSE);
        CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::PARSE, readJsonCode);
        isSuccess = isSuccess && compilerInstance->Compile(CompileStage::SEMA);
        CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::SEMA, readJsonCode);
        isSuccess = isSuccess && compilerInstance->Compile(CompileStage::CHIR);
        if (compilerInstance->diag.GetErrorCount() == 0) {
            CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::CHIR, readJsonCode);
        }
#ifndef CANGJIE_ENABLE_GCOV
    } catch (const std::out_of_range& e) {
        Errorln("Out of range exception caught: ", e.what());
    } catch (const std::exception& e) {
        Errorln("Exception caught: ", e.what());
    } catch (...) {
        Errorln("Unknown exception caught.");
    }
#endif
```
但是由于这三个编译阶段和规则检查都在同一进程内按顺序执行，如果cjc或任意一条规则在某个阶段发生崩溃，整个进程将会立即终止，并且已完成阶段的检查结果全部丢失，用户看不到任何输出（SEMA发生崩溃，PARSE阶段的检查结果不会输出；CHIR发生崩溃，SEMA和PARSE阶段的检查结果都不会输出）。
###  **1.2 优化功能**
因此为了解决上述问题，可以通过进程隔离机制进行优化，使每个阶段完成后立即将检查结果传递给监控父进程保存，这样即使后续阶段发生崩溃，已完成阶段的结果仍然可以通过监控父进程输出给用户。
## 2. Linux和Mac平台优化方案
###  **2.1 创建进程和管道**
cjlint执行代码检查的核心函数是Checker.cpp中的CheckCode()函数，因此需要在函数内部实现进程隔离。实现进程隔离的方法有很多种，Linux和Mac系统可以直接fork一个子进程作为工作进程，父进程作为监控进程，父子进程间通过管道进行通信。

```C++
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
    int writeFd = pipeFd[1];
    ...
}
```
###  **2.2 检查结果的序列化和反序列化**
每个阶段的执行结果是通过Checker构造函数传入的CodeCheckDiagnostic类型参数diagEngine获取的，但是管道不能直接传递diagEngine对象，所以需要把它的信息序列化为JSON再通过管道发送给父进程，父进程获取到JSON信息后再反序列化为diagEngine对象。
在DiagnosticEngine.h中实现的序列化函数：
```C++
nlohmann::ordered_json Serialize() const {
        nlohmann::ordered_json j;
        j["startFileID"] = start.fileID;
        j["startLine"] = start.line;
        j["startColumn"] = start.column;
        j["endFileID"] = end.fileID;
        j["endLine"] = end.line;
        j["endColumn"] = end.column;
        j["kind"] = static_cast<int>(kind);
        j["path"] = path;
        j["diagMessage"] = diagMessage;
        j["diagCategory"] = static_cast<int>(diagCategory);
        j["clearanceRequirement"] = static_cast<int>(clearanceRequirement);
        return j;
}
```
在DiagnosticEngine.h中实现的反序列化函数：
```C++
static CodeCheckDiagnostic Deserialize(const nlohmann::ordered_json& j) {
    Position s(static_cast<unsigned>(j["startFileID"].get<int>()),
        static_cast<unsigned>(j["startLine"].get<int>()),
        static_cast<unsigned>(j["startColumn"].get<int>()));
    Position e(static_cast<unsigned>(j["endFileID"].get<int>()),
        static_cast<unsigned>(j["endLine"].get<int>()),
        static_cast<unsigned>(j["endColumn"].get<int>()));
    CodeCheckDiagKind k = static_cast<CodeCheckDiagKind>(j["kind"].get<int>());
    CodeCheckDiagnostic d(s, e, k, j["path"].get<std::string>());
    d.diagMessage = j["diagMessage"].get<std::string>();
    d.diagCategory = static_cast<CodeCheckDiagCategory>(j["diagCategory"].get<int>());
    d.clearanceRequirement = static_cast<CodeCheckClearanceRequirement>(j["clearanceRequirement"].get<int>());
        return d;
}
```
###  **2.3 工作子进程的实现**
在子进程中将每个阶段的检查结果通过管道发送给父进程的是Lambda表达式sendStageResult。
```C++
auto sendStageResult = [&](CompileStage stage) {
	nlohmann::ordered_json msg;
	msg["stage"] = GetStageName(stage);
	msg["readJsonCode"] = readJsonCode;
	msg["errorCount"] = CodeCheckDiagnosticEngine::GetStaticErrorCount();
	msg["warningCount"] = CodeCheckDiagnosticEngine::GetStaticWarningCount();
	msg["diagnostics"] = diagEngine->SerializeDiagnosticsFrom(prevDiagCount);
	prevDiagCount = diagEngine->GetDiagnosticCount();
	WritePipeMessage(writeFd, msg.dump());
};
```
在PARSE、SEMA和CHIR阶段检查后都分别加上sendStageResult函数，这样可以把每个阶段的检查结果立即传递给父进程,当某个阶段发生崩溃时，不会影响已完成阶段的检查结果输出。
```C++
#ifndef CANGJIE_ENABLE_GCOV
        try {
#endif
	        isSuccess = compilerInstance->Compile(CompileStage::PARSE);
            CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::PARSE, readJsonCode);
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
#ifndef CANGJIE_ENABLE_GCOV
        try {
#endif                
            isSuccess = isSuccess && compilerInstance->Compile(CompileStage::SEMA);
            CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::SEMA, readJsonCode);
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
#ifndef CANGJIE_ENABLE_GCOV
        try {
#endif
            isSuccess = isSuccess && compilerInstance->Compile(CompileStage::CHIR);
            if (compilerInstance->diag.GetErrorCount() == 0) {
                CreateRuleThreads(jsonInfo, compilerInstance, CompileStage::CHIR, readJsonCode);
            }
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
```
###  **2.4 监控父进程获取检查结果**
在父进程中需要读取子进程往管道中发送的JSON信息，并将它导入到diagEngine，无论是正常情况还是某个阶段发生崩溃，在diagEngine析构时都会正常输出已经获取到的检查结果。
```C++
for (int i = 0; i < 3; ++i) {
	std::string stageData;
	if (!ReadPipeMessage(readFd, stageData)) {
		Errorln("Process isolation: child process crashed during ", stageName[i],
		" stage, recovered diagnostics from ", stagesReceived, " completed stage(s)");
		break;
	}
	auto msg = nlohmann::ordered_json::parse(stageData);
	readJsonCode = msg["readJsonCode"].get<int>();
	diagEngine->ImportSerializedDiagnostics(msg["diagnostics"]);
	++stagesReceived;
}
```
在PARSE、SEMA和CHIR阶段检查后都分别加上sendStageResult函数，这样可以把每个阶段的检查结果立即传递给父进程。
## 3. Windows平台优化方案
###  3.1 创建管道和进程
Windows没有fork()的系统调用，需要通过CreateProcess()创建新的进程，Windows上创建的进程并不会继承父进程的内存状态，需要父进程把各种配置信息通过管道传递给子进程。因此需要创建两个管道，父进程需要写入配置信息，子进程读取；子进程需要写入检查结果，父进程读取。
首先在父进程中需要声明四个管道句柄，分别是配置信息的读句柄和写句柄，检查结果的读句柄和写句柄。SECURITY_ATTRIBUTES类型是安全属性结构体，对它的参数进行配置，让句柄可以继承。因为子进程只需要配置信息读句柄和检查结果写句柄，所以调用SetHandleInformation函数禁止配置信息写句柄和检查结果读句柄被子进程继承。
```C++
HANDLE hConfigReadPipe = NULL, hConfigWritePipe = NULL;
HANDLE hResultReadPipe = NULL, hResultWritePipe = NULL;
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
```
接着创建子进程，父进程需要知道自己的可执行文件路径，才能通过调用CreateProcessA启动同一个可执行文件作为子进程。
首先通过调用GetModuleFileNameA获取当前父进程的可执行文件路径，并用exePath来接收这个路径。接着需要构建启动可执行文件的命令行cmdLine，它由从父进程获取的可执行文件路径exePath,启动工作子进程的标识--cjlint-worker，配置信息读句柄和检查结果写句柄四个部分组成。初始化进程启动信息STARTUPINFOA和返回的进程信息PROCESS_INFORMATION后，调用CreateProcessA并传入上述设置的各个参数创建子进程。
```C++
char exePath[MAX_PATH];
if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
	CloseHandle(hConfigReadPipe);
	CloseHandle(hConfigWritePipe);
	CloseHandle(hResultReadPipe);
	CloseHandle(hResultWritePipe);
	return ERR;
}
std::string cmdLine = std::string("\"") + exePath + "\" --cjlint-worker " +
					  std::to_string(reinterpret_cast<uintptr_t>(hConfigReadPipe))                       + " " + std::to_string(reinterpret_cast<uintptr_t>(hResultWritePipe));
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
CloseHandle(hConfigReadPipe);
CloseHandle(hResultWritePipe);
```
当父进程调用CreateProcessA启动可执行文件后，就会进入到main函数，如果获取到的命令行参数满足条件，就会从中获取配置文件读句柄和检查结果写句柄，并作为参数传递给执行检查的WorkerMain函数，这样子进程就可以通过句柄获取配置信息和传递检查结果。
```C++
#ifdef _WIN32
    if (argc >= 4 && std::string(argv[1]) == "--cjlint-worker") {
		uintptr_t configReadHandle = std::strtoull(argv[2], nullptr, 10);
        uintptr_t resultWriteHandle = std::strtoull(argv[3], nullptr, 10);
        int result = Checker::WorkerMain(
            reinterpret_cast<void*>(configReadHandle),
            reinterpret_cast<void*>(resultWriteHandle));
        _exit(result);
        return result;
    }
#endif
```
###  3.2 传递配置信息
在创建子进程后，需要从ConfigContext中读取所有检查配置，并将 Windows ANSI 编码的路径字符串转换为 JSON 标准的 UTF-8 编码，序列化为 JSON 字符串后，通过管道发送给子进程。
```C++
ConfigContext& configContext = ConfigContext::GetInstance();
nlohmann::ordered_json configJson;
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
```
###  3.3 检查结果的序列化和反序列化
Windows平台实现检查结果序列化和反序列化的方式与Linux相同。
###  3.4 工作子进程的实现
子进程需要接收父进程通过管道传来的配置数据，解析 JSON 并将 UTF-8 编码的路径转换回 Windows 原生的 ANSI 编码，应用到全局配置上下文，最后初始化代码检查所需的源文件管理器和诊断引擎。
```C++
int Checker::WorkerMain(void* configReadHandle, void* resultWriteHandle){
    HANDLE configReadPipe = static_cast<HANDLE>(configReadHandle);
    HANDLE resultWritePipe = static_cast<HANDLE>(resultWriteHandle);
    std::string configData;
    if (!ReadPipeMessage(configReadPipe, configData)) {
        CloseHandle(configReadPipe);
        CloseHandle(resultWritePipe);
        return ERR;
    }
    CloseHandle(configReadPipe);
    auto configJson = nlohmann::ordered_json::parse(configData, nullptr, false);
    if (configJson.is_discarded()) {
        CloseHandle(resultWritePipe);
        return ERR;
    }
    if (configJson.contains("cangjieHome")) {     configContext.SetCangjieHome(Utf8ToAnsi(configJson["cangjieHome"].get<std::string>()));
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
    if (configJson.contains("modulesDir")) {       configContext.SetModulesDir(Utf8ToAnsi(configJson["modulesDir"].get<std::string>()));
    }
    if (configJson.contains("configFileDir")) {     configContext.SetConfigFileDir(Utf8ToAnsi(configJson["configFileDir"].get<std::string>()));
    }
    if (configJson.contains("cjoPath")) {
	    configContext.SetCjoPath(Utf8ToAnsi(configJson["cjoPath"].get<std::string>()));
    }

    if (configJson.contains("reportFile")) {
configContext.SetReportFile(Utf8ToAnsi(configJson["reportFile"].get<std::string>()));
    }
    if (configJson.contains("reportFormat")) {  configContext.SetReportFormat(Utf8ToAnsi(configJson["reportFormat"].get<std::string>()));
    }
```
后续的实现与Linux相同，都是通过在PARSE、SEMA和CHIR阶段检查后都分别加上sendStageResult函数，把每个阶段的检查结果立即传递给父进程。
###  3.5 监控父进程获取检查结果
Windows上父进程获取检查结果的方式与Linux相同，也是读取JSON信息，并将它导入到diagEngine，无论是正常情况还是某个阶段发生崩溃，在diagEngine析构时都会正常输出已经获取到的检查结果。最后在PARSE、SEMA和CHIR阶段检查后都分别加上sendStageResult函数，这样可以把每个阶段的检查结果立即传递给父进程。
## 4. 优化效果

这是自验证的仓颉测试文件 里面包括编译三个阶段PARSE、SEMA和CHIR对应检查规则的测试代码
```C++
// PARSE stage
// G.DCL.02
struct Test {
    public let a = 2
}
// SEMA stage
// G.NAM.05
struct A{
    static let a_A = 200
}
// CHIR stage
//G.VAR.01
func test() {
    var a = 1
    func test02() {
        var b = 1
        var c = 0
        c = 2
    }
    test02()
    a += 1
}
```
###  4.1.1 Linux优化前
当不使用进程隔离，直接对测试代码进行检查时，会正常输出三个阶段对应规则的检查结果：
![Pasted image 20260205154007.png](https://raw.gitcode.com/user-images/assets/8923410/f54e694b-dd77-4ed4-92ee-5617d0fe2613/Pasted_image_20260205154007.png 'Pasted image 20260205154007.png')
首先需要模拟让不同的检查阶段发生崩溃，使用的方法是给空指针赋值，先在PARSE阶段GDCL02规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGDCL02::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入PARSE阶段的GDCL02规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    CheckReturnValType(node);
}
```
检查结果如下，可以看到程序在GDCL02的执行函数中挂掉了，同时也不会输出其他编译阶段的执行结果。
![Pasted image 20260205155849.png](https://raw.gitcode.com/user-images/assets/8923410/119ed6bf-2704-4907-b122-4b8859bf4347/Pasted_image_20260205155849.png 'Pasted image 20260205155849.png')              
接着只在SEMA阶段的GNAM05规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGNAM05::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入SEMA阶段的GNAME05规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    FindGlobalOrStaticVarWithLetName(node);
}
```
检查结果如下，可以看到程序在GNAM05的执行函数中挂掉了，也不会输出PARSE阶段GDCL02的执行结果。
![Pasted image 20260205154803.png](https://raw.gitcode.com/user-images/assets/8923410/542e5010-48f2-4fab-8b15-d8282b10fa7f/Pasted_image_20260205154803.png 'Pasted image 20260205154803.png')  
  最后只在CHIR阶段的VAR01规则的执行函数中给空指针赋值。
```C++
void DataflowRuleVAR01Check::DoAnalysis(CJLintCompilerInstance* instance)
{
    std::cout << "进入CHIR阶段的VAR01规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    auto packageList = instance->GetSourcePackages();
    for (auto package : packageList) {
        FindAllUnSafeBlock(*package);
    }
    // Traverse and inspect CHIR packages
    analysisWrapper = const_cast<AnalysisWrapper*>(&instance->GetConstAnalysisWrapper());
    for (auto chirPkg : instance->GetAllCHIRPackages()) {
        CheckBasedOnCHIR(*chirPkg);
    }
}
```
检查结果如下，可以看到程序在VAR01的执行函数中挂掉了，也不会输出之前两个阶段PARSE和SEMA对应规则的执行结果。
![Pasted image 20260205152603.png](https://raw.gitcode.com/user-images/assets/8923410/f6e99307-7add-4349-803e-cb87fb1b7198/Pasted_image_20260205152603.png 'Pasted image 20260205152603.png')
###  4.1.2 Linux优化后
使用进程隔离对原来代码进行优化，直接对测试代码进行检查时，同样会正常输出三个阶段对应规则的检查结果：
![Pasted image 20260205163333.png](https://raw.gitcode.com/user-images/assets/8923410/d2a76f24-dabe-4243-9d06-6a75555980e4/Pasted_image_20260205163333.png 'Pasted image 20260205163333.png')
与优化前采取的方式一样，需要模拟让不同的检查阶段发生崩溃，使用的方法是给空指针赋值，先在PARSE阶段GDCL02规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGDCL02::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入PARSE阶段的GDCL02规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    CheckReturnValType(node);
}
```
检查结果如下，可以看到程序在GDCL02的执行函数中挂掉了，同时也不会输出其他编译阶段的执行结果。
![Pasted image 20260205160542.png](https://raw.gitcode.com/user-images/assets/8923410/b44cc43b-b668-4cd6-bf1e-2694295bffa3/Pasted_image_20260205160542.png 'Pasted image 20260205160542.png')
接着只在SEMA阶段的GNAM05规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGNAM05::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入SEMA阶段的GNAME05规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    FindGlobalOrStaticVarWithLetName(node);
}
```
检查结果如下，可以看到程序在GNAM05的执行函数中挂掉了，但是会输出上一阶段PARSE的GDCL02的执行结果。
![Pasted image 20260205162241.png](https://raw.gitcode.com/user-images/assets/8923410/d9092cbc-a812-4171-866b-e92b0da361ba/Pasted_image_20260205162241.png 'Pasted image 20260205162241.png')
最后只在CHIR阶段的VAR01规则的执行函数中给空指针赋值。
```C++
void DataflowRuleVAR01Check::DoAnalysis(CJLintCompilerInstance* instance)
{
    std::cout << "进入CHIR阶段的VAR01规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    auto packageList = instance->GetSourcePackages();
    for (auto package : packageList) {
        FindAllUnSafeBlock(*package);
    }
    // Traverse and inspect CHIR packages
    analysisWrapper = const_cast<AnalysisWrapper*>(&instance->GetConstAnalysisWrapper());
    for (auto chirPkg : instance->GetAllCHIRPackages()) {
        CheckBasedOnCHIR(*chirPkg);
    }
}
```
检查结果如下，可以看到程序在VAR01的执行函数中挂掉了，但是会输出之前两个阶段PARSE和SEMA对应规则的执行结果。
![Pasted image 20260205162821.png](https://raw.gitcode.com/user-images/assets/8923410/ee51a247-165a-4bc3-80e4-bcae87d26d6f/Pasted_image_20260205162821.png 'Pasted image 20260205162821.png')
上述结果说明通过对原来代码进行进程隔离优化可以实现需求，即SEMA发生崩溃，PARSE阶段的检查结果可以正常输出；CHIR发生崩溃，SEMA和PARSE阶段的检查结果都可以正常输出。
###   4.2.1 Windows优化前
当不使用进程隔离，直接对测试代码进行检查时，会正常输出三个阶段对应规则的检查结果：
![Pasted image 20260224144550.png](https://raw.gitcode.com/user-images/assets/8923410/f88078c8-6f2a-4680-acfe-12edce4cc291/Pasted_image_20260224144550.png 'Pasted image 20260224144550.png')
在PARSE阶段GDCL02规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGDCL02::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入PARSE阶段的GDCL02规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    CheckReturnValType(node);
}
```
检查结果如下，可以看到程序在GDCL02的执行函数中挂掉了，同时也不会输出其他编译阶段的执行结果。       
![Pasted image 20260224144634.png](https://raw.gitcode.com/user-images/assets/8923410/984e1764-a984-4339-bfb6-57471f2b9827/Pasted_image_20260224144634.png 'Pasted image 20260224144634.png')                                   
接着只在SEMA阶段的GNAM05规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGNAM05::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入SEMA阶段的GNAME05规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    FindGlobalOrStaticVarWithLetName(node);
}
```
检查结果如下，可以看到程序在GNAM05的执行函数中挂掉了，也不会输出PARSE阶段GDCL02的执行结果。             
![Pasted image 20260224151338.png](https://raw.gitcode.com/user-images/assets/8923410/2ec9a0e1-129b-494c-8414-d660b6d23a93/Pasted_image_20260224151338.png 'Pasted image 20260224151338.png')           
最后只在CHIR阶段的VAR01规则的执行函数中给空指针赋值。
```C++
void DataflowRuleVAR01Check::DoAnalysis(CJLintCompilerInstance* instance)
{
    std::cout << "进入CHIR阶段的VAR01规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    auto packageList = instance->GetSourcePackages();
    for (auto package : packageList) {
        FindAllUnSafeBlock(*package);
    }
    // Traverse and inspect CHIR packages
    analysisWrapper = const_cast<AnalysisWrapper*>(&instance->GetConstAnalysisWrapper());
    for (auto chirPkg : instance->GetAllCHIRPackages()) {
        CheckBasedOnCHIR(*chirPkg);
    }
}
```
检查结果如下，可以看到程序在VAR01的执行函数中挂掉了，也不会输出之前两个阶段PARSE和SEMA对应规则的执行结果。
![Pasted image 20260224153752.png](https://raw.gitcode.com/user-images/assets/8923410/ee6b42e3-1e57-4100-8755-9cd2e86c495f/Pasted_image_20260224153752.png 'Pasted image 20260224153752.png')
###   4.2.2 Windows优化后
使用进程隔离对原来代码进行优化，直接对测试代码进行检查时，同样会正常输出三个阶段对应规则的检查结果：
![Pasted image 20260227113411.png](https://raw.gitcode.com/user-images/assets/8923410/c28280ba-0f47-45d5-a9e6-bf84ad472926/Pasted_image_20260227113411.png 'Pasted image 20260227113411.png')
与优化前采取的方式一样，需要模拟让不同的检查阶段发生崩溃，使用的方法是给空指针赋值，先在PARSE阶段GDCL02规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGDCL02::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入PARSE阶段的GDCL02规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    CheckReturnValType(node);
}
```
检查结果如下，可以看到程序在GDCL02的执行函数中挂掉了，同时也不会输出其他编译阶段的执行结果。
![Pasted image 20260227131110.png](https://raw.gitcode.com/user-images/assets/8923410/c2168dc4-42ef-40eb-adb5-4593c26b83a1/Pasted_image_20260227131110.png 'Pasted image 20260227131110.png')
接着只在SEMA阶段的GNAM05规则的执行函数中给空指针赋值。
```C++
void StructuralRuleGNAM05::MatchPattern(ASTContext& ctx, Ptr<Node> node)
{
    (void)ctx;
    std::cout << "进入SEMA阶段的GNAME05规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    FindGlobalOrStaticVarWithLetName(node);
}
```
检查结果如下，可以看到程序在GNAM05的执行函数中挂掉了，但是会输出上一阶段PARSE的GDCL02的执行结果。
![Pasted image 20260227120819.png](https://raw.gitcode.com/user-images/assets/8923410/34608865-5af6-413f-a22b-2e7eb383ab09/Pasted_image_20260227120819.png 'Pasted image 20260227120819.png')
最后只在CHIR阶段的VAR01规则的执行函数中给空指针赋值。
```C++
void DataflowRuleVAR01Check::DoAnalysis(CJLintCompilerInstance* instance)
{
    std::cout << "进入CHIR阶段的VAR01规则的检查函数" << std::endl;
    int* ptr = nullptr;
    *ptr = 100;
    auto packageList = instance->GetSourcePackages();
    for (auto package : packageList) {
        FindAllUnSafeBlock(*package);
    }
    // Traverse and inspect CHIR packages
    analysisWrapper = const_cast<AnalysisWrapper*>(&instance->GetConstAnalysisWrapper());
    for (auto chirPkg : instance->GetAllCHIRPackages()) {
        CheckBasedOnCHIR(*chirPkg);
    }
}
```
检查结果如下，可以看到程序在VAR01的执行函数中挂掉了，但是会输出之前两个阶段PARSE和SEMA对应规则的执行结果。
![Pasted image 20260227114920.png](https://raw.gitcode.com/user-images/assets/8923410/ec157b00-b6a5-4743-b611-bd70229bd7bc/Pasted_image_20260227114920.png 'Pasted image 20260227114920.png')
上述结果说明通过对原来代码进行进程隔离优化可以实现需求，即SEMA发生崩溃，PARSE阶段的检查结果可以正常输出；CHIR发生崩溃，SEMA和PARSE阶段的检查结果都可以正常输出。