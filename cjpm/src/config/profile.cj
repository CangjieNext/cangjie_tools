// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*
import std.convert.*
import std.fs.*
@When[os == "Linux" || os == "macOS"]
import std.io.*
import std.time.*
@When[os == "Windows"]
import std.regex.*
import std.deriving.Derive
import stdx.serialization.serialization.*
import stdx.encoding.json.*
@When[os == "Windows" || os == "macOS"]
import std.runtime.*

const TEST_COMPILATION_OPTIONS = "build"
const CUSTOMIZED_OPTION = "customized-option"
const ENV_CONFIG = "env"
const ENV_VALUE = "value"
const ENV_SPLICE_TYPE = "splice-type"
const K_BYTE: Float64 = 1024.0
let PAGE_SIZE: Float64 = getOSPageSize() // unit: Byte
public const COMPILE_PERFORMANCE_DIR = "performance_analysis"

public class Profile <: Serializable<Profile> {
    public var build: BuildProfile = BuildProfile()
    public var test: TestProfile = TestProfile(JsonObject())
    public var bench: TestProfile = TestProfile(JsonObject())
    public var run: RunProfile = RunProfile()
    public var customizedOption: HashMap<String, String> = HashMap<String, String>()
    public var isExperimental = false

    // Serialize of toml is useless.
    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): Profile {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }
        var result = Profile()
        result.build = BuildProfile.deserialize((dms.get("build") as DataModelStruct) ?? DataModelStruct())
        result.test = TestProfile.deserialize((dms.get("test") as DataModelStruct) ?? DataModelStruct())
        result.bench = TestProfile.deserialize((dms.get("bench") as DataModelStruct) ?? DataModelStruct())
        result.run = RunProfile.deserialize((dms.get("run") as DataModelStruct) ?? DataModelStruct())
        result.customizedOption = HashMap<String, String>.deserialize(
            (dms.get(CUSTOMIZED_OPTION) as DataModelStruct) ?? DataModelStruct())
        result.isExperimental = match (dms.get("experimental") as DataModelBool) {
            case Some(val) => val.getValue()
            case None => false
        }
        return result
    }
}

public class BuildProfile <: Serializable<BuildProfile> {
    public var ltoValue: String = ""
    public var incremental: Bool = false
    public var analysisCompilePerformance: Bool = false
    public var combined: HashMap<String, OutputType> = HashMap<String, OutputType>()
    public var cjcJobs: ?Int64 = None
    public var heuristic: Bool = false
    public var compilePipelineParallel: Bool = false

    // Serialize of toml is useless.
    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): BuildProfile {
        return match (dm) {
            case data: DataModelStruct => deserializeFromDataModelStruct(data)
            case _ => throw Exception("this data is not DataModelStruct")
        }
    }

    private static func deserializeFromDataModelStruct(dms: DataModelStruct): BuildProfile {
        var result = BuildProfile()
        result.ltoValue = String.deserialize((dms.get("lto") as DataModelString) ?? DataModelString(""))
        result.incremental = Bool.deserialize((dms.get("incremental") as DataModelBool) ?? DataModelBool(false))
        result.analysisCompilePerformance = Bool.deserialize((dms.get("performance_analysis") as DataModelBool) ??
            DataModelBool(false))
        result.combined = HashMap<String, OutputType>.deserialize((dms.get("combined") as DataModelStruct) ??
            DataModelStruct())
        if (let Some(cjcJobsDm) <- (dms.get("cjc-jobs") as DataModelInt)) {
            result.cjcJobs = Some(cjcJobsDm.getValue())
        }
        result.heuristic = Bool.deserialize((dms.get("enable-heuristic-parallelism") as DataModelBool) ?? DataModelBool(false))
        result.compilePipelineParallel = Bool.deserialize((dms.get("compile-pipeline-parallel") as DataModelBool) ?? DataModelBool(false))
        return result
    }
}

public class TestProfile <: Serializable<TestProfile> {
    public let value: JsonObject

    TestProfile(json: JsonValue) {
        this.value = json.asObject()
    }

    public prop verbose: Bool {
        get() {
            compilationBoolOptionByName("verbose")
        }
    }

    public prop mock: ?MockSupport {
        get() {
            match (compilationOptionByName("mock")?.asString().getValue()) {
                case Some(mock) => MockSupport.fromString(mock)
                case None => None
            }
        }
    }

    public prop ltoValue: String {
        get() {
            match (compilationOptionByName("lto")?.asString().getValue()) {
                case Some(val) => val
                case None => ""
            }
        }
    }

    public prop compileOption: String {
        get() {
            match (compilationOptionByName(COMPILE_OPTION)?.asString().getValue()) {
                case Some(val) => val
                case None => ""
            }
        }
    }

    public prop reportFormat: ?String {
        get() {
            if (let Some(format) <- value.get("report-format")) {
                return String.deserialize(DataModel.fromJson(format))
            }
            return None
        }
    }

    public prop reportPath: ?String {
        get() {
            if (let Some(path) <- value.get("report-path")) {
                return String.deserialize(DataModel.fromJson(path))
            }
            return None
        }
    }

    public prop isProgressReportDisabled: Bool {
        get() {
            compilationBoolOptionByName("no-progress")
        }
    }

    public prop isProgressReportBrief: Bool {
        get() {
            compilationBoolOptionByName("progress-brief")
        }
    }

    public prop progressReportEntriesLimit: ?Int64 {
        get() {
            if (let Some(limit) <- value.get("progress-entries-limit")?.asInt().getValue()) {
                return limit
            }
            return None
        }
    }

    public prop envConfig: HashMap<String, EnvInfo> {
        get() {
            if (let Some(envs) <- value.get(ENV_CONFIG)) {
                return HashMap<String, EnvInfo>.deserialize(DataModel.fromJson(envs))
            }
            return HashMap<String, EnvInfo>()
        }
    }

    public func serialize(): DataModel {
        return DataModel.fromJson(value)
    }

    public static func deserialize(dm: DataModel): TestProfile {
        let fields = dm.toJson().asObject().getFields()
        return TestProfile(JsonObject(fields))
    }

    public func toStringWithoutCompilationOptions(): String {
        let fields = value.getFields()
        fields.remove(TEST_COMPILATION_OPTIONS)
        fields.remove(ENV_CONFIG)

        return JsonObject(fields).toJsonString().replace("\r", "").replace("\n", "")
    }

    private func compilationBoolOptionByName(name: String): Bool {
        if (let Some(value) <- compilationOptionByName(name)) {
            value.asBool().getValue()
        } else {
            false
        }
    }

    private func compilationOptionByName(name: String): ?JsonValue {
        if (let Some(compilationOptions) <- value.get(TEST_COMPILATION_OPTIONS)) {
            for ((k, v) in compilationOptions.asObject().getFields()) {
                if (k == name) {
                    return v
                }
            }
        }
        return None
    }

    public func isEmpty(): Bool {
        return this.value.size() == 0
    }
}

public class RunProfile <: Serializable<RunProfile> {
    public var envConfig: HashMap<String, EnvInfo> = HashMap<String, EnvInfo>()

    // Serialize of toml is useless.
    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): RunProfile {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }
        var result = RunProfile()
        result.envConfig = HashMap<String, EnvInfo>.deserialize(
            (dms.get(ENV_CONFIG) as DataModelStruct) ?? DataModelStruct())
        return result
    }
}

@Derive[Equatable]
public enum SpliceType <: ToString {
    | ABSENT
    | REPLACE
    | PREPEND
    | APPEND
    | Unknown(String)

    static func fromString(value: String): SpliceType {
        match {
            case value == "absent" => ABSENT
            case value == "replace" || value.isEmpty() => REPLACE
            case value == "prepend" => PREPEND
            case value == "append" => APPEND
            case _ => Unknown(value)
        }
    }

    public func toString(): String {
        match (this) {
            case ABSENT => "absent"
            case REPLACE => "replace"
            case PREPEND => "prepend"
            case APPEND => "append"
            case Unknown(s) => s
        }
    }

    public prop isAbsent: Bool {
        get() {
            match (this) {
                case ABSENT | Unknown(_) => true
                case _ => false
            }
        }
    }
}

public struct EnvInfo <: Serializable<EnvInfo> {
    private EnvInfo(
        public let envValue: String,
        public let spliceType: SpliceType
    ) {}

    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): EnvInfo {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }

        let spliceTypeString = match (dms.get(ENV_SPLICE_TYPE)) {
            case _: DataModelNull => ""
            case s: DataModelString => s.getValue()
            case _ => "invalid"
        }
        return EnvInfo(
            String.deserialize((dms.get(ENV_VALUE) as DataModelString) ?? DataModelString("")),
            SpliceType.fromString(spliceTypeString)
        )
    }
}

@When[os == "Linux" || os == "macOS"]
public func envMapToString(envMap: HashMap<String, String>): String {
    if (envMap.isEmpty()) {
        return ""
    }
    var res: String = ""
    for ((key, value) in envMap) {
        res += "${key}=${value} "
    }
    return res
}

@When[os == "Windows"]
public func envMapToString(envMap: HashMap<String, String>): String {
    if (envMap.isEmpty()) {
        return ""
    }
    var res: String = "cmd /V /C \""
    for ((key, value) in envMap) {
        res += "set ${key}=${value}&& "
    }
    return res
}

@When[os == "Linux" || os == "macOS"]
public let ENV_HEAD_PATCH: String = ""
@When[os == "Linux" || os == "macOS"]
public let ENV_PATCH: String = ""

@When[os == "Linux" || os == "macOS"]
public func getFinalEnvValue(envName: String, envValue: String, spliceType: SpliceType): String {
    match (spliceType) {
        case REPLACE => "${envValue}"
        case PREPEND => "${envValue}:$${envName}"
        case APPEND => "$${envName}:${envValue}"
        case _ => ""
    }
}

@When[os == "Windows"]
public let ENV_HEAD_PATCH: String = "cmd /V /C \""
@When[os == "Windows"]
public let ENV_PATCH: String = "\""

@When[os == "Windows"]
public func getFinalEnvValue(envName: String, envValue: String, spliceType: SpliceType): String {
    match (spliceType) {
        case REPLACE => "${envValue}"
        case PREPEND => "${envValue};%${envName}%"
        case APPEND => "%${envName}%;${envValue}"
        case _ => ""
    }
}

public func verifyCombinedField(cfg: TomlInfo, path: String): Bool {
    var flag = true
    for ((pkgName, outputType) in cfg.profile.build.combined) {
        if (pkgName != cfg.pkg.name) {
            eprintln("Error: field 'profile.build.combined' in ${Path(path).join(CONFIG_FILE_NAME).toString()} " +
            "can only used for root package '${cfg.pkg.name}', current value is '${pkgName}'")
            flag = false
            continue
        }
        if (outputType != Dynamic) {
            eprintln(
                "Error: field 'profile.build.combined.${cfg.pkg.name}' in " +
                    "${Path(path).join(CONFIG_FILE_NAME).toString()} can only configured as 'dynamic'")
            flag = false
            continue
        }
        eprintln("Warning: field 'profile.build.combined' in '${Path(path).join(CONFIG_FILE_NAME).toString()}' " +
            "is experimental, it is unstable and has some limitations, " +
            "please refer to related documentation for more details")
    }
    return flag
}

func verifyCustomizedOption(customizedOption: HashMap<String, String>, path: String): Bool {
    for ((k, v) in customizedOption) {
        if (!customizedOptionCheck(k, path)) {
            return false
        }
        let ownOptions = ["jobs", "coverage", "verbose", "incremental", "help", "lint", "mock", "bench"]
        if (ownOptions.contains(k)) {
            eprintln("Error: '${k}' is an option used by cjpm, it does not allow to be used as a customized key")
            return false
        }
        if (!safeCheck(v, path)) {
            return false
        }
        customizedOption[k] = fixCompileOption(v, path)
    }
    return true
}

public func verifyEnvConfig(envConfig: HashMap<String, EnvInfo>, cmd: String, configFilePath: String): Bool {
    for ((envName, envInfo) in envConfig) {
        if (!envNameCheck(envName, cmd, configFilePath)) {
            return false
        }
        if (let Unknown(_) <- envInfo.spliceType) {
            eprintln("Error: splice type of environment variable '${envName}' in profile '${cmd}' " +
                "in ${configFilePath} can be only '${SpliceType.ABSENT}', '${SpliceType.REPLACE}', " +
                "'${SpliceType.PREPEND}' or '${SpliceType.APPEND}'")
            return false
        }
    }
    return true
}

@When[os == "Linux"]
func getMemoryValueFromMeminfoString(meminfoStr: String, target: String): Float64 {
    var memoryStr = String.empty
    try {
        let targetBeginPos: Int64 = meminfoStr.indexOf(target).getOrThrow()
        let targetEndPos: Int64 = targetBeginPos + target.size
        let kBBeginPos: Int64 = meminfoStr.indexOf("kB", targetEndPos).getOrThrow()
        memoryStr = meminfoStr[targetEndPos + 1..kBBeginPos]
    } catch (_) {
    }

    return Float64.parse(memoryStr) / K_BYTE  // unit: MB
}

/**
 * @brief Get memory usage of current OS, including all tasks, the format of `/proc/meminfo` is like:
 *      MemTotal:        xxx kB
 *      MemFree:         xxx kB  // totally unused memory
 *      MemAvailable:    xxx kB  // estimated available memory
 *      Buffers:         xxx kB  // can be released if memory is not enough
 *      Cached:          xxx kB  // can be released if memory is not enough
 *      ...
 *      SReclaimable:    xxx kB  // can be reclaimed in slab
 *      ...
 */
@When[os == "Linux"]
public func getMemInfo(): Float64 {
    let linuxMemPath = Path("/proc/meminfo")
    let meminfo = File(linuxMemPath, Read)
    let meminfoBuffer = readToEnd(meminfo)
    let meminfoStr = String.fromUtf8(meminfoBuffer)
    let totalMemory = getMemoryValueFromMeminfoString(meminfoStr, "MemTotal:")
    let memoryAvailableList: Array<String> = ["MemFree:", "Buffers:", "Cached:", "SReclaimable:"]
    var memoryAvailable: Float64 = 0.0
    for (item in memoryAvailableList) {
        memoryAvailable += getMemoryValueFromMeminfoString(meminfoStr, item)
    }
    return totalMemory - memoryAvailable
}

/**
 * @brief Get memory usage of current process, the format of `/proc/self/statm` is like:
 *      num1 num2 num3 num4 num5 num6 num7
 * Just seven numbers in one line, their meanings are:
 *      num1: total memory
 *      num2: real used memory
 *      num3: shared memory
 *      num4: text segment memory
 *      num5: dynamic lib memory
 *      num6: data segment memory
 *      num7: dirty page memory
 * the unit of these memory is page, not KB or MB, 1 page = 4 KB
 */
@When[os == "Linux"]
public func getCjpmMemInfo(): Float64 {
    let usedMemoryIdx = 1
    let linuxCjpmInitMemPath: Path = Path("/proc/self/statm")
    let statm: File = File(linuxCjpmInitMemPath, Read)
    let statmBuffer = readToEnd(statm)
    let statmStr = String.fromUtf8(statmBuffer)
    let statmArray = statmStr.split(" ")
    return Float64.parse(statmArray[usedMemoryIdx]) * PAGE_SIZE / K_BYTE / K_BYTE // unit: MB
}

@When[os == "Windows"]
public func getCjpmMemInfo(): Float64 {
    var cjpmMemInfo = Float64(getUsedHeapSize()) / K_BYTE / K_BYTE
    return cjpmMemInfo
}

// ---------------------------------------------------------
// macOS Specific Legacy Interfaces (Retained for Compatibility)
// ---------------------------------------------------------
@C
struct vm_statistics64_data_t {
    var free_count: UInt32 = 0
    var active_count: UInt32 = 0
    var inactive_count: UInt32 = 0
    var wire_count: UInt32 = 0
    var zero_fill_count: UInt64 = 0
    var reactivations: UInt64 = 0
    var pageins: UInt64 = 0
    var pageouts: UInt64 = 0
    var faults: UInt64 = 0
    var cow_faults: UInt64 = 0
    var lookups: UInt64 = 0
    var hits: UInt64 = 0
    var purges: UInt64 = 0
    var purgeable_count: UInt32 = 0
    var speculative_count: UInt32 = 0
    var decompressions: UInt64 = 0
    var compressions: UInt64 = 0
    var swapins: UInt64 = 0
    var swapouts: UInt64 = 0
    var compressor_page_count: UInt32 = 0
    var throttled_count: UInt32 = 0
    var external_page_count: UInt32 = 0
    var internal_page_count: UInt32 = 0
    var total_uncompressed_pages_in_compressor: UInt64 = 0
}

@When[os == "macOS"]
foreign func host_statistics64(mach_host: Int64, HOST_VM_INFO64: Int32,
    memoryData: CPointer<vm_statistics64_data_t>, mach_msg_type_number_t: CPointer<UInt32>): Int64

@When[os == "macOS"]
foreign func mach_host_self(): Int64

@When[os == "macOS"]
foreign func getpagesize(): Int64

@C
struct MachTaskBasicInfoData {
    var virtual_size: UInt64 = 0
    var resident_size: UInt64 = 0
    var user_time_seconds: Int32 = 0
    var user_time_microseconds: Int32 = 0
    var system_time_seconds: Int32 = 0
    var system_time_microseconds: Int32 = 0
    var policy: Int32 = 0
    var suspend_count: Int32 = 0
}

@When[os == "macOS"]
foreign func mach_task_self(): Int64

@When[os == "macOS"]
foreign func task_info(targetTask: Int64, flavor: Int32, memoryData: CPointer<MachTaskBasicInfoData>,
    mach_msg_type_number_t: CPointer<UInt32>): Int32

@When[os == "macOS"]
func getOSPageSize(): Float64 {
    unsafe {
        return Float64(getpagesize())
    }
}

@When[os != "macOS"]
func getOSPageSize(): Float64 {
    return 4.0 * K_BYTE
}

@When[os == "macOS"]
public func getMemInfo(): Float64 {
    unsafe {
        var memoryData = vm_statistics64_data_t()
        // there are 38 32-bit data in vm_statistics64_data_t
        var mach_msg_type_number_t: UInt32 = 38
        // `HOST_VM_INFO64` is global var in <mac/mac.h>, and its value is always 4, that means we want memory info
        const HOST_VM_INFO64: Int32 = 4
        host_statistics64(mach_host_self(), HOST_VM_INFO64, inout memoryData, inout mach_msg_type_number_t)
        return Float64(memoryData.active_count + memoryData.wire_count) * PAGE_SIZE / K_BYTE / K_BYTE // unit: MB
    }
}

@When[os == "macOS"]
public func getCjpmMemInfo(): Float64 {
    unsafe {
        var memoryData = MachTaskBasicInfoData()
        const MACH_TASK_BASIC_INFO: Int32 = 20
        // sizeof(MachTaskBasicInfoData) / sizeof(integer_t)
        var mach_msg_type_number_t: UInt32 = 10
        let status = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, inout memoryData, inout mach_msg_type_number_t)
        if (status == 0) {
            return Float64(memoryData.resident_size) / K_BYTE / K_BYTE
        }
        return Float64(getUsedHeapSize()) / K_BYTE / K_BYTE
    }
}


// ---------------------------------------------------------
// macOS New Implementation (libproc for core compilers)
// ---------------------------------------------------------
@When[os == "macOS"]
@C
struct ProcTaskInfo {
    var pti_virtual_size: UInt64 = 0       
    var pti_resident_size: UInt64 = 0      
    var pti_total_user: UInt64 = 0
    var pti_total_system: UInt64 = 0
    var pti_threads_user: UInt64 = 0
    var pti_threads_system: UInt64 = 0
    var pti_policy: Int32 = 0
    var pti_faults: Int32 = 0
    var pti_pageins: Int32 = 0
    var pti_cow_faults: Int32 = 0
    var pti_messages_sent: Int32 = 0
    var pti_messages_received: Int32 = 0
    var pti_syscalls_mach: Int32 = 0
    var pti_syscalls_unix: Int32 = 0
    var pti_csw: Int32 = 0
    var pti_threadnum: Int32 = 0
    var pti_numrunning: Int32 = 0
    var pti_priority: Int32 = 0
}

@When[os == "macOS"]
foreign func proc_listpids(ptype: UInt32, ptypeinfo: UInt32, buffer: CPointer<UInt8>, buffersize: Int32): Int32

@When[os == "macOS"]
foreign func proc_name(pid: Int32, buffer: CPointer<UInt8>, buffersize: UInt32): Int32

@When[os == "macOS"]
foreign func proc_pidinfo(pid: Int32, flavor: Int32, arg: UInt64, buffer: CPointer<ProcTaskInfo>, buffersize: Int32): Int32

// 使用单一的 CPointer<UInt8> 规避 CPointer 命名冲突
@When[os == "macOS"]
foreign func malloc(size: UIntNative): CPointer<UInt8>
@When[os == "macOS"]
foreign func free(ptr: CPointer<UInt8>): Unit

@When[os == "macOS"]
func getCString(buffer: CPointer<UInt8>, maxSize: Int64): String {
    unsafe {
        var len = 0
        while (len < maxSize && buffer.read(len) != 0) {
            len++
        }
        if (len == 0) {
            return ""
        }
        var bytes = Array<UInt8>(len, repeat: 0)
        for (i in 0..len) {
            bytes[i] = buffer.read(i)
        }
        return String.fromUtf8(bytes)
    }
}

@When[os == "macOS"]
public func getCoreCompilerMemInfo(): Float64 {
    var totalRSSBytes: UInt64 = 0
    let targetProcesses = ["cjpm", "cjc", "llc", "opt"]
    
    // 对应系统的 C 宏常量
    const PROC_ALL_PIDS: UInt32 = 1
    const PROC_PIDTASKINFO: Int32 = 4
    
    unsafe {
        // 传入空指针探查当前需要分配多大内存
        let nullPtr = CPointer<UInt8>()
        let bytesNeeded = proc_listpids(PROC_ALL_PIDS, 0, nullPtr, 0)
        
        if (bytesNeeded <= 0) {
            return 0.0
        }
        
        let pidBuffer = malloc(UIntNative(bytesNeeded))
        // 获取有效数量，每个 PID 占据 4 字节
        let count = proc_listpids(PROC_ALL_PIDS, 0, pidBuffer, bytesNeeded) / 4 
        let nameBuffer = malloc(UIntNative(256)) 
        
        for (i in 0..count) {
            // 利用偏移手动拼装 Int32，规避跨类型强转问题
            let offset = Int64(i * 4)
            let b0 = UInt32(pidBuffer.read(offset))
            let b1 = UInt32(pidBuffer.read(offset + 1))
            let b2 = UInt32(pidBuffer.read(offset + 2))
            let b3 = UInt32(pidBuffer.read(offset + 3))
            let pid = Int32(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24))
            
            if (pid <= 0) { continue }
            
            let nameLen = proc_name(pid, nameBuffer, 256)
            if (nameLen > 0) {
                let procName = getCString(nameBuffer, 256)
                
                var isTarget = false
                for (target in targetProcesses) {
                    if (procName == target) {
                        isTarget = true
                        break
                    }
                }
                
                if (isTarget) {
                    var taskInfo = ProcTaskInfo()
                    let sizeOfTaskInfo: Int32 = 96 // 64位macOS中 proc_taskinfo 是严格的96字节
                    
                    let status = proc_pidinfo(
                        pid, 
                        PROC_PIDTASKINFO, 
                        0, 
                        inout taskInfo, 
                        sizeOfTaskInfo
                    )
                    
                    if (status == sizeOfTaskInfo) {
                        totalRSSBytes += taskInfo.pti_resident_size
                    }
                }
            }
        }
        
        free(pidBuffer)
        free(nameBuffer)
    }
    
    return Float64(totalRSSBytes) / 1024.0 / 1024.0
}

@When[os == "Windows"]
func parseTasklistMemUsageToBytes(memUsage: String): UInt64 {
    var digits = ""
    let numberRegex = Regex("[0-9]+")
    for (m in numberRegex.findAll(memUsage)) {
        digits += m.matchString(0)
    }
    if (digits.isEmpty()) {
        return 0
    }
    let kiloBytes = Int64.tryParse(digits) ?? 0
    return UInt64(kiloBytes) * 1024
}

@When[os == "Windows"]
public func getCoreCompilerMemInfo(): Float64 {
    let targets = HashSet<String>(["cjpm.exe", "cjc.exe", "llc.exe", "opt.exe", "cjpm", "cjc", "llc", "opt"])
    let args = ArrayList<String>(["/FO", "CSV", "/NH"])
    let (ok, outInfo, _) = execWithOutput("tasklist", args)
    if (!ok || outInfo.isEmpty()) {
        return 0.0
    }

    var totalRSSBytes: UInt64 = 0
    let lineRegex = Regex("^\"([^\"]+)\",\"([^\"]*)\",\"([^\"]*)\",\"([^\"]*)\",\"([^\"]+)\"$", MultiLine)
    for (line in outInfo.split("\n")) {
        let row = line.replace("\r", "")
        if (row.isEmpty()) {
            continue
        }
        match (lineRegex.find(row, group: true)) {
            case Some(md) =>
                let imageName = md.matchString(1)
                if (targets.contains(imageName)) {
                    totalRSSBytes += parseTasklistMemUsageToBytes(md.matchString(5))
                }
            case _ => ()
        }
    }

    return Float64(totalRSSBytes) / K_BYTE / K_BYTE
}

// ---------------------------------------------------------
// Thread Spawner implementation split by OS
// ---------------------------------------------------------

// 非 macOS/Windows 系统：继续沿用以前通过系统内存估算和 Offset 差值的方案
@When[os == "Linux"]
public func addMemoryProcess(): Future<Thread> {
let cjpmMemOffset = getMemInfo() - getCjpmMemInfo()
    CJPM_COMPILE_INFO.add(DateTime.now().toUnixTimeStamp().toMilliseconds().toString(), getCjpmMemInfo())
    let memoryProcess = spawn { =>
        while (true) {
            if (Thread.currentThread.hasPendingCancellation) {
                return Thread.currentThread
            }
            sleep(100 * Duration.millisecond)
            var cjpmRealTimeSystemPhysicalMemory = getMemInfo() - cjpmMemOffset
            var cjpmTime: String = DateTime.now().toUnixTimeStamp().toMilliseconds().toString()
            CJPM_COMPILE_INFO.add(cjpmTime,cjpmRealTimeSystemPhysicalMemory)
        }
        return Thread.currentThread
    }
    return memoryProcess
}

// macOS/Windows 共用：直接提取核心编译进程 (cjc/cjpm/llc/opt) 的 RSS 之和
@When[os == "macOS" || os == "Windows"]
public func addMemoryProcess(): Future<Thread> {
    CJPM_COMPILE_INFO.add(DateTime.now().toUnixTimeStamp().toMilliseconds().toString(), getCoreCompilerMemInfo())
    let memoryProcess = spawn { =>
        while (true) {
            if (Thread.currentThread.hasPendingCancellation) {
                return Thread.currentThread
            }
            sleep(100 * Duration.millisecond)
            
            // 使用纯 FFI 调用抓取进程列表并统计
            let compilerMem = getCoreCompilerMemInfo()
            var cjpmTime: String = DateTime.now().toUnixTimeStamp().toMilliseconds().toString()
            CJPM_COMPILE_INFO.add(cjpmTime, compilerMem)
        }
        return Thread.currentThread
    }
    return memoryProcess
}
