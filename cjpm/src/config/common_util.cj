// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*
import std.fs.*
import std.regex.*
import std.io.*
import std.process.*
import std.runtime.*
import std.env.*
import std.sync.*

foreign func system(command: CString): Int32

// Start a native OS thread to read one subprocess pipe through the callback.
foreign func CJPM_EXEC_StartPipeReader(readerId: Int64, callback: CFunc<(Int64) -> Unit>): CPointer<Unit>

// Wait until the native pipe reader thread exits.
foreign func CJPM_EXEC_JoinPipeReader(reader: CPointer<Unit>): Bool

// Free the native pipe reader handle after it has been joined or failed to start.
foreign func CJPM_EXEC_FreePipeReader(reader: CPointer<Unit>): Unit

// Keep the Cangjie stream and collected result for one native pipe reader thread.
private class PipeReaderContext {
    let stream: InputStream
    var output: String = ""
    var error: String = ""

    init(stream: InputStream) {
        this.stream = stream
    }
}

private let pipeReaderMutex = Mutex()
private var nextPipeReaderId: Int64 = 0
private let pipeReaderContexts = HashMap<Int64, PipeReaderContext>()

// Register one pipe stream and return an id that can be passed through C ABI.
private func addPipeReaderContext(stream: InputStream): Int64 {
    synchronized(pipeReaderMutex) {
        nextPipeReaderId++
        pipeReaderContexts[nextPipeReaderId] = PipeReaderContext(stream)
        nextPipeReaderId
    }
}

// Look up the stream context for the native thread callback.
private func getPipeReaderContext(readerId: Int64): ?PipeReaderContext {
    synchronized(pipeReaderMutex) {
        pipeReaderContexts.get(readerId)
    }
}

// Remove the stream context after the native thread has finished.
private func removePipeReaderContext(readerId: Int64): ?PipeReaderContext {
    synchronized(pipeReaderMutex) {
        pipeReaderContexts.remove(readerId)
    }
}

// C callback run by the native pipe reader thread to drain one Cangjie InputStream.
@C
func readPipeToEnd(readerId: Int64): Unit {
    let context = getPipeReaderContext(readerId)
    if (context.isNone()) {
        return
    }
    let readerContext = context.getOrThrow()
    var output = ""
    var error = ""
    try {
        output = StringReader(readerContext.stream).readToEnd()
    } catch (e: Exception) {
        error = e.message
    }
    synchronized(pipeReaderMutex) {
        if (pipeReaderContexts.contains(readerId)) {
            readerContext.output = output
            readerContext.error = error
        }
    }
}

// Join one native pipe reader and return its collected output and error message.
private func joinPipeReader(readerId: Int64, reader: CPointer<Unit>): (String, String) {
    var joinFailed = false
    try {
        if (reader.isNull() || !unsafe { CJPM_EXEC_JoinPipeReader(reader) }) {
            joinFailed = true
        }
    } finally {
        if (!reader.isNull()) {
            unsafe { CJPM_EXEC_FreePipeReader(reader) }
        }
    }
    let context = removePipeReaderContext(readerId)
    if (context.isNone()) {
        return ("", if (joinFailed) { "Failed to join pipe reader." } else { "" })
    }
    let readerContext = context.getOrThrow()
    let error = if (joinFailed && readerContext.error.isEmpty()) {
        "Failed to join pipe reader."
    } else {
        readerContext.error
    }
    return (readerContext.output, error)
}

// Simply execute the command.
// For expand command and without env config
public func execAndToTerminal(exeTool: String, arguments: Array<String>, timeout!: Option<Duration> = None,
    envBuilder!: EnvironmentBuilder = EnvironmentBuilder(), originalEnv!: ?Collection<(String, String)> = None, workingDirectory!: ?Path = None): Bool {
    try {
        let env = envBuilder.finish(originalEnv ?? getVariables())
        let subProcess = launch(exeTool, arguments, environment: env, workingDirectory: workingDirectory)
        let returnCode = subProcess.wait(timeout: timeout)
        return returnCode == 0
    } catch (e: Exception) {
        eprintln(e.message)
        return false
    }
}

// Execute the command and store the output.
public func execWithOutput(exeTool: String, arguments: ArrayList<String>,
    envMap!: HashMap<String, String> = HashMap<String, String>(), timeout!: Option<Duration> = None,
    workingDirectory!: ?Path = None): (Bool, String, String) {
    let subProcess: SubProcess
    var returnCode: Int64
    var outInfo = ""
    var errInfo = ""
    try {
        if (envMap.isEmpty()) {
            subProcess = launch(exeTool, arguments.toArray(), workingDirectory: workingDirectory,
                stdOut: ProcessRedirect.Pipe, stdErr: ProcessRedirect.Pipe)
        } else {
            let env = HashMap<String, String>(getVariables())
            for ((k, v) in envMap) {
                if (env.contains(k)) {
                    env[k] = v + env[k]
                } else {
                    env[k] = v
                }
            }
            subProcess = launch(exeTool, arguments.toArray(), environment: env, workingDirectory: workingDirectory,
                stdOut: ProcessRedirect.Pipe, stdErr: ProcessRedirect.Pipe)
        }

        let outReaderId = addPipeReaderContext(subProcess.stdOutPipe)
        let errReaderId = addPipeReaderContext(subProcess.stdErrPipe)
        let outReader = unsafe { CJPM_EXEC_StartPipeReader(outReaderId, readPipeToEnd) }
        let errReader = unsafe { CJPM_EXEC_StartPipeReader(errReaderId, readPipeToEnd) }
        if (outReader.isNull() || errReader.isNull()) {
            eprintln("Error: failed to start pipe reader for '${exeTool}'")
            subProcess.terminate(force: true)
            let _ = joinPipeReader(outReaderId, outReader)
            let _ = joinPipeReader(errReaderId, errReader)
            return (false, outInfo, errInfo)
        }
        let (out, outError) = joinPipeReader(outReaderId, outReader)
        let (err, errError) = joinPipeReader(errReaderId, errReader)
        outInfo = out
        errInfo = err
        if (!outError.isEmpty()) {
            eprintln(outError)
        }
        if (!errError.isEmpty()) {
            eprintln(errError)
        }
        returnCode = subProcess.wait(timeout: timeout)
        return (returnCode == 0 && outError.isEmpty() && errError.isEmpty(), outInfo, errInfo)
    } catch (e: Exception) {
        eprintln(e.message)
        return (false, outInfo, errInfo)
    }
}

// Execute command, and print the output indirectly.
public func exec(command: String): Bool {
    let cCommand = unsafe { LibC.mallocCString(command) }
    let res = unsafe { system(cCommand) }
    unsafe {
        LibC.free(cCommand)
    }
    return res == 0
}

// Execute commands through pipes and redirect the output to files.
public func execAndToFile(exeTool: String, arguments: ArrayList<String>, outFile: File, errFile: File,
    envBuilder!: EnvironmentBuilder = EnvironmentBuilder(), originalEnv!: ?Collection<(String, String)> = None, workingDirectory!: ?Path = None): Option<Int64> {
    try {
        let env = envBuilder.finish(originalEnv ?? getVariables())
        let subProcess = launch(exeTool, arguments.toArray(), workingDirectory: workingDirectory, environment: env,
            stdOut: ProcessRedirect.FromFile(outFile), stdErr: ProcessRedirect.FromFile(errFile))
        return subProcess.wait()
    } catch (e: Exception) {
        errFile.write("${e.message}\n".toArray())
        return Option.None
    }
}

public func getCpus(): Int64 {
    let size: Int64 = getProcessorCount()
    if (size <= 0) {
        // Faied to get cpu size, so it is set as the default value '8'
        return 8
    }
    return size
}

public func getOriginTargetDirectory(oriTargetDir: String, rootPath: String, cmdDir: String, configFileDir: String,
    needCreate: Bool): Option<String> {
    var targetDir: String = oriTargetDir

    if (targetDir == TARGET) {
        if (!cmdDir.isEmpty()) {
            targetDir = cmdDir
        } else if (!configFileDir.isEmpty()) {
            targetDir = configFileDir
        }
    }

    if (!safeCheck(getCanonicalPath(rootPath), "current path") ||
        !safeCheck(targetDir, Path(rootPath).join(CONFIG_FILE_NAME).toString())) {
        return Option.None
    }

    if (needCreate && !createDirectory(targetDir)) {
        return Option.None
    }

    if (directoryExists(targetDir)) {
        targetDir = getCanonicalPath(targetDir)
    }

    return targetDir
}

public func replaceEnvforTargetDir(tomlInfo: TomlInfo, path: String): Bool {
    if (tomlInfo.isPkgConfig) {
        if (!tomlInfo.pkg.targetDir.isEmpty() && !checkEnvVars(tomlInfo.pkg.targetDir, path)) {
            return false
        }
        tomlInfo.pkg.targetDir = replaceEnvVars(tomlInfo.pkg.targetDir)
    } else {
        if (!tomlInfo.workspace.targetDir.isEmpty() && !checkEnvVars(tomlInfo.workspace.targetDir, path)) {
            return false
        }
        tomlInfo.workspace.targetDir = replaceEnvVars(tomlInfo.workspace.targetDir)
    }
    return true
}

public func getScriptDirectory(oriScriptDir: String, rootPath: String, configFileDir: String, needCreate: Bool): Option<String> {
    var scriptDir: String = oriScriptDir

    if (!configFileDir.isEmpty()) {
            scriptDir = configFileDir
    }
    scriptDir = replaceEnvVars(scriptDir)

    if (!safeCheck(getCanonicalPath(rootPath), "current path") ||
        !safeCheck(scriptDir, Path(rootPath).join(CONFIG_FILE_NAME).toString())) {
        return Option.None
    }
    scriptDir = getPath(rootPath,scriptDir)
    if (needCreate && fileExists(Path(rootPath).join(BUILD_SCRIPT_NAME)) && !createDirectory(scriptDir)) {
        return Option.None
    }

    if (directoryExists(scriptDir)) {
        scriptDir = getCanonicalPath(scriptDir)
    }

    return scriptDir
}

public func replaceEnvforScriptDir(tomlInfo: TomlInfo, path: Path): Bool {
    let configPath = path.join(CONFIG_FILE_NAME).toString()
    let scriptDir = getScriptDirValue(tomlInfo)

    if (!scriptDir.isEmpty() && !checkEnvVars(scriptDir, configPath)) {
        return false
    }

    if (tomlInfo.isPkgConfig) {
        tomlInfo.pkg.scriptDir = replaceEnvVars(scriptDir)
    } else {
        tomlInfo.workspace.scriptDir = replaceEnvVars(scriptDir)
    }
    return true
}

public func getScriptDirValue(tomlInfo: TomlInfo): String {
    if (tomlInfo.isPkgConfig) {
        return tomlInfo.pkg.scriptDir
    } else {
        return tomlInfo.workspace.scriptDir
    }
}

// extract options from string, such as: ( -a  --ab="a bc" -g --bc "c d" ) -> [-a,--ab,"a bc",-g,--bc,"c d"]
public func extractOptionByString(input: String): ArrayList<String> {
    if (input.isEmpty()) {
        return ArrayList<String>()
    }

    let options = ArrayList<String>()
    var option: String = ""
    var count = 0
    for (k in input.runes()) {
        if (k == r' ') {
            if (option.isEmpty()) {
                continue
            }
            if (count % 2 == 0) {
                options.add(option)
                option = ""
                continue
            }
        }
        if (k == r'"') {
            if (option.isEmpty() || option[option.size - 1] != UInt8(UInt32(r'\\'))) {
                count += 1
            }
        }
        option += k.toString()
    }
    if (!option.isEmpty()) {
        options.add(option)
    }

    let res = ArrayList<String>()
    for (val in options) {
        res.add(all: parseEachArgs(val))
    }
    return res
}

// Check if dir1 is a parent of (or is same with) dir2
public func checkParentDir(dir1: String, dir2: String): Bool {
    if (dir1.isEmpty() || !directoryExists(dir1)) {
        return false
    }
    let arr1: Array<String> = dir1.split(PATH_SPLIT_CHAR)
    let arr2: Array<String> = dir2.split(PATH_SPLIT_CHAR)
    if (arr1.size > arr2.size) {
        return false
    }
    for (index in 0..arr1.size) {
        if (arr1[index] != arr2[index]) {
            return false
        }
    }
    return true
}

public func getRootPkgName(name: String): String {
    var idx = 0
    if (let Some(next) <- name.indexOf(UInt8(UInt32(r'.')), idx + 1)) {
        return name[0..next]
    }
    return name
}

public func getParentPkgName(name: String): String {
    if (let Some(next) <- name.lastIndexOf(UInt8(UInt32(r'.')))) {
        return name[0..next]
    }
    return ""
}

// parse option for pure string, such as: ("abc",-o=aaa,-d="vvv",k"l) -> [abc,-o,aaa,-d,vvv,k"l]
func parseEachArgs(input: String): ArrayList<String> {
    let res = ArrayList<String>()
    var option: String = ""
    var counts = 0
    var equals = 0
    var hasLeft: Bool = false
    var hasExplicitEmptyArg: Bool = false
    for (k in input.runes()) {
        if (k == r'=') {
            if (counts % 2 == 0 && equals == 0 && !option.isEmpty() && option.contains("-")) {
                res.add(option)
                option = ""
                equals += 1
                continue
            }
            if (counts % 2 == 0) {
                equals += 1
            }
        }
        if (k == r'"') {
            if (!option.isEmpty() && option[option.size - 1] == UInt8(UInt32(r'\\'))) {
                option += k.toString()
                continue
            }
            if (option.isEmpty() && !hasLeft) {
                counts += 1
                hasLeft = true
                continue
            }
            if (hasLeft) {
                if (option.isEmpty()) {
                    hasExplicitEmptyArg = true
                }
                counts += 1
                hasLeft = false
                continue
            }
        }
        option += k.toString()
    }
    if (!option.isEmpty()) {
        res.add(option)
    } else if (hasExplicitEmptyArg && counts % 2 == 0) {
        res.add("")
    }

    if (counts % 2 != 0) {
        return ArrayList<String>.of(input)
    }
    return res
}

func combineOptionString(preStr: String, postStr: String): String {
    if (preStr.isEmpty() || postStr.isEmpty()) {
        return preStr + postStr
    }
    return preStr + " " + postStr
}

// Change Path from "xxx/xxx/pro/xoo.cjo" to (xxx/xxx, xxx/xxx/pro), this path must be exist.
public func makePackagePath(path: String): (String, String) {
    let packagePath = Path(path).parent.toString()
    if (packagePath.isEmpty()) {
        throw NoneValueException()
    }
    let cangjiePath = Path(packagePath).parent.toString()
    if (cangjiePath.isEmpty()) {
        throw NoneValueException()
    }
    return (cangjiePath, packagePath)
}

// Hashcode of json data
public func bufferHashCode(b: ArrayList<String>): String {
    var dfh: DefaultHasher = DefaultHasher()
    for (k in b) {
        dfh.write(k.hashCode())
    }
    return dfh.finish().toString()
}

// Check environment config in field values
public func checkEnvVars(envVar: String, path: String): Bool {
    let regexEnvVars = Regex("\\$\\{(.*?)\\}")
    let matchDataArray: Array<MatchData> = regexEnvVars.findAll(envVar, group: true)
    for (matchData in matchDataArray) {
        if (!checkEnvKeys(matchData.matchString(0), path)) {
            return false
        }
    }

    return true
}

// Check if environment keys in field values exist
public func checkEnvKeys(key: String, path: String): Bool {
    var matched = Regex(#"^\${[a-zA-Z]+[_a-zA-Z0-9]*}$"#)
    if (matched.find(key).isNone()) {
        let jsonPath = path + PATH_SPLIT_CHAR + CONFIG_FILE_NAME
        eprintln("Error: the environment variable '${key}' is wrong in ${jsonPath}")
        return false
    }
    return true
}

// Replace environment config by real path
public func replaceEnvVars(envVar: String): String {
    if (envVar.isEmpty()) {
        return envVar
    }

    var res = envVar
    let regexEnvVars = Regex("\\$\\{(.*?)\\}")
    let matchDataArray = regexEnvVars.findAll(res, group: true)
    for (matchData in matchDataArray) {
        let key: String = matchData.matchString(0)
        let value: String = getVariable(matchData.matchString(1)) ?? ""
        res = res.replace(key, value)
    }

    return res
}

// extract config options from string, such as: ( -a  --cfg="a bc" -g --cfg "c d" ) -> [--cfg,"a bc",--cfg,"c d"]
public func extractCfgOption(input: String): ArrayList<String> {
    let oriRes = extractOptionByString(input)
    let res = ArrayList<String>()
    var index = 0
    while (index < oriRes.size - 1) {
        if ((oriRes[index] == "--cfg")) {
            res.add(all: oriRes[index..(index + 2)])
            index += 2
        } else {
            index += 1
        }
    }
    return res
}

// split org::name to org and name
// if there is multiple ::, retain extra :: in name
public func splitFullName(fullName: String): (String, String) {
    if (fullName.contains("::")) {
        let fullNameArr = fullName.split("::")
        let org = fullNameArr[0]
        return (org, fullName.removePrefix("${org}::"))
    }
    return ("", fullName)
}
