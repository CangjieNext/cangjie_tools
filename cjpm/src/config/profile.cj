// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*
import std.env.*
import stdx.serialization.serialization.*
import stdx.encoding.json.*

const TEST_COMPILATION_OPTIONS = "build"
const CUSTOMIZED_OPTION = "customized-option"
const ENV_CONFIG = "env"
const ENV_VALUE = "value"
const ENV_SPLICE_TYPE = "splice-type"

public class Profile <: Serializable<Profile> {
    public var build: BuildProfile = BuildProfile()
    public var test: TestProfile = TestProfile(JsonObject())
    public var bench: TestProfile = TestProfile(JsonObject())
    public var run: RunProfile = RunProfile()
    public var customizedOption: HashMap<String, String> = HashMap<String, String>()

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
        return result
    }
}

public class BuildProfile <: Serializable<BuildProfile> {
    public var ltoValue: String = ""
    public var incremental: Bool = false

    // Serialize of toml is useless.
    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): BuildProfile {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }
        var result = BuildProfile()
        result.ltoValue = String.deserialize((dms.get("lto") as DataModelString) ?? DataModelString(""))
        result.incremental = Bool.deserialize((dms.get("incremental") as DataModelBool) ?? DataModelBool(false))
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

        return JsonObject(fields).toJsonString().replace("\"", "\\\"").replace("\r", "").replace("\n", "")
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

public class EnvInfo <: Serializable<EnvInfo> {
    public var envValue: String = ""
    public var spliceType: String = ""

    public func serialize(): DataModel {
        return DataModelStruct()
    }

    public static func deserialize(dm: DataModel): EnvInfo {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }
        var result = EnvInfo()
        result.envValue = String.deserialize((dms.get(ENV_VALUE) as DataModelString) ?? DataModelString(""))
        result.spliceType = String.deserialize((dms.get(ENV_SPLICE_TYPE) as DataModelString) ?? DataModelString(""))
        return result
    }
}

public func envConfigToMap(envConfig: HashMap<String, EnvInfo>): HashMap<String, String> {
    let res: HashMap<String, String> = HashMap<String, String>()
    for ((envName, envInfo) in envConfig) {
        var spliceType = envInfo.spliceType
        let envs: HashMap<String, String> = HashMap(getVariables())
        if (!envs.contains(envName)) {
            res.add(envName, getFinalEnvValue(envName, envInfo.envValue, "replace"))
        } else if (spliceType != "absent") {
            res.add(envName, getFinalEnvValue(envName, envInfo.envValue, spliceType))
        }
    }
    return res
}

@When[os == "Linux" || os == "macOS"]
public func envMapToString(envMap: HashMap<String, String>): String {
    if (envMap.isEmpty()) {
        return ""
    }
    var res: String = ""
    for ((key, value) in envMap) {
        if (key == LD_PATH) {
            res += "${key}=${value}:$${LD_PATH} "
        } else if (key == PATH) {
            res += "${key}=${value}:$${PATH} "
        } else {
            res += "${key}=${value} "
        }
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
        if (key == PATH) {
            res += "set ${key}=${value};%${PATH}%&& "
        } else {
            res += "set ${key}=${value}&& "
        }
    }
    return res
}

@When[os == "Linux" || os == "macOS"]
public let ENV_HEAD_PATCH: String = ""
@When[os == "Linux" || os == "macOS"]
public let ENV_PATCH: String = ""

@When[os == "Linux" || os == "macOS"]
public func getFinalEnvValue(envName: String, envValue: String, spliceType: String): String {
    if (spliceType == "replace") {
        return "${envValue}"
    }
    if (spliceType == "prepend") {
        return "${envValue}:$${envName}"
    }
    if (spliceType == "append") {
        return "$${envName}:${envValue}"
    }
    return ""
}

@When[os == "Linux" || os == "macOS"]
func setFinalEnvValue(envName: String, envValue: String, spliceType: String): String {
    if (spliceType == "replace") {
        return "${envName}=\"${envValue}\""
    }
    if (spliceType == "prepend") {
        return "${envName}=\"${envValue}\":$${envName}"
    }
    if (spliceType == "append") {
        return "${envName}=$${envName}:\"${envValue}\""
    }
    return ""
}

@When[os == "Windows"]
public let ENV_HEAD_PATCH: String = "cmd /V /C \""
@When[os == "Windows"]
public let ENV_PATCH: String = "\""

@When[os == "Windows"]
public func getFinalEnvValue(envName: String, envValue: String, spliceType: String): String {
    if (spliceType == "replace") {
        return "${envValue}"
    }
    if (spliceType == "prepend") {
        return "${envValue};%${envName}%"
    }
    if (spliceType == "append") {
        return "%${envName}%;${envValue}"
    }
    return ""
}

@When[os == "Windows"]
func setFinalEnvValue(envName: String, envValue: String, spliceType: String): String {
    return "set ${envName}=${getFinalEnvValue(envName, envValue, spliceType)}&& "
}

public func appendEnvConfig(envConfig: HashMap<String, EnvInfo>): ArrayList<String> {
    var res = ArrayList<String>()
    for ((envName, envInfo) in envConfig) {
        var spliceType = envInfo.spliceType
        let envs: HashMap<String, String> = HashMap(getVariables())
        if (!envs.contains(envName)) {
            res.add(setFinalEnvValue(envName, envInfo.envValue, "replace"))
        } else if (spliceType != "absent") {
            res.add(setFinalEnvValue(envName, envInfo.envValue, spliceType))
        }
        if (!res.isEmpty() && !ENV_HEAD_PATCH.isEmpty()) {
            res[0] = ENV_HEAD_PATCH + res[0]
        }
    }
    return res
}

func verifyCustomizedOption(customizedOption: HashMap<String, String>, path: String): Bool {
    for ((k, v) in customizedOption) {
        if (!customizedOptionCheck(k, path)) {
            return false
        }
        let ownOptions = ["jobs", "verbose", "incremental", "help", "mock", "bench"]
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
        let envSpliceType: String = envInfo.spliceType
        if (!envSpliceType.isEmpty() && envSpliceType != "absent" && envSpliceType != "replace" &&
            envSpliceType != "prepend" && envSpliceType != "append") {
            eprintln("Error: splice type of environment variable '${envName}' in profile '${cmd}' " +
                "in ${configFilePath} can be only 'absent', 'replace', 'prepend' or 'append'")
            return false
        }
    }
    return true
}
