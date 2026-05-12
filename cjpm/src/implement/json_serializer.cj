// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.io.*
import std.fs.*
import std.env.*

import cjpm.config.*

import stdx.serialization.serialization.*
import stdx.encoding.json.*

private const JSON_PLAN_VERSION = 1

class JsonPlanSerializer <: PlanSerializer {
    private let targetDir: String
    private let testTargetDir: String
    private let modulePath: String
    private let ldPath: String
    private let taskIdToIndex: HashMap<TaskId, Int64>
    private var currentIndex: Int64 = 0
    private let tasks: ArrayList<PlanTaskData>
    private let out: OutputStream

    init(buildConfig: BuildConfig, out: OutputStream) {
        this.out = out
        this.targetDir = buildConfig.globalConfig.targetDir
        this.testTargetDir = Path(buildConfig.globalConfig.targetDir).join(UNITTEST_BIN).toString()
        this.modulePath = getCanonicalPath(buildConfig.globalConfig.modulePath)
        this.ldPath = buildConfig.globalConfig.ldPath |> collectString(delimiter: SPLIT_ENV)
        this.taskIdToIndex = HashMap<TaskId, Int64>()
        this.currentIndex = 0
        this.tasks = ArrayList<PlanTaskData>()
    }

    public func writeHeader(): Unit {}

    public func writeCompileTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTask(invocation, task, "compile")
    }

    public func writeLinkTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTask(invocation, task, "link")
    }

    private func writeTask(invocation: CompilerInvocation, task: CompileTask, taskType: String): Unit {
        let taskIndex = currentIndex
        taskIdToIndex[task.id] = taskIndex
        currentIndex += 1

        let args = ArrayList<String>()
        if (let Some(pkg) <- invocation.pkgPath) {
            args.add("-p")
            args.add(pkg)
        }
        if (let Some(obj) <- invocation.objPath) {
            args.add(obj)
        }
        for (p in invocation.importPaths) {
            args.add("--import-path=${p}")
        }
        for (f in invocation.cjcFlags) { args.add(f) }
        for (f in invocation.linkArgs) { args.add(f) }
        for (f in invocation.customOptions) { args.add(f) }
        for (f in invocation.linkOptions) { args.add(f) }
        for (f in invocation.outputConfigFlags) { args.add(f) }
        if (let Some(out) <- invocation.output) {
            args.add("-o=${out}")
        }

        let cmdEnv = invocation.envBuilder.asCliStrings(getVariables())
        let deps = resolveDependencies(task.requireTasks)

        tasks.add(PlanTaskData(taskIndex, taskType,
            PlanMetadata(invocation.metadata),
            PlanCommandData(invocation.pkgPath, invocation.objPath, invocation.importPaths,
                invocation.output, cmdEnv, args),
            deps, invocation.metadata.artifacts))
    }

    private func resolveDependencies(deps: HashSet<TaskId>): Array<Int64> {
        let result = ArrayList<Int64>()
        for (depId in deps) {
            if (let Some(idx) <- taskIdToIndex.get(depId)) {
                result.add(idx)
            }
        }
        result.toArray()
    }

    public func writeFooter(): Unit {
        let globalEnv = HashMap<String, String>()
        if (!ldPath.isEmpty()) {
            globalEnv[LD_PATH] = ldPath
        }

        let global = PlanGlobalData(targetDir, testTargetDir, modulePath, globalEnv)
        let output = PlanOutput(JSON_PLAN_VERSION, global, tasks)

        let jsonValue = output.serialize().toJson()
        let jsonObj = match (jsonValue as JsonObject) {
            case Some(j) => j
            case None => return
        }
        out.write(jsonObj.toJsonString(0, indent: "  ").toArray())
    }
}

class PlanOutput <: Serializable<PlanOutput> {
    var version: Int64
    var global: PlanGlobalData
    var tasks: ArrayList<PlanTaskData>

    public init(version: Int64, global: PlanGlobalData, tasks: ArrayList<PlanTaskData>) {
        this.version = version
        this.global = global
        this.tasks = tasks
    }

    public func serialize(): DataModel {
        return DataModelStruct()
            .add(field<Int64>("version", version))
            .add(field<PlanGlobalData>("global", global))
            .add(field<ArrayList<PlanTaskData>>("tasks", tasks))
    }

    public static func deserialize(dm: DataModel): PlanOutput {
        throw DataModelException("PlanOutput deserialize is not supported")
    }
}

class PlanGlobalData <: Serializable<PlanGlobalData> {
    var targetDir: String
    var testTargetDir: String
    var modulePath: String
    var env: HashMap<String, String>

    public init(targetDir: String, testTargetDir: String, modulePath: String, env: HashMap<String, String>) {
        this.targetDir = targetDir
        this.testTargetDir = testTargetDir
        this.modulePath = modulePath
        this.env = env
    }

    public func serialize(): DataModel {
        return DataModelStruct()
            .add(field<String>("targetDir", targetDir))
            .add(field<String>("testTargetDir", testTargetDir))
            .add(field<String>("modulePath", modulePath))
            .add(field<HashMap<String, String>>("env", env))
    }

    public static func deserialize(dm: DataModel): PlanGlobalData {
        throw DataModelException("PlanGlobalData deserialize is not supported")
    }
}

class PlanTaskData <: Serializable<PlanTaskData> {
    var id: Int64
    var taskType: String
    var metadata: PlanMetadata
    var command: PlanCommandData
    var dependencies: Array<Int64>
    var artifacts: Array<String>

    public init(id: Int64, taskType: String, metadata: PlanMetadata, command: PlanCommandData,
        dependencies: Array<Int64>, artifacts: Array<String>) {
        this.id = id
        this.taskType = taskType
        this.metadata = metadata
        this.command = command
        this.dependencies = dependencies
        this.artifacts = artifacts
    }

    public func serialize(): DataModel {
        return DataModelStruct()
            .add(field<Int64>("id", id))
            .add(field<String>("type", taskType))
            .add(field<PlanMetadata>("metadata", metadata))
            .add(field<PlanCommandData>("command", command))
            .add(field<Array<Int64>>("dependencies", dependencies))
            .add(field<Array<String>>("artifacts", artifacts))
    }

    public static func deserialize(dm: DataModel): PlanTaskData {
        throw DataModelException("PlanTaskData deserialize is not supported")
    }
}

class PlanMetadata <: Serializable<PlanMetadata> {
    var fullName: String
    var target: String
    var debug: Bool
    var mock: Bool
    var outputType: String
    var sourceSetDir: String
    var features: Array<String>

    public init(metadata: CompilerInvocationMetadata) {
        this.fullName = metadata.fullName
        this.target = metadata.target
        this.debug = metadata.debug
        this.mock = metadata.mock
        this.outputType = metadata.outputType.toString()
        this.sourceSetDir = metadata.sourceSetDir
        this.features = metadata.features
    }

    public func serialize(): DataModel {
        return DataModelStruct()
            .add(field<String>("fullName", fullName))
            .add(field<String>("target", target))
            .add(field<Bool>("debug", debug))
            .add(field<Bool>("mock", mock))
            .add(field<String>("outputType", outputType))
            .add(field<String>("sourceSetDir", sourceSetDir))
            .add(field<Array<String>>("features", features))
    }

    public static func deserialize(dm: DataModel): PlanMetadata {
        throw DataModelException("PlanMetadata deserialize is not supported")
    }
}

class PlanCommandData <: Serializable<PlanCommandData> {
    var pkgPath: Option<String>
    var objPath: Option<String>
    var importPaths: Array<String>
    var output: Option<String>
    var env: HashMap<String, String>
    var compiler: String
    var args: Array<String>

    public init(pkgPath: Option<String>, objPath: Option<String>, importPaths: Array<String>,
        output: Option<String>, env: HashMap<String, String>, args: ArrayList<String>) {
        this.pkgPath = pkgPath
        this.objPath = objPath
        this.importPaths = importPaths
        this.output = output
        this.env = env
        this.compiler = "cjc"
        this.args = args.toArray()
    }

    public func serialize(): DataModel {
        return DataModelStruct()
            .add(field<Option<String>>("pkgPath", pkgPath))
            .add(field<Option<String>>("objPath", objPath))
            .add(field<Array<String>>("importPaths", importPaths))
            .add(field<Option<String>>("output", output))
            .add(field<HashMap<String, String>>("env", env))
            .add(field<String>("compiler", compiler))
            .add(field<Array<String>>("args", args))
    }

    public static func deserialize(dm: DataModel): PlanCommandData {
        throw DataModelException("PlanCommandData deserialize is not supported")
    }
}
