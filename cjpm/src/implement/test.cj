// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.collection.concurrent.*
import std.sync.*
import std.fs.*
import stdx.encoding.json.*
import stdx.serialization.serialization.*
import cjpm.toml.*
import cjpm.config.*

var dependencyMap: HashMap<String, HashSet<String>> = HashMap<String, HashSet<String>>()
@When[os == "Windows"]
const TEST_GCNO_FILE: String = "$test"
@When[os != "Windows"]
const TEST_GCNO_FILE: String = "\\$test"

class PkgTestInfo {
    public var fullName: String = ""
    public var path: String = ""
    public var linkStr: String = ""
    public var outLogFile: String = ""
    public var errLogFile: String = ""

    init(fullName: String, path: String, linkStr: String) {
        this.fullName = fullName
        this.path = path
        this.linkStr = linkStr
    }
}

// Command test implement.
public func doTest(unparseArgs: Array<String>, testConfig: TestConfig): Bool {
    if (testConfig.skipBuild && testConfig.noRun) {
        eprintln("Warning: cjpm doesn't do ${testConfig.origCommand} in current mode")
        return true
    }

    let (pathSet, checkFlag) = testArgsCheck(unparseArgs, testConfig)
    if (!checkFlag) {
        return false
    }
    if (!testConfig.isModuleTest && !testConfig.testModule.isEmpty()) {
        eprintln("Error: single package unittest option conflicts with module test by '--module'")
        return false
    }
    for (path in pathSet) {
        testConfig.testPath.add(getCanonicalPath(path))
    }

    let tomlInfo: TomlInfo = loadModuleFile(DIR_CURRENT) ?? return false

    if (!replaceEnvforTargetDir(tomlInfo, Path(DIR_CURRENT).join(CONFIG_FILE_NAME).toString())) {
        return false
    }
    var configFileDir: String
    if (tomlInfo.isPkgConfig) {
        configFileDir = tomlInfo.pkg.targetDir
    } else {
        configFileDir = tomlInfo.workspace.targetDir
    }
    testConfig.globalConfig.targetDir = getOriginTargetDirectory(testConfig.globalConfig.targetDir, DIR_CURRENT,
        testConfig.targetDir, configFileDir, false) ?? return false
    testConfig.originTargetPath = testConfig.globalConfig.targetDir

    var targetName: String = targetConfigName
    if (testConfig.isCrossCompile) {
        targetName = crossCompileTarget
    }
    if (!tomlInfo.replaceTargetConfig(DIR_CURRENT, targetName, testConfig.isDebug, true)) {
        return false
    }

    var scriptConfig: ScriptConfig = ScriptConfig()
    if (!testConfig.globalConfig.isSkipScript) {
        scriptConfig = makeScriptConfigByToml(testConfig.globalConfig.rootPath, testConfig.globalConfig.rootPath,
            tomlInfo, testConfig.isDebug, testConfig.isVerbose, testConfig.isCrossCompile) ?? return false
        if (!runScript(scriptConfig, BUILD_SCRIPT_STEP_PRE, testConfig.origCommand)) {
            return false
        }
    }

    if (testConfig.isBench) {
        testConfig.profile = tomlInfo.profile.bench
    } else {
        testConfig.profile = tomlInfo.profile.test
    }
    if (!verifyConfigForTest(tomlInfo, testConfig)) {
        return false
    }

    if (!checkForWorkspace(DIR_CURRENT, tomlInfo, testConfig.memberModule, isDebug: testConfig.isDebug, isTest: true)) {
        return false
    }
    if (COMMON_INFO.inWorkspace && !testConfig.isModuleTest && !testConfig.memberModule.isEmpty()) {
        eprintln("Error: single package unittest option conflicts with -m/--member option")
        return false
    }

    if (!verifyEnvConfig(testConfig.profile.envConfig, testConfig.origCommand,
        Path(DIR_CURRENT).join(CONFIG_FILE_NAME).toString())) {
        return false
    }
    testConfig.envConfig = testConfig.profile.envConfig

    testConfig.globalConfig.isVerbose = testConfig.isVerbose
    testConfig.globalConfig.isDebug = testConfig.isDebug
    testConfig.globalConfig.isTest = true
    testConfig.globalConfig.isCross = testConfig.isCrossCompile
    testConfig.globalConfig.resolveType = RESOLVE_TEST
    let (dm, flag, mockUsed) = resolve(testConfig.globalConfig)
    if (!flag) {
        return false
    }
    testConfig.packageDepMap = dm.packageDepMap

    if (testConfig.testModule.isEmpty() && dm.globalConfig.rootName != WORKSPACE_NAME) {
        testConfig.testModule = HashSet<String>(dm.globalConfig.rootName)
    } else if (!testConfig.testModule.isEmpty()) {
        for (testModuleName in testConfig.testModule) {
            if (!dm.depPathMap.contains(testModuleName)) {
                eprintln("Error: cannot find module '${testModuleName}' of option '--module'")
                return false
            }
        }
    }
    testConfig.globalConfig.scriptList = dm.globalConfig.scriptList

    testConfig.reportFormat = testConfig.reportFormat ?? testConfig.profile.reportFormat
    testConfig.reportPath = testConfig.reportPath ?? testConfig.profile.reportPath

    testConfig.isProgressReportDisabled = match (testConfig.isProgressReportDisabled) {
        case true => true
        case false => testConfig.profile.isProgressReportDisabled
    }

    testConfig.isProgressReportBrief = match (testConfig.isProgressReportBrief) {
        case true => true
        case false => testConfig.profile.isProgressReportBrief
    }

    testConfig.progressReportEntriesLimit = match (testConfig.progressReportEntriesLimit) {
        case Some(limit) => limit
        case None => testConfig.profile.progressReportEntriesLimit
    }

    if (let Some(limit) <- testConfig.progressReportEntriesLimit && limit < 0) {
        eprintln("Error: illegal format of option '--progress-entries-limit'. " +
            "Expected positive integer or 0 to disable the limit (default), but got: '${limit}'")
        return false
    }

    testConfig.mock = match (testConfig.profile.mock) {
        case Some(mock) => mock // explicit mock compilation option is preferred
        case None where testConfig.isBench => MockSupport.RuntimeError
        case None where mockUsed => MockSupport.On
        case None => MockSupport.Off
    }

    if (!execPreTestArgs(testConfig)) {
        return false
    }

    return startTest(testConfig, scriptConfig)
}

func startTest(testConfig: TestConfig, scriptConfig: ScriptConfig): Bool {
    let historyFile = Path(testConfig.originTargetPath).join(HISTORY_FILE_NAME).toString()
    if (!fileExists(historyFile)) {
        eprintln("Error: load file '${historyFile}' failed")
        if (testConfig.skipBuild) {
            eprintln("Error: failed to show unittest result, please generate it with '--no-run' option first")
        }
        return false
    }
    let resolve: ModuleResolve = if (let Some(res) <- loadHistoryFile(historyFile)) {
        res
    } else {
        return false
    }

    (testConfig.globalConfig.targetDir, testConfig.globalConfig.nativeDir) = setFinalTargetDirectory(
        testConfig.globalConfig, testConfig.isCrossCompile, testConfig.isDebug, testConfig.mock == MockSupport.On)
    let (ldPath, cLibLinkMap, cjPathList) = configureModuleResolveData(
        testConfig.globalConfig.targetDir, testConfig.globalConfig.nativeDir, resolve)
    testConfig.globalConfig.cLibLinkMap = cLibLinkMap.clone()
    testConfig.globalConfig.ldPath.add(all: ldPath)
    testConfig.globalConfig.cjPathList.add(all: cjPathList)

    let (testPkgsMap, linkMap) = prepareTest(resolve, testConfig)
    testConfig.ldPath = setLdLibraryPath(testConfig.globalConfig.ldPath)

    if (!parallelTest(testConfig, testPkgsMap, linkMap)) {
        return false
    }

    if (testConfig.globalConfig.isSkipScript) {
        return true
    }

    if (!runMultiScriptPost(testConfig.globalConfig)) {
        return false
    }

    return runScript(scriptConfig, BUILD_SCRIPT_STEP_POST, testConfig.origCommand)
}

func testArgsCheck(unparseArgs: Array<String>, testConfig: TestConfig): (HashSet<String>, Bool) {
    var checkFlag: Bool = true
    var pathSet = HashSet<String>()

    if (unparseArgs.size == 0) {
        return (pathSet, checkFlag)
    }

    for (val in unparseArgs) {
        if (directoryExists(val)) {
            testConfig.isModuleTest = false
            pathSet.add(val)
        } else if (val.startsWith("--")) {
            let option = val[2..]
            if (!customizedOptionCheck(option, "")) {
                checkFlag = false
            } else {
                testConfig.customizedOption.add(option)
            }
        } else if (val.startsWith("-")) {
            eprintln("Error: unknow command '${val}' for cjpm ${testConfig.origCommand}")
            checkFlag = false
        } else {
            eprintln("Error: the path '${val}' does not exist")
            checkFlag = false
        }
    }

    return (pathSet, checkFlag)
}

func execPreTestArgs(testConfig: TestConfig): Bool {
    if (testConfig.skipBuild) {
        return true
    }

    let buildConfig = BuildConfig()
    buildConfig.isIncremental = true
    buildConfig.isDebug = testConfig.isDebug
    buildConfig.memberModule = testConfig.memberModule
    buildConfig.isCrossCompile = testConfig.isCrossCompile
    buildConfig.isVerbose = testConfig.isVerbose
    buildConfig.customizedOption = testConfig.customizedOption
    buildConfig.targetDir = testConfig.targetDir
    buildConfig.globalConfig = GlobalConfig(testConfig.globalConfig)
    buildConfig.globalConfig.targetDir = TARGET
    buildConfig.globalConfig.resolveType = RESOLVE_BUILD
    buildConfig.requiredForTests = true

    if (testConfig.mock == MockSupport.On) {
        buildConfig.mockSupported = true
    }

    let targetDirectoryCache: String = testConfig.globalConfig.targetDir
    let commonInfoCache: CommonInfo = COMMON_INFO
    COMMON_INFO = CommonInfo()
    if (!doBuild(buildConfig)) {
        eprintln("Error: please execute '${generateBuildCmd(testConfig)}' successfully first")
        return false
    }
    testConfig.rebuildList = buildConfig.rebuildList
    testConfig.testPkgs = HashSet<String>(buildConfig.testPkgs)
    testConfig.globalConfig.targetDir = targetDirectoryCache
    testConfig.globalConfig.nativeDir = buildConfig.globalConfig.nativeDir
    testConfig.globalConfig.overrideCompileOption = buildConfig.globalConfig.overrideCompileOption

    COMMON_INFO = commonInfoCache
    return true
}

func generateBuildCmd(testConfig: TestConfig): String {
    var cmdList = ArrayList<String>()
    cmdList.add("${CJPM_TOOL} build -i")
    if (testConfig.isDebug) {
        cmdList.add("-g")
    }
    if (!testConfig.memberModule.isEmpty()) {
        cmdList.add("--member=\"${testConfig.memberModule}\"")
    }
    if (testConfig.isSpecifiedConcurrency) {
        cmdList.add("-j${maxParallelSize}")
    }
    if (testConfig.isCrossCompile) {
        cmdList.add("--target=${crossCompileTarget}")
    }
    if (testConfig.mock == MockSupport.On) {
        cmdList.add("--mock")
    }
    if (testConfig.customizedOption.size != 0) {
        for (option in testConfig.customizedOption) {
            cmdList.add("--${option}")
        }
    }
    if (!testConfig.targetDir.isEmpty()) {
        cmdList.add("--target-dir=${testConfig.targetDir}")
    }
    return String.join(cmdList.toArray(), delimiter: " ")
}

// Get all the information in preparation for unittest.
func prepareTest(res: ModuleResolve, testConfig: TestConfig): (HashMap<String, PackageDescription>, HashMap<String,
    String>) {
    // Record the packages that need to be tested, the format is ["pkgPath": PackageDescription].
    var testPkgsMap = HashMap<String, PackageDescription>()
    // Record all dependency.
    dependencyMap = getDependencyMap(res)
    // Record c libs.
    var cLibPathSet = HashSet<String>()

    // Recod test module names for workspace
    var testModuleNames: HashSet<String> = HashSet<String>(testConfig.testModule)
    if (COMMON_INFO.inWorkspace && testConfig.isModuleTest && testModuleNames.isEmpty()) {
        testModuleNames = appendModulesForWorkspace(res)
    }

    for (r in res.resolves) {
        for ((_, cLibPath) in r.cLibs) {
            cLibPathSet.add(cLibPath)
        }
        if (r.isMacroPackage) {
            continue
        }
        // package will be added into testPkgsMap if one of following conditions is satisfied:
        // 1. in single-package test mode, and package is contained in input package list;
        // 2. in module test mode, and module of the package is in module list or in test-member of workspace
        let testCheck: Bool = (testConfig.isModuleTest &&
            ((COMMON_INFO.inWorkspace && testModuleNames.contains(r.rootPkgName)) ||
            testConfig.testModule.contains(r.rootPkgName))) ||
            (!testConfig.isModuleTest && testConfig.testPath.contains(r.packagePath))
        if (testCheck) {
            testPkgsMap[r.packagePath] = PackageDescription(r.fullName, r.hasTestFiles, r.hasProdFiles,
                r.hasTestsInProdFiles)
        }
    }

    return (testPkgsMap, getTestLinkMap(testConfig, res, testPkgsMap.values(), dependencyMap))
}

func getTestLinkMap(testConfig: TestConfig, res: ModuleResolve, testPkgsMap: Collection<PackageDescription>,
    dependencyMap: HashMap<String, HashSet<String>>): HashMap<String, String> {
    let linkMap = HashMap<String, String>()
    for (pkgDesc in testPkgsMap) {
        let requires = if (dependencyMap.contains(pkgDesc.fullName)) {
            dependencyMap[pkgDesc.fullName]
        } else {
            HashSet<String>()
        }
        linkMap[pkgDesc.fullName] = getLinkString(res, pkgDesc.fullName, requires, testConfig)
    }
    return linkMap
}

func appendModulesForWorkspace(res: ModuleResolve): HashSet<String> {
    let testModuleNames = HashSet<String>()
    if (!COMMON_INFO.memberModule.isEmpty()) { // for the single module
        for (r in res.resolves) {
            if (COMMON_INFO.memberModule == getCanonicalPath(r.modulePath)) {
                testModuleNames.add(r.rootPkgName)
                break
            }
        }
    } else { // for all workspace modules
        for (r in res.resolves) {
            if (testModuleNames.contains(r.rootPkgName)) {
                continue
            }
            if (COMMON_INFO.testMembers.contains(getCanonicalPath(r.modulePath))) {
                testModuleNames.add(r.rootPkgName)
            }
        }
    }
    return testModuleNames
}

// Generate the required static library files in parallel, then execute the test command.
func parallelTest(testConfig: TestConfig, testPkgsMap: HashMap<String, PackageDescription>,
    linkMap: HashMap<String, String>): Bool {
    if (!testConfig.isModuleTest) {
        for (pkgPath in testConfig.testPath) {
            if (!testPkgsMap.contains(pkgPath)) {
                eprintln("Warning: '${pkgPath}' is an invalid package path, it will be ignored")
            }
        }
    }

    if (!compileAllTests(linkMap, testPkgsMap, testConfig)) {
        return false
    }

    if (!execAllTests(testPkgsMap, testConfig)) {
        return false
    }

    return saveTestIncremental(testConfig, testPkgsMap)
}

func getLinkString(res: ModuleResolve, packageName: String, requires: HashSet<String>,
    testConfig: TestConfig): String {
    let list: ArrayList<String> = getLinkList(res, packageName, requires, testConfig)
    return String.join(list.toArray(), delimiter: " ")
}

func appendLinkOption(option: String): String {
    if (!option.isEmpty()) {
        return "--link-options=\"${option}\""
    }
    return ""
}

func getLinkList(res: ModuleResolve, fullName: String, requires: HashSet<String>,
    testConfig: TestConfig): ArrayList<String> {
    var linkSort = ArrayList<String>()
    var cLibNameList = HashSet<String>()

    let allOptions = ArrayList<String>()
    for (r in res.resolves) {
        if (!requires.contains(r.fullName) && r.fullName != fullName) {
            continue
        }
        if (r.isMacroPackage) {
            continue
        }
        cLibNameList.add(r.rootPkgName)
        if (r.fullName == fullName) {
            let testCondition = getTestConditionOption(testConfig.customizedOption, r.customizedOption)
            if (!testCondition.isEmpty()) {
                allOptions.add(testCondition)
            }
            if (!r.compileOption.isEmpty()) {
                allOptions.add(r.compileOption)
            }
            if (!testConfig.globalConfig.overrideCompileOption.isEmpty()) {
                allOptions.add(testConfig.globalConfig.overrideCompileOption)
            }
            let linkOption: String = appendLinkOption(r.linkOption)
            if (!linkOption.isEmpty()) {
                allOptions.add(linkOption)
            }
            allOptions.add(testConfig.profile.compileOption)
            break
        }
        if (r.isPureTestPkg()) {
            continue
        }
        let libPath = Path(testConfig.globalConfig.targetDir).join(r.rootPkgName).toString()
        allOptions.add(all: linkPackage(r.fullName, libPath, testConfig.isLto), at: 0)
    }
    linkSort.add(all: allOptions)

    linkSort.add(all: appendBinOptionForTest(res, requires, testConfig.isLto))

    for (k in cLibNameList) {
        if (testConfig.globalConfig.cLibLinkMap.contains(k)) {
            linkSort.add(all: testConfig.globalConfig.cLibLinkMap[k])
        }
    }

    let result = ArrayList<String>()
    var index: Int64 = 0
    var libPathSet = HashSet<String>()
    while (index < linkSort.size) {
        if (linkSort[index] == "-L") {
            if (!libPathSet.contains(linkSort[index + 1])) {
                result.add(linkSort[index])
                result.add(linkSort[index + 1])
                libPathSet.add(linkSort[index + 1])
            }
            index += 2
        } else {
            result.add(linkSort[index])
            index += 1
        }
    }

    return result
}

func getTestConditionOption(cmdOption: HashSet<String>, conditionMap: HashMap<String, String>): String {
    var res = ""
    for (k in cmdOption) {
        if (conditionMap.contains(k)) {
            res = conditionMap[k] + " " + res
        }
    }
    return res
}

// Dependency analysis
func getDependencyMap(res: ModuleResolve): HashMap<String, HashSet<String>> {
    let requiresMap = HashMap<String, HashSet<String>>()
    for (r in res.resolves) {
        requiresMap[r.fullName] = r.requires
    }
    for ((_, deps) in res.binDeps) {
        for ((fullName, info) in deps.packageRequires) {
            if (requiresMap.contains(fullName)) {
                continue
            }
            requiresMap[fullName] = info.requires
        }
    }
    for ((_, deps) in res.crossBinDeps) {
        for ((fullName, info) in deps.packageRequires) {
            if (requiresMap.contains(fullName)) {
                continue
            }
            requiresMap[fullName] = info.requires
        }
    }

    var dependencyMap = HashMap<String, HashSet<String>>()
    for (r in res.resolves) {
        if (r.requires.size == 0) {
            continue
        }
        dependencyMap[r.fullName] = getDepSet(r.fullName, requiresMap)
    }
    return dependencyMap
}

public func getOutput(targetDir: String, fullName: String, isCross: Bool): String {
    let outputDir = Path(targetDir).join(UNITTEST_BIN).toString()
    var output: String = Path(outputDir).join(fullName).toString()
    output = getUnittestOutput(output, isCross)
    return output
}

func getRootPackageDir(pkgFullName: String, targetDir: String) {
    return targetDir.replace(UNITTEST_BIN, RELEASE) + PATH_SPLIT_CHAR + pkgFullName.split(".")[0]
}

func addOptionsForSeparateCompilation(pkgInfo: PkgTestInfo, testConfig: TestConfig) {
    let rootProdPkgDir = getRootPackageDir(pkgInfo.fullName, testConfig.globalConfig.targetDir)
    if (testConfig.isLto) {
        pkgInfo.linkStr = " ${rootProdPkgDir}${PATH_SPLIT_CHAR}${pkgInfo.fullName}.bc ${pkgInfo.linkStr}"
    } else {
        pkgInfo.linkStr = " -L ${rootProdPkgDir} -l${pkgInfo.fullName} ${pkgInfo.linkStr}"
    }
}

func packageTest(pkgInfo: PkgTestInfo, testConfig: TestConfig, hasProdFiles: Bool): Bool {
    let output = getOutput(testConfig.globalConfig.targetDir, pkgInfo.fullName, testConfig.isCrossCompile)

    var cmdList = ArrayList<String>()
    if (!testConfig.ldPath.isEmpty()) {
        cmdList.add(testConfig.ldPath)
    }

    // add output-type arg
    cmdList.add(COMPILE_TOOL)

    for (p in testConfig.globalConfig.cjPathList) {
        if (directoryExists(p)) {
            cmdList.add("--import-path \"${p}\"")
        }
    }
    if (hasProdFiles) {
        addOptionsForSeparateCompilation(pkgInfo, testConfig)
        cmdList.add("--test-only")
    } else {
        cmdList.add("--test")
    }
    cmdList.add(pkgInfo.linkStr)
    cmdList.add("-p \"${pkgInfo.path}\" -o \"${output}\"")
    cmdList.add("--mock=${testConfig.mock.toString()}")
    if (testConfig.isCrossCompile) {
        cmdList.add("--target=${crossCompileTarget}")
    }
    if (testConfig.isDebug) {
        cmdList.add("-g")
    }
    if (testConfig.isLto) {
        cmdList.add("--lto=${testConfig.ltoValue}")
    }
    cmdList.add("--no-sub-pkg")

    var cmdStr = String.join(cmdList.toArray(), delimiter: " ")

    if (!deleteFile(pkgInfo.outLogFile)) {
        return false
    }

    if (testConfig.isVerbose || testConfig.profile.verbose) {
        let verbose: String = "Compiling test package `${pkgInfo.fullName}`: ${cmdStr}\n"
        if (!creatAndWriteFile(pkgInfo.outLogFile, verbose)) {
            return false
        }
    }

    return exec("${cmdStr} 1>>\"${pkgInfo.outLogFile}\" 2>\"${pkgInfo.errLogFile}\"")
}

// It is too hard to do proper json escaping for serialization and then command line,
// so Unittest prefers to inherit json configuration from test runner.
func mkUnittestArgs(testConfig: TestConfig, includeJson!: Bool): Array<String> {
    let args = ArrayList<String>()
    if (testConfig.isBench) {
        args.add("--bench")
    }
    if (testConfig.dryRun) {
        args.add("--dry-run")
    }
    if (let Some(filters) <- testConfig.filters) {
        args.add("--filter=${filters}")
    }
    if (let Some(tags) <- testConfig.includeTags) {
        args.add("--include-tags=${tags}")
    }
    if (let Some(tags) <- testConfig.excludeTags) {
        args.add("--exclude-tags=${tags}")
    }
    if (testConfig.noColor) {
        args.add("--no-color")
    }
    if (testConfig.noCaptureOutput) {
        args.add("--no-capture-output")
    }
    if (testConfig.showAllOutput) {
        args.add("--show-all-output")
    }
    if (let Some(timeout) <- testConfig.timeoutEach) {
        args.add("--timeout-each=${timeout}")
    }
    if (let Some(nWorkers) <- testConfig.nWorkers) {
        args.add("--parallel=${nWorkers}")
    }
    if (let Some(randomSeed) <- testConfig.randomSeed) {
        args.add("--random-seed=${randomSeed}")
    }
    if (let Some(baselinePath) <- testConfig.baselinePath) {
        args.add("--baseline-path=${baselinePath}")
    }
    // to generate report, at least one option should be specified explicitly
    // for benchmarks always generate report so that it can compare with previous run
    if (testConfig.reportFormat.isSome() || testConfig.reportPath.isSome() || testConfig.isBench) {
        if (let Some(reportFormat) <- testConfig.reportFormat) {
            args.add("--report-format=${reportFormat}")
        }

        let reportPath = testConfig.reportPath ?? Path(testConfig.globalConfig.targetDir).join("reports").toString()
        args.add("--report-path=${reportPath}")
    }
    if (includeJson) {
        let jsonTestConfiguration = testConfig.profile.toStringWithoutCompilationOptions()
        if (!jsonTestConfiguration.isEmpty()) {
            args.add("--json-configuration=\"${jsonTestConfiguration}\"")
        }
    }
    if (testConfig.isProgressReportDisabled) {
        args.add("--no-progress")
    }
    if (testConfig.isProgressReportBrief) {
        args.add("--progress-brief")
    }
    if (let Some(limit) <- testConfig.progressReportEntriesLimit) {
        args.add("--progress-entries-limit=${limit}")
    }

    args.toArray()
}

func mkExecTestRunnerCommand(testConfig: TestConfig, pkgFullName: String, runnerArgs: Array<String>): String {
    let output = getOutput(getCanonicalPath(testConfig.globalConfig.targetDir), pkgFullName, testConfig.isCrossCompile)

    var cmdList = ArrayList<String>()
    let envSetup = appendEnvConfig(testConfig.envConfig)
    cmdList.add(all: envSetup)
    cmdList.add(all: addExtraArgsForTest(testConfig))
    cmdList.add(output)
    cmdList.add(all: mkUnittestArgs(testConfig, includeJson: true))
    cmdList.add(all: runnerArgs)

    var cmdStr = String.join(cmdList.toArray(), delimiter: " ")
    if (!envSetup.isEmpty()) {
        cmdStr += ENV_PATCH
    }
    if (testConfig.isVerbose || testConfig.profile.verbose) {
        println(cmdStr)
    }
    cmdStr
}

func mkExecTestPackageCommand(testConfig: TestConfig, pkgFullName: String): TestPackageExecuteCommand {
    TestPackageExecuteCommand(
        getOutput(getCanonicalPath(testConfig.globalConfig.targetDir), pkgFullName, testConfig.isCrossCompile),
        mkUnittestArgs(testConfig, includeJson: false), HashMap<String, String>()
    )
}

func addExtraArgsForTest(testConfig: TestConfig): ArrayList<String> {
    var args = ArrayList<String>()
    if (!testConfig.ldPath.isEmpty()) {
        args.add("${testConfig.ldPath}")
    }
    return args
}

func compileAllTests(linkMap: HashMap<String, String>, testPkgsMap: HashMap<String, PackageDescription>,
    testConfig: TestConfig): Bool {
    if (testConfig.skipBuild || testPkgsMap.isEmpty()) {
        if (testPkgsMap.isEmpty()) { // if no test package to compile, then generate dummy runner
            if (!checkTestDir(testConfig.globalConfig.targetDir, testPkgsMap.values())) {
                return false
            }
            TestRunner.generateSources(testConfig)
            TestRunner.compile(testConfig)
        }
        return true
    }

    if (!checkTestDir(testConfig.globalConfig.targetDir, testPkgsMap.values())) {
        return false
    }

    if (testConfig.isIncremental) {
        loadTestIncremental(testConfig)
        let finalTestPkgsMap = HashMap<String, PackageDescription>()
        for ((pkgPath, pkgDesc) in testPkgsMap) {
            if (testConfig.rebuildList.contains(pkgDesc.fullName) || rebuildTestPkgs(testConfig, pkgDesc.fullName) ||
                rebuildPureTestPkgs(testConfig, pkgPath, pkgDesc.fullName)) {
                finalTestPkgsMap.add(pkgPath, pkgDesc)
            }
        }
        return parallelCompileTests(linkMap, finalTestPkgsMap, testConfig)
    }

    return parallelCompileTests(linkMap, testPkgsMap, testConfig)
}

// Check packages: rebuild if cjo file is changed
func rebuildTestPkgs(testConfig: TestConfig, pkgName: String): Bool {
    try {
        let binPath: Path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN)
        let cjoFilePath = binPath.join("${pkgName}.cjo")
        let testCjoFilePath = binPath.join("${pkgName}$test.cjo")
        if (fileExists(cjoFilePath)) {
            return max(FileInfo(cjoFilePath).lastModificationTime.toUnixTimeStamp().toSeconds().toString(),
                FileInfo(binPath.join(getUnittestOutput(pkgName, testConfig.isCrossCompile)))
                    .lastModificationTime.toUnixTimeStamp().toSeconds().toString()) !=
                    testConfig.incrementalCache[pkgName]["binTimeStamp"]
        } else if (fileExists(testCjoFilePath)) {
            return max(FileInfo(testCjoFilePath).lastModificationTime.toUnixTimeStamp().toSeconds().toString(),
                FileInfo(binPath.join(getUnittestOutput(pkgName, testConfig.isCrossCompile)))
                    .lastModificationTime.toUnixTimeStamp().toSeconds().toString()) !=
                    testConfig.incrementalCache[pkgName]["binTimeStamp"]
        } else {
            return true
        }
    } catch (e: Exception) {
        return true
    }
}

// Check pure test packages: rebuild if source files are changed or depended packages are changed
func rebuildPureTestPkgs(testConfig: TestConfig, pkgPath: String, pkgName: String): Bool {
    if (!testConfig.testPkgs.contains(pkgName)) {
        return false
    }

    try {
        if (genFileTimeStamp(pkgPath) != testConfig.incrementalCache[pkgName]["fileTimeStamp"]) {
            return true
        }
    } catch (e: Exception) {
        return true
    }

    for (require in testConfig.packageDepMap[pkgName].requires) {
        if (testConfig.rebuildList.contains(require)) {
            return true
        }
    }

    return false
}

// Load test incremental cache
func loadTestIncremental(testConfig: TestConfig): Unit {
    let testCachePath: Path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN).join("incremental-cache.json")
    if (!fileExists(testCachePath)) {
        return
    }

    // load json file
    // format: pkgName -> (fileTimeStamp, binTimeStamp)
    var buf: String = ""
    try {
        buf = String.fromUtf8(File.readFrom(testCachePath))
        testConfig.incrementalCache = HashMap<String, HashMap<String, String>>.deserialize(
            DataModel.fromJson(JsonValue.fromStr(buf)))
    } catch (e: Exception) {
        return
    }

    return
}

// Save test incremental cache
func saveTestIncremental(testConfig: TestConfig, testPkgsMap: HashMap<String, PackageDescription>): Bool {
    let testBinPath: Path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN)
    testConfig.incrementalCache.add(".testrunner",
        HashMap<String, String>([
            ("binTimeStamp", max(
                FileInfo(testBinPath.join("${TestRunner.PKG_FULL_NAME}.cjo"))
                    .lastModificationTime.toUnixTimeStamp().toSeconds(),
                FileInfo(testBinPath.join(makeExeName(TestRunner.PKG_FULL_NAME)))
                    .lastModificationTime.toUnixTimeStamp().toSeconds()).toString()),
            ("fileTimeStamp", FileInfo(testBinPath.join("testrunner.cj"))
                .lastModificationTime.toUnixTimeStamp().toSeconds().toString())]))
    for ((pkgPath, pkgDesc) in testPkgsMap) {
        let pkgCache = HashMap<String, String>()
        let cjoFilePath = testBinPath.join("${pkgDesc.fullName}.cjo")
        let testCjoFilePath = testBinPath.join("${pkgDesc.fullName}$test.cjo")
        if (fileExists(cjoFilePath)) {
            pkgCache.add("binTimeStamp",
                max(FileInfo(cjoFilePath).lastModificationTime.toUnixTimeStamp().toSeconds(),
                    FileInfo(testBinPath.join(getUnittestOutput(pkgDesc.fullName, testConfig.isCrossCompile)))
                        .lastModificationTime.toUnixTimeStamp().toSeconds()).toString())
        } else if (fileExists(testCjoFilePath)) {
            pkgCache.add("binTimeStamp",
                max(FileInfo(testCjoFilePath).lastModificationTime.toUnixTimeStamp().toSeconds(),
                    FileInfo(testBinPath.join(getUnittestOutput(pkgDesc.fullName, testConfig.isCrossCompile)))
                        .lastModificationTime.toUnixTimeStamp().toSeconds()).toString())
        } else {
            pkgCache.add("binTimeStamp", "")
        }
        // collect file time stamp
        pkgCache.add("fileTimeStamp", genFileTimeStamp(pkgPath))
        testConfig.incrementalCache.add(pkgDesc.fullName, pkgCache)
    }
    let cacheStr = testConfig.incrementalCache.serialize().toJson().toJsonString()
    return creatAndWriteFile(testBinPath.join("incremental-cache.json").toString(), cacheStr)
}

// Collect file timestamp for pure test packages
func genFileTimeStamp(pkgPath: String): String {
    var timeStamp: Int64 = Int64.Min
    for (fileInfo in getFileList(pkgPath)) {
        let fileName = fileInfo.path.toString()
        if (fileName.endsWith("_test.cj")) {
            timeStamp = max(timeStamp, fileInfo.lastModificationTime.toUnixTimeStamp().toSeconds())
        }
    }
    return timeStamp.toString()
}

// Create a unittest directory to store middle files.
func checkTestDir(targetDir: String, testPkgsMap: Collection<PackageDescription>): Bool {
    let testBinPath: String = Path(targetDir).join(UNITTEST_BIN).toString()
    if (!createDirectory(testBinPath)) {
        return false
    }

    let testLogPath: String = Path(targetDir).join(".test-logs").toString()
    if (!createDirectory(testLogPath)) {
        return false
    }

    for (pkgDesc in testPkgsMap) {
        let rootPkgName: String = getRootPkgName(pkgDesc.fullName)
        let logModPath: String = Path(testLogPath).join(rootPkgName).toString()
        if (!createDirectory(logModPath)) {
            return false
        }
    }

    return true
}

func packageTestIfNeeded(pkgInfo: PkgTestInfo, testConfig: TestConfig, pkgDesc: PackageDescription): Bool {
    return if (pkgDesc.hasTestFiles) {
        packageTest(pkgInfo, testConfig, pkgDesc.hasProdFiles)
    } else {
        true
    }
}

func parallelCompileTests(
    linkMap: HashMap<String, String>,
    testPkgsMap: HashMap<String, PackageDescription>,
    testConfig: TestConfig
): Bool {
    let queue = LinkedBlockingQueue<(String, String)>()
    let logFuture = spawn {
        =>
        var count: Int64 = testPkgsMap.size
        var logFlag: Bool = true
        while (count > 0) {
            let (outLogFile, errLogFile) = queue.remove()
            count--
            if (fileExists(outLogFile) && !loadLogAndPrint(outLogFile, false)) {
                logFlag = false
            }
            if (fileExists(errLogFile) && !loadLogAndPrint(errLogFile, true)) {
                logFlag = false
            }
        }
        return logFlag
    }

    var testFlag: Bool = true
    let testLogPath: Path = Path(testConfig.globalConfig.targetDir).join(".test-logs")
    let sem = Semaphore(maxParallelSize)
    let testFut = ArrayList<Future<Bool>>()
    for ((pkgPath, pkgDesc) in testPkgsMap) {
        sem.acquire()
        if (pkgDesc.hasTestsInProdFiles && !pkgDesc.hasTestFiles) {
            // If no test files, then generate dummy test file to further run prod package's tests
            DummyTestFile.generateSources(testConfig.globalConfig.targetDir, pkgDesc.fullName)
            DummyTestFile.compile(testConfig, pkgDesc.fullName, linkMap)
        }
        testFut.add(
            spawn {
                =>
                let pkgInfo = PkgTestInfo(pkgDesc.fullName, pkgPath, linkMap[pkgDesc.fullName])
                let rootPkgName: String = getRootPkgName(pkgDesc.fullName)
                pkgInfo.outLogFile = testLogPath.join(rootPkgName).join("${pkgDesc.fullName}.outlog").toString()
                pkgInfo.errLogFile = testLogPath.join(rootPkgName).join("${pkgDesc.fullName}.errlog").toString()
                let flag = packageTestIfNeeded(pkgInfo, testConfig, pkgDesc)
                sem.release()
                queue.add((pkgInfo.outLogFile, pkgInfo.errLogFile))
                return flag
            })
    }

    // Compile test runner
    sem.acquire()
    testFut.add(
        spawn {
            =>
            TestRunner.generateSources(testConfig)
            let flag = TestRunner.compile(testConfig)
            sem.release()
            flag
        })

    for (f in testFut) {
        try {
            let res = f.get() // Wait for the thread to complete
            if (!res) {
                testFlag = false
            }
        } catch (e: Exception) {
            testFlag = false
        }
    }

    if (!logFuture.get()) {
        testFlag = false
    }
    return testFlag
}

class DummyTestFile {
    static func generateSources(targetDir: String, packageName: String): Unit {
        let path = Path(targetDir).join(UNITTEST_BIN).join("._${packageName}_test.cj")
        File.writeTo(path, "package ${packageName}".toArray())
    }

    static func compile(testConfig: TestConfig, packageName: String, linkMap: HashMap<String, String>): Bool {
        let cmdList = ArrayList<String>()
        if (!testConfig.ldPath.isEmpty()) {
            cmdList.add("${testConfig.ldPath}")
        }
        cmdList.add(COMPILE_TOOL)
        cmdList.add(
            Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN).join("._${packageName}_test.cj").toString())
        cmdList.add("-o ${getOutput(testConfig.globalConfig.targetDir, packageName, testConfig.isCrossCompile)}")

        let pkgInfo = PkgTestInfo(packageName, "", if (linkMap.contains(packageName)) {
            linkMap[packageName]
        } else {
            ""
        })
        addOptionsForSeparateCompilation(pkgInfo, testConfig)
        cmdList.add("--test-only")
        cmdList.add(pkgInfo.linkStr)
        cmdList.add("--import-path=\"${testConfig.globalConfig.targetDir}\"")
        for (p in testConfig.globalConfig.cjPathList) {
            if (directoryExists(p)) {
                cmdList.add("--import-path \"${p}\"")
            }
        }
        if (testConfig.isCrossCompile) {
            cmdList.add("--target=${crossCompileTarget}")
        }
        let cmd = String.join(cmdList.toArray(), delimiter: " ")
        if (testConfig.isVerbose || testConfig.profile.verbose) {
            println("Compiling dummy test file: ${cmd}")
        }
        exec(cmd)
    }
}

class TestRunner {
    static let PKG_FULL_NAME: String = "std.testrunner"
    static let RUNNER_SOURCE = """
package std.testrunner // Hack to make testRunnerEntryMain not public
import std.unittest.testRunnerEntryMain
main() { testRunnerEntryMain() }"""

    static func generateSources(testConfig: TestConfig): Unit {
        let path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN).join("testrunner.cj")
        if (testConfig.isIncremental && fileExists(path)) {
            return
        }
        File.writeTo(path, TestRunner.RUNNER_SOURCE.toArray())
    }

    static func compile(testConfig: TestConfig): Bool {
        if (testConfig.isIncremental && isIncremental(testConfig)) {
            return true
        }
        let cmdList = ArrayList<String>()
        if (!testConfig.ldPath.isEmpty()) {
            cmdList.add("${testConfig.ldPath}")
        }
        cmdList.add(COMPILE_TOOL)
        cmdList.add(Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN).join("testrunner.cj").toString())
        cmdList.add("-o ${getOutput(testConfig.globalConfig.targetDir, TestRunner.PKG_FULL_NAME, false)}")
        let cmd = String.join(cmdList.toArray(), delimiter: " ")
        if (testConfig.isVerbose || testConfig.profile.verbose) {
            println("Compiling test runner: ${cmd}")
        }
        exec(cmd)
    }

    static func exec(testConfig: TestConfig, tests: ExecutableTestProject): Bool {
        let path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN)
        if (!directoryExists(path.toString())) {
            Directory.create(path, recursive: true)
        }
        let configFile = File.createTemp(path)
        try {
            let json = tests.toJson()
            configFile.write(json.toArray())
            let command = mkExecTestRunnerCommand(testConfig, TestRunner.PKG_FULL_NAME,
                ["--internal-testrunner-input-path=${configFile.info.path}"])
            if (testConfig.isVerbose || testConfig.profile.verbose) {
                println("Executing test runner: ${command}; with input: ${json}")
            }
            exec(command)
        } finally {
            configFile.close()
            remove(configFile.info.path)
        }
    }

    private static func isIncremental(testConfig: TestConfig): Bool {
        try {
            let binPath: Path = Path(testConfig.globalConfig.targetDir).join(UNITTEST_BIN)
            return (
                FileInfo(binPath.join("testrunner.cj"))
                    .lastModificationTime.toUnixTimeStamp().toSeconds().toString() ==
                    testConfig.incrementalCache[".testrunner"]["fileTimeStamp"]) &&
                (max(FileInfo(binPath.join("${TestRunner.PKG_FULL_NAME}.cjo"))
                    .lastModificationTime.toUnixTimeStamp().toSeconds(),
                FileInfo(binPath.join(makeExeName(TestRunner.PKG_FULL_NAME)))
                    .lastModificationTime.toUnixTimeStamp().toSeconds()).toString() ==
                testConfig.incrementalCache[".testrunner"]["binTimeStamp"])
        } catch (e: Exception) {
            return false
        }
    }
}

func execAllTests(testPkgsMap: HashMap<String, PackageDescription>, testConfig: TestConfig): Bool {
    if (testConfig.noRun || testConfig.isCrossCompile) {
        return true
    }

    var execTestFlag = true

    let (flag, tests) = makeExecutableTests(testConfig, testPkgsMap.values())
    execTestFlag &&= flag
    if (!TestRunner.exec(testConfig, tests)) {
        execTestFlag = false
    }

    return deleteDirectory(BUILD_GCNO_OUTPUT) && deleteDirectory(TEST_GCNO_OUTPUT) && execTestFlag
}

func makeExecutableTests(testConfig: TestConfig, pkgFullNames: Collection<PackageDescription>): (Bool,
    ExecutableTestProject) {
    let moduleNameToPackages = HashMap<String, ArrayList<ExecutableTestPackage>>()
    var successFlag = true
    for (pkgDesc in pkgFullNames) {
        if (!pkgDesc.hasTestFiles && !pkgDesc.hasTestsInProdFiles) {
            continue
        }
        if (!validateOutputAndReport(testConfig.globalConfig.targetDir, pkgDesc.fullName, testConfig.isCrossCompile)) {
            successFlag = false
            continue
        }
        let rootPkgName = getRootPkgName(pkgDesc.fullName)
        let executeCommand = mkExecTestPackageCommand(testConfig, pkgDesc.fullName)
        let packageInfo = ExecutableTestPackage(pkgDesc.fullName, executeCommand)
        if (let Some(packages) <- moduleNameToPackages.get(rootPkgName)) {
            packages.add(packageInfo)
        } else {
            moduleNameToPackages.add(rootPkgName, ArrayList([packageInfo]))
        }
    }
    let modules = ArrayList<ExecutableTestModule>()
    for ((rootPkgName, packages) in moduleNameToPackages) {
        modules.add(ExecutableTestModule(rootPkgName, packages.toArray()))
    }
    (successFlag, ExecutableTestProject(modules.toArray()))
}

func validateOutputAndReport(targetDir: String, pkgFullName: String, isCross: Bool): Bool {
    let output = getOutput(targetDir, pkgFullName, isCross)
    if (!fileExists(output)) {
        eprintln("Error: failed to show unittest result, " +
            "please generate the '${output}' file with '--no-run' option first")
        false
    } else {
        true
    }
}

public func getUnittestOutput(output: String, isCross: Bool): String {
    if (isCross) {
        return makeTargetExeName(output)
    }
    return makeExeName(output)
}

func verifyConfigForTest(cfg: TomlInfo, testConfig: TestConfig): Bool {
    let configFilePath: String = Path(DIR_CURRENT).join(CONFIG_FILE_NAME).toString()
    if (!cfg.checkJsonEnvVars()) {
        return false
    }
    cfg.replaceJsonEnvVars()

    if (!safeCheck(cfg.pkg.targetDir, configFilePath)) {
        return false
    }

    if (!ltoValueCheck(testConfig.profile.ltoValue, configFilePath)) {
        return false
    }

    if (!testConfig.profile.ltoValue.isEmpty()) {
        testConfig.isLto = supportLto()
        testConfig.ltoValue = testConfig.profile.ltoValue
    }

    if (!verifyTargetConfig(cfg.targetConfigMap, DIR_CURRENT, configFilePath, isTest: true)) {
        return false
    }

    return combineDevDependencies(cfg)
}

public func combineDevDependencies(tomlInfo: TomlInfo): Bool {
    for ((devDepName, devDep) in tomlInfo.testDependencies) {
        if (tomlInfo.dependencies.contains(devDepName)) {
            var dep = tomlInfo.dependencies[devDepName]
            let (isAlternative, errInfo) = dep.isAlternative(devDep)
            if (!errInfo.isEmpty()) {
                eprintln("Error: modules with name '${devDepName}' in dependency tree are conflicted, " +
                    "which is detected from test-dependencies. " +
                    "Please check test-dependencies config in ${Path(DIR_CURRENT).join(CONFIG_FILE_NAME).toString()}")
                eprintln(errInfo)
                return false
            }
            if (!isAlternative) {
                continue
            }
        }
        tomlInfo.dependencies.add(devDepName, devDep)
    }
    return true
}

// Serialization should remain compatible std.unittest.
struct ExecutableTestProject <: Serializable<ExecutableTestProject> {
    ExecutableTestProject(let testModules: Array<ExecutableTestModule>) {}
    static let API_VERSION = 2

    public func serialize(): DataModel {
        DataModelStruct()
            .add(field<Int64>("apiVersion", API_VERSION))
            .add(field<Array<ExecutableTestModule>>("testModules", testModules))
    }

    public static func deserialize(_: DataModel): ExecutableTestProject {
        throw Exception("Implemented in std.unittest")
    }

    func toJson(): String {
        this.serialize().toJson().toString()
    }
}

// Serialization should remain compatible std.unittest.
struct ExecutableTestModule <: Serializable<ExecutableTestModule> {
    ExecutableTestModule(let name: String, let testPackages: Array<ExecutableTestPackage>) {}

    public func serialize(): DataModel {
        DataModelStruct()
            .add(field<String>("name", name))
            .add(field<Array<ExecutableTestPackage>>("testPackages", testPackages))
    }

    public static func deserialize(_: DataModel): ExecutableTestModule {
        throw Exception("Implemented in std.unittest")
    }
}

// Serialization should remain compatible std.unittest.
struct ExecutableTestPackage <: Serializable<ExecutableTestPackage> {
    ExecutableTestPackage(let name: String, let executeCommand: TestPackageExecuteCommand) {}

    public func serialize(): DataModel {
        DataModelStruct()
            .add(field<String>("name", name))
            .add(field<TestPackageExecuteCommand>("executeCommand", executeCommand))
    }

    public static func deserialize(_: DataModel): ExecutableTestPackage {
        throw Exception("Implemented in std.unittest")
    }
}

// Serialization should remain compatible std.unittest.
struct TestPackageExecuteCommand <: Serializable<TestPackageExecuteCommand> {
    TestPackageExecuteCommand(
        public let command: String,
        public let args: Array<String>,
        public let env: Map<String, String>
    ) {}

    public func serialize(): DataModel {
        DataModelStruct()
            .add(field<String>("command", command))
            .add(field<Array<String>>("args", args))
            .add(field<HashMap<String, String>>("env", HashMap<String, String>(env)))
    }

    public static func deserialize(_: DataModel): TestPackageExecuteCommand {
        throw Exception("Implemented in std.unittest")
    }
}

struct PackageDescription {
    PackageDescription(
        let fullName: String,
        let hasTestFiles: Bool,
        let hasProdFiles: Bool,
        let hasTestsInProdFiles: Bool
    ) {}
}
