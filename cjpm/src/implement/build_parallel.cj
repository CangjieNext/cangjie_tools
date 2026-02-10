// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.collection.concurrent.*
import std.convert.*
import std.deriving.Derive
import std.env.getVariables
import std.fs.*
import std.regex.*
import std.sync.*
import std.time.*

import stdx.serialization.serialization.*
import stdx.encoding.json.*

import cjpm.config.*
import cjpm.util.*

@When[(os != "iOS") && (env != "ohos") && (env != "android")]
foreign func initFSWatcher(dirs: CString, cb: CFunc<(CString) -> Unit>, verbose: Bool): Bool

@When[(os != "iOS") && (env != "ohos") && (env != "android")]
foreign func startFSWatcher(verbose: Bool): Unit

@When[(os != "iOS") && (env != "ohos") && (env != "android")]
foreign func stopFSWatcher(verbose: Bool): Unit

type TaskMap = HashMap<TaskId, CompileTask>
type DownStreamTasks = HashMap<TaskId, HashSet<TaskId>>
type Task2SyncCounter = HashMap<TaskId, SyncCounter>

// Self-sufficient compilation unit. 
// Stores all of the information for constructing a single `cjc` call
public struct CompileTask {
    public CompileTask(
        public let id!: TaskId,
        // The directory, of current `target`
        public let targetDir!: String = "",
        // The value of `--target-dir`: where to dump the compilation result (depends on many factors)
        public let targetPath!: String = "", 
        // The value of `-p`: The location of package files of current source set
        public let packagePath!: String = "", 
        // Is this a product source set (`--output-type` can be of binary)
        public let product!: Bool = false, 
        // Source set directory without the module path prefix. Uniquely identifies source set
        public let sourceSetDir!: String = "", 
        // The feature set header of the package
        public let sourceSetFeatures!: HashSet<Feature> = HashSet(), 
        // e.g. for `cjpm.config` the value would be `cjpm`
        public let rootPkgName!: String = "", 
        // Full package name, e.g. `cjpm.config`
        public let fullName!: String = "", 
        // The expected name of output file without extension
        public let filename!: String = "", 
        // The value of `--output-type`
        public let outputType!: OutputType = Unknown(""), 
        // The target triple passed to `--target` for compilation/cross-compilation
        public let target!: String = "", 
        // `true` if value stored in `target` field is different from host
        public let isCrossCompile!: Bool = false, 
        // A set of option fields from `cjpm.toml` of a module, specialized for this particular package
        public let compileOption!: String = "", 
        public let overrideOption!: String = "", 
        public let linkOption!: String = "", 
        public let customizedOption!: ArrayList<String> = ArrayList(), 
        // Stores names of all subpackages of a module to combine them into a single compilation unit
        public let subPkgSet!: HashSet<String> = HashSet(), 
        // Use `--whole-archive` to link dendencies
        public let isCombined!: Bool = false,
        // Compile this root package and make it into a single linkable binary
        public let isProjectCombined!: Bool = false,
        // Package is being compiled by `cjpm test`
        public let requiredForTests!: Bool = false, 
        // Include some declarations from testfiles `*_test.cj` of a package.
        public let exportForTests!: Bool = false, 
        // Incoming edges of dependency graph
        public let requireTasks!: HashSet<TaskId> = HashSet(), 
        // All features, asked by different modules of this package. Required for proper selection of product source set
        public let allEnabledFeatures!: HashSet<Feature> = HashSet(), 
        // Mappings from `[[feature]]` array of tables from `cjpm.toml`. Used to provide correct `*.cjo` files in `--import-path`
        public let featureMapping!: FeatureDeducer = FeatureDeducer.empty, 
        // Does this compiler call support AnalysisCompilePerformance
        public let isAnalysisCompilePerformance!: Bool = false, 
        // `true` if:
        //  - this is a parent package of some subpackage
        //  - requiredForTests
        public let hasSubPkgs!: Bool = false, 
        // Is this compilation task for debug build
        public let isDebug!: Bool = false,
        // Compile package with support of `coverage`
        public let isCov!: Bool = false,
        // Compile package with support of `mock`
        public let mockSupported!: Bool = false,
        // Is package a macro package
        public let isMacro!: Bool = false, 
        // A value of `--lto` flag to be passed to a compiler
        public let isLto!: Bool = false,
        public let ltoValue!: String = "",
        // Locations of `*.chir` and `*.cjo` of each source set, the current one depends on
        // 0 - CHIR file with declarations to be merged for multiplatform build
        // 1 - The value of `--common-part-cjo` for multiplatform build
        public let prevStageArtifacts!: Array<(String, String)> = Array(), 
        // Whether the source code location is spread between multiple [[source-set]] src-dir
        public let isMultiplatform!: Bool = false, 
        // A list of `-Woff` compiler options
        public let supressed!: ArrayList<String> = ArrayList<String>(),
        // The directory, where `outLogFile` and `errLogFile` are stored
        public let logPath!: String = "",
        // A file for verbose output of `cjpm` when compiling task
        public let outLogFile!: String = "", 
        // A joined file containing `stderr` of a compiler together with `cjpm` errors
        public let errLogFile!: String = "",
        // The value of `--compile-target`
 	    public let compileTargetOutput!: ?OutputType = None,
        // Default - compiling as single `cjc` call
        // ObjOnly - `cjc` call with `--output-type=obj`. Produces `*.o/obj` files
        // LinkOnly - `cjc` call which links result in a single binary. Takes `prevStageObj` as input
        public let compilePhase!: CompilePhase = Default,
        // The *.o/obj file generated by a ObjOnly task, as the input of a LinkOnly task
 	    public let prevStageObj!: ?String = None
    ) { }
}

enum Notification {
    | CjoReady
    | ChirReady
    | ObjReady
    | ExeReady
    | DylibReady
    | StaticlibReady
    | Err

    public func toString(): String {
        match (this) {
            case CjoReady => "CjoReady"
            case ChirReady => "ChirReady"
            case ObjReady => "ObjReady"
            case ExeReady => "ExeReady"
            case DylibReady => "DylibReady"
            case StaticlibReady => "StaticlibReady"
            case Err => "Err"
        }
    }
}

let OUTPUTTYPE2NOTIFICATION = HashMap<String, Notification>(
    [
        ("static", StaticlibReady),
        ("dynamic", DylibReady),
        ("executable", ExeReady),
        ("chir", ChirReady),
        ("obj", ObjReady)
    ]
)

func parallelBuild(module: ModuleResolve, buildConfig: BuildConfig): Bool {
    let engine = IncrementalEngine(
        cacheLocation: Path(buildConfig.globalConfig.targetDir).join(INCREMENTAL_CACHE)
    )

    // The `--output-type` for `exe` files is set to `staticlib` when compiling it via `cjpm test`
    // So the output type has to be a part of task identifier
    let productOutputTypes = HashMap<String, OutputType>()

    // modules.resolves is topologically sorted
    let (taskMap, orderedTasks) = module.resolves.fold((TaskMap(), ArrayList<TaskId>())) {
        acc, r: ResolveItem =>
        // TODO: consider removing it
        if (!r.hasProdFiles) {
            return acc
        }
        let compileTasks = createPackageCompileTasks(r, buildConfig, productOutputTypes)
        for (compileTask in compileTasks) {
            if (compileTask.product) {
                productOutputTypes.add(compileTask.fullName, compileTask.outputType)
            }
            registerReadSourceTask(engine, compileTask, buildConfig)
            registerReadCompilerOptionsTask(engine, compileTask, buildConfig)
            // TODO: registerReadEnvVarsTask(engine, compileTask, buildConfig)
            registerCompilerCallTask(engine, compileTask, buildConfig)
        }
        acc
    }
    
    engine.askAll()

    buildConfig.rebuildList = engine.dirtyTasks.iterator().filterMap { it =>
        match (it) {
            case Compile(fullName, _, _, _, _, _, _) => fullName
            case _ => None
        }
    } |> collectHashSet

    if (!buildConfig.rebuildList.isEmpty()) {
        buildConfig.isRebuild = true
    }

    // TODO: actual disable
    let result = if (buildConfig.isIncremental) {
        engine.execute()
    } else {
        engine.executeClean()
    }

    return match(result) {
        case Rebuild | Cached => true
        case Fatal => false
    }

    // TODO: figure stuff out
    // let (downstreamTasks, scs) = getDownstreamTasksAndSynCounters(buildConfig, taskMap, orderedTasks)
}

// This functions creates an list of files, expected to be produced after compiler call
// Then different kinds of checks are performed over each file to choose whether to:
// - run task, which calls compiler
// - reuse artifacts from previous run
private func createArtifactGroup(compileTask: CompileTask, buildConfig: BuildConfig): ArtifactGroup {
    ArtifactGroup(
    match (compileTask.outputType) {
        case Static =>
            let name = if (compileTask.isLto) {
                makeLtoName(compileTask.filename)
            } else {
                makeCangjieStaticlibName(compileTask.filename)
            }
            [
                Artifact(Path(compileTask.targetPath).join(name).toString(),
                    CheckExists),
                Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                    CheckTimestamp)
            ]
        case Dynamic =>
            if (compileTask.isMacro) {[
                Artifact(Path(compileTask.targetPath).join(makeTargetMacroName(compileTask.filename, compileTask.target)).toString(),
                    CheckTimestamp),
                Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                    CheckTimestamp)
            ]} else {[
                Artifact(Path(compileTask.targetPath).join(makeTargetDylibName(compileTask.filename, compileTask.target)).toString(),
                    CheckExists),
                Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                    CheckTimestamp)
            ]}
        case Exe => [
            Artifact(Path(compileTask.targetPath).join(makeTargetExeName(compileTask.filename, compileTask.target)).toString(),
                CheckExists),
            Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                CheckTimestamp)
        ]
        case Chir => [
            Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.chir").toString(),
                CheckTimestamp),
            Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                CheckTimestamp)
        ]
        // TODO: case obj (is *.o crossplatform?)
        case Obj => [
            Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.o").toString(),
                CheckTimestamp),
            Artifact(Path(compileTask.targetPath).join("${swapOrgName(compileTask.fullName)}.cjo").toString(),
                CheckTimestamp)
        ]
        case Unknown(_) => []
    })
}

// Create task which computes the hash of source files in the directory without `*_test.cj` files
private func registerReadSourceTask(engine: IncrementalEngine, compileTask: CompileTask, buildConfig: BuildConfig): Unit {
    let id = TaskId.ReadPackageSource(compileTask.fullName, compileTask.sourceSetDir)
    if (!engine.taskStorage.contains(id)) {
        engine.taskGraphBuilder.add(id, incoming: [], data: ())
        let out = Task(id, engine.cache) { =>
            return PackageSource(Path(compileTask.packagePath))
        }
        engine.taskStorage.add(id, out)
    }
}

// TODO: consider removing to avoid issues with test framework
// Create task which stores the hash code of compiler options, which are not affecting the stored location of the binary
private func registerReadCompilerOptionsTask(engine: IncrementalEngine, compileTask: CompileTask, buildConfig: BuildConfig): Unit {
    let id = TaskId.ReadCompilerOptions(compileTask.fullName, compileTask.target, compileTask.isDebug, compileTask.mockSupported, compileTask.sourceSetDir)
    engine.taskGraphBuilder.add(id, incoming: [], data: ())
    let out = Task(id, engine.cache) { =>
        let res = ArrayList<String>()
        res.add(all: appendCommandOptions(compileTask))
        for (option in compileTask.customizedOption) {
            res.add(all: extractOptionByString(option))
        }
        res.add(all: extractOptionByString(compileTask.compileOption))
        res.add(all: extractOptionByString(compileTask.overrideOption))
        res.add(all: appendLinkOption(compileTask))
        res.add(all: getTypeCommand(compileTask))
        CompilerOptions.fromString(res |> collectString(delimiter: " "))
    }

    engine.taskStorage.add(id, out)
}

// Create task of compiler call, which returns the locations of expected to be produced artifacts
private func registerCompilerCallTask(engine: IncrementalEngine, compileTask: CompileTask, buildConfig: BuildConfig) {
    let id = compileTask.id
    engine.taskGraphBuilder.add(id, incoming: compileTask.requireTasks.toArray(), data: ())
    let artifacts = createArtifactGroup(compileTask, buildConfig)

    var out = Task(id, engine.cache) { =>
        if (!createDirectory(compileTask.targetPath)) {
            throw Exception("Failed to create directory: `${compileTask.targetPath}`")
        }
        if (!createDirectory(compileTask.logPath)) {
            throw Exception("Failed to create directory: `${compileTask.logPath}`")
        }
        let cjcCall = constructCjcInvocation(compileTask, buildConfig)
        let envBuilder = EnvironmentBuilder()
        envBuilder.prepend(LD_PATH, buildConfig.globalConfig.ldPath)
        if (!callCompiler(compileTask, cjcCall, envBuilder, buildConfig)) {
            throw Exception("Failed to run task: `${compileTask.id}`")
        }
        return artifacts
    }
    out.logFiles = (compileTask.outLogFile, compileTask.errLogFile)
    engine.taskStorage.add(id, out)
}

// TASK OUTPUTS
private struct PackageSource <: Serializable<PackageSource> {
    PackageSource(
        let dirpath: Path
    ) { }

    public func serialize(): DataModel {
        dirpath.toString().serialize()
    }

    public static func deserialize(dm: DataModel): PackageSource {
        let dmstr = dm as DataModelString ?? throw DataModelException("this data is not DataModelString")
        PackageSource(Path(dmstr.getValue()))
    }
}
extend PackageSource <: SerializableIncData<PackageSource> {
    public func incHash(): ?Hash {
        if (exists(dirpath) && FileInfo(canonicalize(dirpath)).isDirectory()) {
            return Directory.readFrom(dirpath).iterator()
                .filter { it => it.isRegular() &&
                    it.name.endsWith(".cj") &&
                    !it.name.endsWith("_test.cj")
                }
                .map { it => it.lastModificationTime } |>
                unorderedHash
        }
        return None
    }
}

private struct CompilerOptions <: Serializable<CompilerOptions> {
    CompilerOptions(
        let optionsHash: Hash
    ) {}
    public func serialize(): DataModel {
        optionsHash.serialize()
    }

    public static func deserialize(dm: DataModel): CompilerOptions {
        let dmint = dm as DataModelInt ?? throw DataModelException("this data is not DataModelInt")
        CompilerOptions(dmint.getValue())
    }

    static func fromString(options: String): CompilerOptions {
        CompilerOptions(options.hashCode())
    }
}

extend CompilerOptions <: SerializableIncData<CompilerOptions> {
    public func incHash(): ?Hash {
        return optionsHash
    }
}

private enum ArtifactCheckKind <: ToString & Serializable<ArtifactCheckKind> {
    | CheckExists
    | CheckTimestamp

    public func toString(): String {
        match (this) {
            case CheckExists => "check-exists"
            case CheckTimestamp => "check-timestamp"
        }
    }

    public func serialize(): DataModel {
        DataModelString(this.toString())
    }

    static func fromString(value: String): ArtifactCheckKind {
        match (value) {
            case "check-exists" => CheckExists
            case "check-timestamp" => CheckTimestamp
            case _ => throw Exception("Unknown kind of aftifact checking: `${value}`")
        }
    }

    public static func deserialize(dm: DataModel): ArtifactCheckKind {
        let dmstr = match (dm) {
            case dmstr: DataModelString => dmstr
            case _ => throw DataModelException("this is not DataModelString")
        }
        fromString(dmstr.getValue())
    }
}

@Derive[ToString]
private struct Artifact <: Serializable<Artifact> {
    private static let PATH_FIELD = "path"
    private static let CHECK_KIND_FIELD = "checkKind"
    Artifact(
        let path: String,
        let checkKind: ArtifactCheckKind
    ) { }

    public func serialize(): DataModel {
        DataModelStruct()
            .add(field(PATH_FIELD, path))
            .add(field(CHECK_KIND_FIELD, checkKind))
    }

    public static func deserialize(dm: DataModel): Artifact {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw DataModelException("this data is not DataModelStruct")
        }
        let path = String.deserialize(dms.get(PATH_FIELD))
        let checkKind = ArtifactCheckKind.deserialize(dms.get(CHECK_KIND_FIELD))
        Artifact(path, checkKind)
    }
}

extend Artifact <: SerializableIncData<Artifact> {
    public func incHash(): ?Hash {
        match (checkKind) {
            case CheckExists =>
                if (!exists(path)) { return None } else { return 0 }
            case CheckTimestamp =>
                if (!exists(path)) {
                    return None
                }
                try {
                    FileInfo(canonicalize(path)).lastModificationTime.hashCode()
                } catch (_: Exception) {
                    return None
                }
        }
    }
}

@Derive[ToString]
private struct ArtifactGroup <: Serializable<ArtifactGroup> {
    ArtifactGroup(
        let artifacts: Array<Artifact>
    ) { }

    public func serialize(): DataModel {
        artifacts.serialize()
    }

    public static func deserialize(dm: DataModel): ArtifactGroup {
        let dmseq = dm as DataModelSeq ?? throw DataModelException("this data is not DataModelSeq")
        ArtifactGroup(Array<Artifact>.deserialize(dmseq))
    }
}

extend ArtifactGroup <: SerializableIncData<ArtifactGroup> {
    public func incHash(): ?Hash {
        let size = artifacts.size
        let checked = artifacts.filterMap { it => it.incHash() } |> collectArray

        if (checked.size < size) {
            return None
        }

        checked |> unorderedHash
    }
}

@Derive[Hashable, Equatable]
public enum TaskId <: Serializable<TaskId> & ToString {
    // fullname, sourceSetDir
    | ReadPackageSource(String, String)
    // fullname, target, debug, mock, sourceSetDir
    // TODO: consired adding compile phase
    | ReadCompilerOptions(String, String, Bool, Bool, String)
    // fullName, target, debug, mock, outputType, compilePhase, sourceSetDir
    | Compile(String, String, Bool, Bool, OutputType, CompilePhase, String)
    | BuildScriptCompile
    | Unknown(String)

    public func toString(): String {
        match (this) {
            case ReadPackageSource(fullName, sourceSetDir) => "read~${fullName}:${sourceSetDir}"
            case ReadCompilerOptions(fullName, target, debug, mock, sourceSetDir) => "options~${fullName}:${target}:${debug}:${mock}:${sourceSetDir}"
            case Compile(fullName, target, debug, mock, outputType, sourceSetDir, compilePhase) => "compile~${fullName}:${target}:${debug}:${mock}:${outputType}:${compilePhase}:${sourceSetDir}"
            case BuildScriptCompile => "build-script~"
            case Unknown(s) => s
        }
    }

    public func serialize(): DataModel {
        DataModelString(this.toString())
    }

    static func fromString(value: String): TaskId {
        let splitted = value.split("~", 2)
        let values = splitted[1].split(":")
        match (splitted[0]) {
            case "read" => TaskId.ReadPackageSource(values[0], values.get(1) ?? "")
            case "options" =>
                TaskId.ReadCompilerOptions(
                    values[0],
                    values[1],
                    Bool.parse(values[2]),
                    Bool.parse(values[3]),
                    values.get(4) ?? "")
            case "compile" =>
                TaskId.Compile(
                    values[0],
                    values[1],
                    Bool.parse(values[2]),
                    Bool.parse(values[3]),
                    OutputType.fromString(values[4]),
                    CompilePhase.fromString(values[5]),
                    values.get(6) ?? ""
                )
            case "build-script" => TaskId.BuildScriptCompile
            case _ => Unknown(value)
         }
    }

    public static func deserialize(dm: DataModel): TaskId {
        let dmstr = match (dm) {
            case dmstr: DataModelString => dmstr
            case _ => throw DataModelException("this data is not DataModelString")
        }
        fromString(dmstr.getValue())
    }
}

// TASKS
sealed abstract class ErasedTask {
    var _value: Option<IncData> = None
    var logFiles: ?(String, String) = None

    ErasedTask(
        let id: TaskId
    ) { }

    protected func compute(): Unit
    protected func cleanCompute(): Unit
    protected func asDataModel(): DataModel
    protected func tryComputedHash(): ?Hash
    protected func computedHash(): Hash
    protected func readValidCachedValue(): Bool
}

class Task<T> <: ErasedTask where T <: SerializableIncData<T> {
    private let computation: Option<() -> T>
    private let cache: IncrementalCache

    init(id: TaskId, cache: IncrementalCache, comp: () -> T) {
        super(id)
        this._value = None
        this.computation = comp
        this.cache = cache
    }

    mut prop value: Option<T> {
        get() {
            if (let Some(incData) <- _value) {
                match (incData) {
                    case v: T => v
                    case _ => throw DataMistypeException(id)
                }
            } else {
                Option<T>.None
            }
        }
        set(newval) {
            if (let Some(v) <- newval) {
                this._value = v
            } else { this._value = None }
        }
    }

    protected func tryComputedHash(): ?Hash {
        if (let Some(res) <- value) {
            let hash = try { res.incHash() } catch (_: Exception) { throw NotComputedInputException(id) }
            return hash
        } else { throw NotComputedInputException(id) }
    }

    protected func computedHash(): Hash {
        tryComputedHash() ?? throw MissingArtifactException(id)
    }

    protected func asDataModel(): DataModel {
        if (let Some(res) <- value) {
            let dm = try { res.serialize() } catch (_: Exception) { throw NotComputedInputException(id) }
            return dm
        } else { throw NotComputedInputException(id) }
    }

    protected func compute(): Unit {
        if (this.value.isNone()) {
            cleanCompute()
        }
    }

    protected func cleanCompute(): Unit {
        if (let Some(comp) <- computation) {
            this.value = try {
                comp()
            } catch (e: Exception) {
                throw TaskComputeException(id, e)
            }
        }
    }

    protected func readValidCachedValue(): Bool {
        if (let Some(dm) <- cache.readValue(id)) {
            try {
                let result = T.deserialize(dm)
                if (let Some(storedHash) <- cache.readOutputHash(id) && result.incHash() == storedHash) {
                    this.value = result
                    return true
                }
            } catch(e: Exception) { }
        }
        return false
    }
}

// INCREMENTAL ENGINE
class IncrementalEngine {
    let taskGraphBuilder = DAG<TaskId, Unit>.builder()
    let taskStorage = HashMap<TaskId, ErasedTask>()
    let dirtyTasks = HashSet<TaskId>()
    let cacheLocation: Path
    let cache: IncrementalCache

    private var finalized = false

    IncrementalEngine(
        cacheLocation!: Path,
    ) {
        this.cacheLocation = cacheLocation
        this.cache = IncrementalCache(cacheLocation)
    }

    func checkFinalized(): Unit {
        if (finalized) {
            throw FinalizedBuildException()
        }
    }

    func askAll(): Unit {
        askHelper(None)
    }

    func ask(taskId: TaskId): Unit {
        askHelper(taskId)
    }

    private func askHelper(taskId: ?TaskId) {
        checkFinalized()

        let graph = if (let Some(taskId) <- taskId) {
            taskGraphBuilder.finish().incomingClosure(taskId)
        } else {
            taskGraphBuilder.finish()
        }

        for (id in graph.roots where !dirtyTasks.contains(id)) {
            let input = taskStorage.get(id) ?? throw TaskNotFoundException(id)
            input.compute()
            if (let newHash <- input.computedHash() && cache.readOutputHash(id) != newHash) {
                cache.update(id, 0, newHash, input.asDataModel())
            }
        }

        for (id in graph.toposort where !dirtyTasks.contains(id)) {
            var isClean = true
            let dependencies = graph.incoming(id)
            isClean &&= dependencies |> all { it => !dirtyTasks.contains(it) }
            isClean &&= { =>
                let cachedDeps = dependencies |> filterMap { it => cache.readOutputHash(it) } |> collectArray
                if (dependencies.size != cachedDeps.size) { return false }
                let currentDepsHash = cachedDeps.iterator() |> unorderedHash
                let storedDepsHash  = cache.readInputHash(id)
                storedDepsHash == currentDepsHash }()
            isClean &&= taskStorage[id].readValidCachedValue()

            if (!isClean) {
                dirtyTasks.add(id)
            }
        }

    }

    // `dirtyGraph` - a subgraph of `graph` of tasks to be executed
    // `graph` - a whole graph of tasks. Provided for proper incrementality and cache updates
    private func executeSubgraph(dirtyGraph: DAG<TaskId, Unit>, graph: DAG<TaskId, Unit>): TaskResult {
        checkFinalized()

        let rebuilder = Rebuilder(
            taskStorage: taskStorage,
            dirtyGraph: dirtyGraph,
            taskGraph: graph,
            cache: cache)
        let result = rebuilder.rebuild()

        cache.storeCache()

        finalized = true
        return result
    }

    func execute(): TaskResult {
        let graph = taskGraphBuilder.finish()
        let dirtyGraph = graph.filterGraph { it: TaskId, _ => dirtyTasks.contains(it) }
        executeSubgraph(dirtyGraph, graph)
    }

    func executeClean(): TaskResult {
        let graph = taskGraphBuilder.finish()
        executeSubgraph(graph, graph)
    }
}

// Returns an array of compiler calls, which are required to compile a package
func createPackageCompileTasks(r: ResolveItem, buildConfig: BuildConfig, productOutputTypes: HashMap<String, OutputType>): ArrayList<CompileTask> {
    // Common to all tasks, but needs to be embedded in CompileTask struct
    let isDebug = buildConfig.isDebug
    let isCov = buildConfig.isCov
    let requiredForTests = buildConfig.requiredForTests
    let mockSupported = buildConfig.mockSupported

    // Common to all tasks, needed to compile the package
    let fullName = r.fullName
    let rootPkgName = r.rootPkgName
    let isMultiplatform = r.packagePath.isMultiplatform
    let customizedOption = constructCustomizedOption(r, buildConfig)
    let hasSubPkgs = buildConfig.hasSubPkgs.contains(fullName)
    let exportForTests = requiredForTests && r.hasTestFiles
    let isMacro = buildConfig.packageList.isMacro(fullName)
    let isAnalysisCompilePerformance = !isMultiplatform && buildConfig.globalConfig.isAnalysisCompilePerformance

    let allEnabledFeatures = r.featureDeducer
        .addFeature(r.features)
        .collect()
    let featureMapping = r.featureDeducer.cleanFeatures() // leaving only mapping to apply them to source set features

    let productSuffixes = buildConfig.packageList.productSuffixes

    // Specific to the `--target` platform
    // `isNativeForCross == true` - package needs to be compiled with `host` target to be used by or is a `macro package` 
    func createForTarget(isNativeForCross!: Bool = false): ArrayList<CompileTask> {
        let targetDir = if (isNativeForCross) { buildConfig.globalConfig.nativeDir } else { buildConfig.globalConfig.targetDir }

        let isCrossCompile = buildConfig.isCrossCompile && !isNativeForCross

        let target = if (isCrossCompile) {
            crossCompileTarget
        } else { targetConfigName }

        let compileOption = if (isNativeForCross) {
            r.nativeCompileOption
        } else { r.compileOption }

        let overrideOption = if (isNativeForCross) {
            buildConfig.globalConfig.nativeOverrideOption
        } else { buildConfig.globalConfig.overrideCompileOption }

        let linkOption = if (isNativeForCross) {
            r.nativeLinkOption
        } else { r.linkOption }

        let sourceSetDAG = r.packagePath.sourceSets
        let srcDirToTaskId = HashMap<String, TaskId>()
        let srcDirToTargetPath = HashMap<String, String>()

        // Multiplatform packages need an arbitrary number of `cjc` calls
        // Non-multiplatform packages consist of single `sourceSetEntry`
        func createForSingleSourceSet(sourceSetEntry: CJMPPackageInfo): ArrayList<CompileTask> {
            let packagePath = sourceSetEntry.srcDir.toString()
            let product = sourceSetEntry.product
            let sourceSetDir = sourceSetEntry.outputSuffix.toString()
            let sourceSetFeatures = sourceSetEntry.features

            let subPkgSet = r.superPkgCfg?.subPkgSet ?? HashSet<String>()
            let isCombined = product && r.superPkgCfg.isSome() && !isNativeForCross
            let isProjectCombined =
                product &&
                buildConfig.globalConfig.isProjectCombine &&
                r.fullName == buildConfig.globalConfig.rootName &&
                !isNativeForCross

            let supressed = ArrayList<String>()

            let outputType = if (!product) {
                OutputType.Chir
            } else if (r.outputType == Exe && requiredForTests) {
                // to be included in tests as a dependency, the package should be compiled as a library anyway
                // unused main function related warnings should also be suppressed in this scenario
                supressed.add("-Woff=unused-main")
                OutputType.Static
            } else { r.outputType }

            let isLto = buildConfig.isLto && (outputType == Static || outputType == Exe)
            let ltoValue = if (isLto) { buildConfig.ltoValue } else { "" }

            let isSplittable = !isMultiplatform &&
                !isLto &&
                product &&
                buildConfig.isCompilePipelineParallel &&
                !isMacro &&
                (outputType == Dynamic || outputType == Exe)
 
            let compilePhase = if (isSplittable) { ObjOnly } else { CompilePhase.Default }

            let taskId = TaskId.Compile(fullName, target, isDebug, mockSupported, outputType, compilePhase, sourceSetDir)

            let filename = if (outputType == Exe && buildConfig.packageList.exe.size <= 1 && !COMMON_INFO.inWorkspace) {
                buildConfig.exeName
            } else { fullName }

            let targetPath = if (outputType == Exe) {
                Path(targetDir).join(BIN).join(sourceSetDir).toString()
            } else {
                Path(targetDir).join(swapOrgName(rootPkgName)).join(sourceSetDir).toString()
            }
            let logPath = Path(targetDir).join(".build-logs").join(swapOrgName(rootPkgName)).join(sourceSetDir).toString()

            // TODO: move
            // if (!createDirectory(targetPath) || !createDirectory(logPath)) {
            //     return None
            // }

            // TODO: explanation
            let requireTasksIterator = 
                if (buildConfig.globalConfig.isProjectCombine && fullName == buildConfig.globalConfig.rootName) {
                    productOutputTypes.keys().iterator()
                } else {
                    var result = r.requires.iterator()
                    result = result |> concat(subPkgSet)
                    result
                }

            // Requires is not only the compilation tasks. It's the names of all dependencies
            // We need to leave only packages we're creating compilation tasks for

            // TODO: need to decide here some things:
            // - if we're using `isCompilePipelineParallel` add `cjo` monitor to product tasks we depend on
            // - whether the task is splittable and if so, create 2 of them
            // - if we're requested to define monitor, we should attach one to the earlies `cjo` returning task
            // - if we need `cjo` monitor, then add direct dependency to the earlier task `ObjOnly vs Default`
            let requireTasks = requireTasksIterator
                .filter { it => productOutputTypes.contains(it) }
                .map { name =>
                    let isMacroName = buildConfig.packageList.isMacro(name)
                    let waitTarget = if (isMacroName) { 
                        targetConfigName
                    } else { target }
                    let isSplittable = !isMultiplatform &&
                        !isLto &&
                        buildConfig.isCompilePipelineParallel &&
                        !isMacroName &&
                        (buildConfig.packageList.isDyLib(name) || buildConfig.packageList.isExe(name))
                    let compilePhase = if (isSplittable) { ObjOnly } else { CompilePhase.Default }
                    // TODO: compilePhase: compilePhase
                    TaskId.Compile(
                        name, waitTarget, isDebug, mockSupported,
                        productOutputTypes[name], compilePhase, productSuffixes.get(name) ?? ""
                    )
                } |> collectHashSet
            requireTasks.add(TaskId.ReadPackageSource(fullName, sourceSetDir))
            requireTasks.add(TaskId.ReadCompilerOptions(fullName, target, isDebug, mockSupported, sourceSetDir))

            let (outLogFile, errLogFile) = getLogFile(logPath, taskId)

            srcDirToTaskId.add(sourceSetEntry.sourceSetDir.toString(), taskId)
            srcDirToTargetPath.add(sourceSetEntry.sourceSetDir.toString(), targetPath)

            let previousSourceSets = sourceSetDAG.incoming(sourceSetEntry.sourceSetDir.toString())
                .map { it => srcDirToTaskId[it] }
            requireTasks.add(all: previousSourceSets)
            let prevStageArtifacts: Array<(String, String)> = sourceSetDAG.incoming(sourceSetEntry.sourceSetDir.toString())
                .map { it => srcDirToTargetPath[it] }
                .map { it => (Path(it).join("${fullName}.chir").toString(), Path(it).join("${fullName}.cjo").toString()) }

            let task = CompileTask(
                id: taskId,
                targetDir: targetDir,
                targetPath: targetPath,
                packagePath: packagePath,
                product: product,
                sourceSetDir: sourceSetDir,
                sourceSetFeatures: sourceSetFeatures,
                rootPkgName: rootPkgName,
                fullName: fullName,
                filename: filename,
                outputType: if (isSplittable) { Obj } else { outputType },
                target: target,
                isCrossCompile: isCrossCompile,
                compileOption: compileOption,
                overrideOption: overrideOption,
                linkOption: linkOption,
                customizedOption: customizedOption,
                subPkgSet: subPkgSet,
                isCombined: isCombined,
                isProjectCombined: isProjectCombined,
                requiredForTests: requiredForTests,
                exportForTests: exportForTests,
                requireTasks: requireTasks,
                allEnabledFeatures: allEnabledFeatures,
                featureMapping: featureMapping,
                isAnalysisCompilePerformance: isAnalysisCompilePerformance,
                hasSubPkgs: hasSubPkgs,
                isDebug: isDebug,
                isCov: isCov,
                mockSupported: mockSupported,
                isMacro: isMacro,
                isLto: isLto,
                ltoValue: ltoValue,
                prevStageArtifacts: prevStageArtifacts,
                isMultiplatform: isMultiplatform,
                supressed: supressed,
                logPath: logPath,
                outLogFile: outLogFile,
                errLogFile: errLogFile,
                compileTargetOutput: if (isSplittable) { outputType } else { None },
                compilePhase: compilePhase
            )

            let compileTasks = ArrayList<CompileTask>([task])
 	 
            // TODO: simplify it. (`copy` function?)
            if (isSplittable) {
                let taskId = TaskId.Compile(fullName, target, isDebug, mockSupported, outputType, LinkOnly, sourceSetDir)
                let prevStageObj = Path(task.targetPath).join(getOutput(task).getOrThrow()).toString()
                let requireTasks = getDepSet(fullName, buildConfig).iterator().filterMap { name: String =>
                        if(!buildConfig.packageList.isMacro(name) && !buildConfig.packageList.isTest(name)) {
                            let isSplittable = buildConfig.packageList.isDyLib(name) || buildConfig.packageList.isExe(name)
                            let compilePhase = if (isSplittable) { LinkOnly } else { CompilePhase.Default }
                            TaskId.Compile(name, target, isDebug, mockSupported, productOutputTypes[name], compilePhase, productSuffixes.get(name) ?? "")
                        } else {
                            None
                        }
                    } |> collectHashSet
                requireTasks.add(task.id)
                let (outLogFile, errLogFile) = getLogFile(logPath, taskId)
                let linkTask = CompileTask(
                    id: taskId,
                    targetDir: targetDir,
                    targetPath: targetPath,
                    packagePath: packagePath,
                    product: product,
                    sourceSetDir: sourceSetDir,
                    sourceSetFeatures: sourceSetFeatures,
                    rootPkgName: rootPkgName,
                    fullName: fullName,
                    filename: filename,
                    outputType: outputType,
                    target: target,
                    isCrossCompile: isCrossCompile,
                    compileOption: compileOption,
                    overrideOption: overrideOption,
                    linkOption: linkOption,
                    customizedOption: customizedOption,
                    subPkgSet: subPkgSet,
                    isCombined: isCombined,
                    isProjectCombined: isProjectCombined,
                    requiredForTests: requiredForTests,
                    exportForTests: exportForTests,
                    requireTasks: requireTasks,
                    allEnabledFeatures: allEnabledFeatures,
                    featureMapping: featureMapping,
                    isAnalysisCompilePerformance: isAnalysisCompilePerformance,
                    hasSubPkgs: hasSubPkgs,
                    isDebug: isDebug,
                    isCov: isCov,
                    mockSupported: mockSupported,
                    isMacro: isMacro,
                    isLto: isLto,
                    ltoValue: ltoValue,
                    isMultiplatform: isMultiplatform,
                    supressed: supressed,
                    logPath: logPath,
                    outLogFile: outLogFile,
                    errLogFile: errLogFile,
                    prevStageObj: prevStageObj,
                    compilePhase: LinkOnly
                )
                compileTasks.add(linkTask)
            }
            compileTasks
        }

        let sourceSetUnwrap = ArrayList<CompileTask>()
        for (sourceSetEntry in r.packagePath._sources) {
            let tasks = createForSingleSourceSet(sourceSetEntry)
            sourceSetUnwrap.add(all: tasks)
        }
        sourceSetUnwrap
    }

    let result = ArrayList<CompileTask>()

    assertion { r.targetPlatform || r.nativePlatform }
    if (r.nativePlatform) {
        result.add(all: createForTarget(isNativeForCross: true))
    }
    if (r.targetPlatform) {
        result.add(all: createForTarget(isNativeForCross: false))
    }
    return result
}

private func constructCustomizedOption(r: ResolveItem, buildConfig: BuildConfig): ArrayList<String> {
    let customizedOption = ArrayList<String>()
    for (k in buildConfig.customizedOption) {
        if (let Some(option) <- r.customizedOption.get(k)) {
            customizedOption.add(option)
        }
    }
    return customizedOption
}

// REBUILDER
@Derive[Equatable, ToString]
enum TaskResult {
    | Cached
    | Rebuild
    | Fatal

    operator func +(b: TaskResult): TaskResult {
        match((this, b)) {
            case (Fatal, _) => Fatal
            case (_, Fatal) => Fatal
            case (Rebuild, _) => Rebuild
            case (_, Rebuild) => Rebuild
            case (Cached, Cached) => Cached
        }
    }
}

extend Array<TaskResult> {
    prop cached: Bool {
        get() {
            this.iterator().all { it => it == Cached }
        }
    }

    prop fatal: Bool {
        get() {
            this.iterator().any { it => it == Fatal }
        }
    }
}

class Rebuilder {
    private let futureMap: HashMap<TaskId, Future<TaskResult>>
    private let barrier: Barrier
    private let logger: BoundedFileLogger

    // TODO: merge taskStorage and graph?   
    Rebuilder(
        private let taskStorage!: ReadOnlyMap<TaskId, ErasedTask>,
        private let dirtyGraph!: DAG<TaskId, Unit>,
        private let taskGraph!: DAG<TaskId, Unit>,
        private let cache!: IncrementalCache,
    ) {
        let tasksCount = dirtyGraph.toposort.size
        this.futureMap = HashMap<TaskId, Future<TaskResult>>(tasksCount)
        this.barrier = Barrier(tasksCount + 1)
        this.logger = BoundedFileLogger(tasksCount)
    }

    func rebuild(): TaskResult {
        logger.start()

        for (id in dirtyGraph.toposort) {
            let task = taskStorage[id]
            let future = spawn { =>
                barrier.wait()
                deleteLog(task.logFiles)

                let result = executeSingleTask(task)

                logger.log(task.logFiles)
                result
            }
            futureMap.add(id, future)
        }

        // Starting all task simultaneously
        barrier.wait()

        var result = Cached
        for (future in futureMap.values()) {
            result += future.get()
        }
        if (!logger.unsafeWait()) {
            result = Fatal
        }
        return result
    }

    private func executeSingleTask(task: ErasedTask): TaskResult {
        let id = task.id
        let dependenciesResult = dirtyGraph.incoming(id)
            .map { it => futureMap[it].get() }

        if (dependenciesResult.fatal) {
            return Fatal
        }

        let dependencies = taskGraph.incoming(id)

        try {
            // Early cutoff
            if (dependenciesResult.cached) {
                if (canCutoffEarly(id, dependencies)) {
                    return Cached
                }
            }

            task.cleanCompute()
            let computedHash = if (let Some(computedHash) <- task.tryComputedHash()) {
                computedHash
            } else {
                return Rebuild
            }
            let newInputCache = dependencies |> filterMap { it: TaskId => cache.readOutputHash(it) } |> unorderedHash

            if (let Some(hash) <- cache.readOutputHash(id) && computedHash == hash) {
                // Early cutoff
                cache.update(id, newInputCache, computedHash, task.asDataModel())
                return Cached
            } else {
                cache.update(id, newInputCache, computedHash, task.asDataModel())
                return Rebuild
            }
        } catch (e: IncrementalEngineException) {
            return Fatal
        }
    }

    private func canCutoffEarly(id: TaskId, dependencies: Array<TaskId>): Bool {
        let cachedDeps = dependencies |> filterMap { it => cache.readOutputHash(it) } |> collectArray
        // Not all dependencies are cached
        if (dependencies.size != cachedDeps.size) { return false }
        let currentDepsHash = cachedDeps.iterator() |> unorderedHash
        let storedDepsHash = cache.readInputHash(id)
        // Task was computed with different tasks or different states
        if (storedDepsHash != currentDepsHash) {
            return false
        }
        // The value stored in cache is invalid
        if (!taskStorage[id].readValidCachedValue()) {
            return false
        }
        return true
    }
}

// CACHING

// Non-cacheable data (e.g. root inputs, always computed)
interface IncData {
    // case None  - Although, value is known, the current state is invalid and the task should be recomputed
    // case value - The value is valid, and can be reused
    func incHash(): ?Hash
}

// Cacheable data
interface SerializableIncData<T> <: IncData where T <: Serializable<T> {
    // case None  - Although, value is known, the current state is invalid and the task should be recomputed
    // case value - The value is valid, and can be reused
    func incHash(): ?Hash
}

private class CacheItem <: Serializable<CacheItem> {
    private static let VALUE = "value"
    private static let INPUT_HASH = "inputHash"
    private static let OUTPUT_HASH = "outputHash"

    CacheItem(
        let value: DataModel,
        let inputHash: Hash,
        let outputHash: Hash
    ) { }

    public func serialize(): DataModel {
        DataModelStruct()
            .add(Field(VALUE, value))
            .add(field(INPUT_HASH, inputHash))
            .add(field(OUTPUT_HASH, outputHash))
    }

    public static func deserialize(dm: DataModel): CacheItem {
        let dms = match (dm) {
            case dms: DataModelStruct => dms
            case _ => throw DataModelException("this data is not DataModelStruct")
        }
        let value = dms.get(VALUE)
        let inputHash = Hash.deserialize(dms.get(INPUT_HASH))
        let outputHash = Hash.deserialize(dms.get(OUTPUT_HASH))
        CacheItem(value, inputHash, outputHash)
    }
}

class IncrementalCache {
    private let cache: ConcurrentHashMap<TaskId, CacheItem>

    IncrementalCache(
        private let cacheLocation: Path
    ) {
        this.cache = readCacheFromJson(cacheLocation)
    }

    func update(id: TaskId, inputHash: Hash, outputHash: Hash, dm: DataModel): Unit {
        cache.add(id, CacheItem(dm, inputHash, outputHash))
    }

    func readValue(id: TaskId): ?DataModel {
        cache.get(id)?.value
    }

    func readInputHash(id: TaskId): ?Hash {
        cache.get(id)?.inputHash
    }

    func readOutputHash(id: TaskId): ?Hash {
        cache.get(id)?.outputHash
    }

    private static func readCacheFromJson(cacheLocation: Path): ConcurrentHashMap<TaskId, CacheItem> {
        let dm = readJsonFileOrEmpty(cacheLocation)
        try {
            // stdx.serialization.serialization doesn't work properly with non-string keys
            let hashmap = HashMap<String, CacheItem>.deserialize(dm)
                .iterator().map {it => (TaskId.fromString(it[0]), it[1])} |>
                collectHashMap
            ConcurrentHashMap(hashmap)
        } catch (e: Exception) {
            ConcurrentHashMap()
        }
    }

    func storeCache(): Unit {
        writeJsonToFile(cacheLocation, cache)
    }
}

private func readJsonFileOrEmpty(path: Path): DataModel {
    try {
        let barray = File.readFrom(path)
        let s = String.fromUtf8(barray)
        let json = JsonValue.fromStr(s)
        let res = DataModel.fromJson(json)
        return res
    } catch (e: Exception) {
        DataModel.fromJson(JsonObject())
    }
}

private func writeJsonToFile(path: Path, cache: ConcurrentHashMap<TaskId, CacheItem>): Unit {
    try {
        // stdx.serialization.serialization doesn't work properly with non-string keys
        let dm = (cache |> map { it => (it[0].toString(), it[1]) } |> collectHashMap).serialize()
        let json = dm.toJson()
        let s = json.toJsonString()
        let barray = s.toArray()
        File.writeTo(path, barray)
    } catch (e: Exception) { }
}


private func callCompiler(compileTask: CompileTask, args: ArrayList<String>, envBuilder: EnvironmentBuilder, buildConfig: BuildConfig): Bool {
    var execCmdFlag: Bool = true

    // TODO: Try to remove
    // CUR_PARALLEL_SIZE.fetchAdd(1)
    // if (!futJudge(args)) {
    //     let jobs = calculateParallel(CUR_PARALLEL_SIZE.load())
    //     args.add("-j${jobs}", at: 0)
    // }

    let env = getVariables()
    let commandStr = getCmdStr(envBuilder.asCliStrings(env), COMPILE_TOOL, args)
    if (buildConfig.isVerbose) {
        let verbose: String = if (compileTask.isMultiplatform) {
            "Compiling `${compileTask.sourceSetDir}` part of package `${compileTask.fullName}`: ${commandStr}\n"
        } else { 
            "Compiling package `${compileTask.fullName}`: ${commandStr}\n"
        }
        // TODO: make sure it's not failing after Liu Zheng refactoring
        writeFile(compileTask.outLogFile, verbose, Append)
    }

    if (compileTask.isAnalysisCompilePerformance) {
        addStartTime(compileTask.fullName, commandStr)
    }

    let outFilePath = compileTask.outLogFile
    let errFilePath = compileTask.errLogFile
    let (outFile, errFile) = try {
        (File(outFilePath, OpenMode.Append), File(errFilePath, OpenMode.Append))
    } catch (e: Exception) {
        eprintln(e.message)
        eprintln("Error: create '${outFilePath}' failed")
        return false
    }

    try {
        if (let Some(returnCode) <- execAndToFile(COMPILE_TOOL, args, outFile, errFile, envBuilder: envBuilder, originalEnv: env)) {
            execCmdFlag = (returnCode == 0)
            if (execCmdFlag) {
                if (buildConfig.globalConfig.isAnalysisCompilePerformance) {
                    moveCjcProfToCjpmDir(compileTask, buildConfig.globalConfig.compilePerformanceTargetDir)
                }
            } else {
                let errmesg = if (compileTask.isMultiplatform) {
                    "Error: failed to compile `${compileTask.sourceSetDir}` part of package `${compileTask.fullName}`, return code is ${returnCode}\n"
                } else {
                    "Error: failed to compile package `${compileTask.fullName}`, return code is ${returnCode}\n"
                }
                errFile.write(errmesg.toArray())
            }
        } else {
            errFile.write("Error: failed to compile package `${compileTask.fullName}` with exception occurred\n".toArray())
        }
    } catch (e: Exception) {
        eprintln("Error: failed to write error log into ${errFilePath}: ${e.message}")
        execCmdFlag = false
    }
    // TODO: make sure that's working/removeable
    // CUR_PARALLEL_SIZE.fetchSub(1)
    outFile.close()
    errFile.close()

    if (buildConfig.globalConfig.isAnalysisCompilePerformance) {
        addEndTime(compileTask.fullName)
    }
    return execCmdFlag
}

// TODO: analyze changes, try to not miss anything
// class ParallelBuilder {
//     let buildConfig: BuildConfig
//     let taskMap: TaskMap
//     let downstreamTasks: DownStreamTasks
//     let scs: Task2SyncCounter
//     let orderedTasks: Array<TaskId>
//     let notifyMutex = Mutex()
//     let notified = DownStreamTasks()
//     let buildMutex = Mutex()
//     let shouldFail = HashSet<TaskId>()
// 
//     init(buildConfig: BuildConfig, taskMap: TaskMap, downstreamTasks: DownStreamTasks, scs: Task2SyncCounter, orderedTasks: Array<TaskId>) {
//         this.buildConfig = buildConfig
//         this.taskMap = taskMap
//         this.downstreamTasks = downstreamTasks
//         this.scs = scs
//         this.orderedTasks = orderedTasks
//         orderedTasks.forEach { task => notified[task] = HashSet<TaskId>() }
//     }
// 
//     func shouldNotify(downstreamTask: TaskId, task: TaskId, notification: Notification): Bool {
//         let res = match (notification) {
//             case CjoReady =>
//                 if (taskMap.get(task)?.isMacro ?? true) {
//                     false
//                 } else {
//                     match (downstreamTask.compilePhase) {
//                         case LinkOnly => false
//                         case ObjOnly => true
//                         case Default =>
//                             if (let Some(compileTask) <- taskMap.get(downstreamTask)) {
//                                 !compileTask.isLto &&
//                                 !compileTask.isMultiplatform &&
//                                 compileTask.outputType == OutputType.Static
//                             } else { false }
//                     }
//                 }
//             case _ => true
//         }
//         if (res) {
//             synchronized (notifyMutex) {
//                 if (!notified[task].add(downstreamTask)) {
//                     return false
//                 }
//             }
//         }
//         res
//     }
// 
//     func printNotification(downstreamTask: TaskId, task: TaskId, notification: Notification): Unit {
//         if (!buildConfig.isVerbose || !buildConfig.isCompilePipelineParallel) {
//             return
//         }
//         println("Task `${task}` notifies Task `${downstreamTask}` with notification `${notification.toString()}`, waiting for ${scs[downstreamTask].count.toString()} tasks")
//     }
// 
//     func notify(task: TaskId, notification: Notification): Unit {
//         for (t in downstreamTasks[task] where shouldNotify(t, task, notification)) {
//             if (let Err <- notification) {
//                 synchronized(buildMutex) {
//                     shouldFail.add(t)
//                 }
//             }
//             scs[t].dec()
//             printNotification(t, task, notification)
//         }
//     }
// 
//     func getCjo2TaskAndListenDirs(): (HashMap<String, TaskId>, HashSet<String>) {
//         let cjo2Task = HashMap<String, TaskId>()
//         let listenDirs = HashSet<String>()
//         for (taskId in orderedTasks where taskId.compilePhase != LinkOnly) {
//             let task = taskMap[taskId]
//             listenDirs.add(task.targetPath)
//             cjo2Task.add(Path(task.targetPath).join("${task.fullName}.cjo.flag").toString(), taskId)
//         }
//         (cjo2Task, listenDirs)
//     }
// 
//     func build(): (Bool, HashSet<String>) {
//         let logger = BoundedFileLogger(taskMap.size)
//         logger.start()
// 
//         let cjoFlagFiles = HashSet<String>()
//         let collectCjoFlagFiles = { file => cjoFlagFiles.add(file) }
// 
//         let (cjo2Task, listenDirs) = getCjo2TaskAndListenDirs()
// 	    let watchFSFuture = spawn { =>
//             if (buildConfig.isCompilePipelineParallel) {
//                 watchFS(cjo2Task, listenDirs, notify, collectCjoFlagFiles, buildConfig.isVerbose)
//             }
//         }
// 
//         let sem = Semaphore(maxParallelSize)
//         let futTasks = HashMap<TaskId, Future<Bool>>()
//         // Guaranteed valid order follows 'buildConfig.packageList.all'
//         for (taskId in orderedTasks) {
//             sem.acquire()
//             let task = taskMap[taskId]
//             let action = { =>
//                 synchronized(buildMutex) {
//                     if (shouldFail.contains(taskId)) {
//                         logger.release()
//                         return false
//                     }
//                 }
// 
//                 if (task.checkDepCjo) {
//                     var nochange: Bool = true
//                     for (depPkg in task.requireTasks) {
//                         if (!checkNoChange(depPkg.fullName)) {
//                             nochange = false
//                             break
//                         }
//                     }
// 
//                     for (subPkg in task.subPkgSet) {
//                         if (!checkNoChange(subPkg)) {
//                             nochange = false
//                             break
//                         }
//                     }
// 
//                     if (nochange) {
//                         logger.release()
//                         return true
//                     }
//                 }
// 
//                 if (!deleteLog(task)) {
//                     logger.release()
//                     return false
//                 }
// 
//                 let cjcCall = constructCjcInvocation(task, this.buildConfig)
//                 let envBuilder = EnvironmentBuilder()
//                 envBuilder.prepend(LD_PATH, this.buildConfig.globalConfig.ldPath)
//                 specifyCjcJobs(cjcCall, this.buildConfig.cjcJobs)
//                 let res = runTask(task, cjcCall, envBuilder) 
//                 logger.log(task.outLogFile, task.errLogFile)
//                 res
//             }
// 
//             let future = spawn { =>
//                 scs[taskId].waitUntilZero()
//                 if (buildConfig.isVerbose) {
//                     createAndWriteFile(task.outLogFile, "", mode: Append)
//                 }
//                 if (buildConfig.isVerbose && buildConfig.isCompilePipelineParallel) {
//                     writeFile(task.outLogFile, "Task `${taskId}` starts", Append)
//                 }
//                 let res = action()
//                 sem.release()
//                 let notification = if (res) {
//                     OUTPUTTYPE2NOTIFICATION.get(task.outputType.toString()).getOrThrow()
//                 } else { Err }
//                 notify(taskId, notification)
//                 if (buildConfig.isVerbose && buildConfig.isCompilePipelineParallel) {
//                     // TODO: cached ?
//                     let result = if (res) { "success" } else { "failure" }
//                     writeFile(task.outLogFile, "Task `${taskId}` ends with ${result}", Append)
//                 }
//                 res
//             }
// 
//             futTasks.add(taskId, future)
//         }
// 
//         var buildResult = true
// 
//         let successPkgs = HashSet<String>(futTasks.iterator().map { item => item[0].fullName } |> collectArray)
//         for ((id, task) in futTasks) {
//             if (!task.get()) {
//                 successPkgs.remove(id.fullName)
//                 buildResult = false
//             }
//         }
// 
//         if (!logger.unsafeWait()) {
//             buildResult = false
//         }
// 
//         if (buildConfig.isCompilePipelineParallel) {
//             stopWatchFS(buildConfig.isVerbose)
//         }
//         watchFSFuture.get()
// 
//         for (file in cjoFlagFiles) {
//             if (!deleteFile(file)) {
//                 buildResult = false
//             }
//         }
// 
//         (buildResult, successPkgs)
//     }
// }

// COMPILER PROFILING
func moveCjcProfToCjpmDir(compileTask: CompileTask, profcompilePerformancePath: String): Unit {
    let profSuffixArray: Array<String> = [".mem.prof", ".time.prof", ".info.prof"]
    for (profSuffix in profSuffixArray) {
        let pkgNameProfPath = Path(compileTask.targetPath).join(compileTask.fullName + profSuffix)
        if (exists(pkgNameProfPath)) {
            let profPath = Path(profcompilePerformancePath).join(compileTask.fullName + profSuffix)
            copy(pkgNameProfPath, to: profPath, overwrite: true)
            remove(pkgNameProfPath)
        } else {
            eprintln("Error: cannot find ${pkgNameProfPath.toString()} file")
        }
    }
    return
}

func addStartTime(fullName: String, cmdArgs: String): Unit {
    synchronized(CACHE_MUTEX) {
        SHOW_CACHE.add(PkgInfo(fullName, "B", DateTime.now().toUnixTimeStamp().toMilliseconds()))
        COMMAND_CACHE.add(CommandInfo(fullName, cmdArgs))
    }
    return
}

func addEndTime(fullName: String): Unit {
    synchronized(CACHE_MUTEX) {
        SHOW_CACHE.add(PkgInfo(fullName, "E", DateTime.now().toUnixTimeStamp().toMilliseconds()))
    }
    return
}

// PARALLELIZATION
func printDependency(taskA: TaskId, taskB: TaskId, verbose: Bool): Unit {
    if (!verbose) {
        return
    }
    println("`${taskA}` depends on `${taskB}`")
}

// TODO: incorporate
// func getDownstreamTasksAndSynCounters(buildConfig: BuildConfig, taskMap: TaskMap,
//     orderedTasks: ArrayList<TaskId>): (DownStreamTasks, Task2SyncCounter) {
//     let downstreamTasks = DownStreamTasks()
//     let scs = Task2SyncCounter()
//     for (taskId in orderedTasks where taskMap.contains(taskId)) {
//         let depTasks = HashSet<TaskId>()
//         downstreamTasks.addIfAbsent(taskId, HashSet<TaskId>())
//         if (buildConfig.globalConfig.isProjectCombine && taskId.fullName == buildConfig.globalConfig.rootName) {
//             for ((k, _) in taskMap where k != taskId && taskMap.contains(k)) {
//                 depTasks.add(k)
//                 downstreamTasks.addIfAbsent(k, HashSet<TaskId>())
//                 downstreamTasks[k].add(taskId)
//                 printDependency(taskId, k, buildConfig.isVerbose && buildConfig.isCompilePipelineParallel)
//             }
//         } else {
//             let task = taskMap[taskId]
//             for (dep in task.requireTasks where taskMap.contains(dep)) {
//                 depTasks.add(dep)
//                 downstreamTasks.addIfAbsent(dep, HashSet<TaskId>())
//                 downstreamTasks[dep].add(taskId)
//                 printDependency(taskId, dep, buildConfig.isVerbose && buildConfig.isCompilePipelineParallel)
//             }
// 
//             for (dep in task.subPkgSet) {
//                 let depId = TaskId(dep, task.target, "")
//                 if (!taskMap.contains(depId)) {
//                     continue
//                 }
//                 depTasks.add(depId)
//                 downstreamTasks.addIfAbsent(depId, HashSet<TaskId>())
//                 downstreamTasks[depId].add(taskId)
//                 printDependency(taskId, depId, buildConfig.isVerbose && buildConfig.isCompilePipelineParallel)
//             }
//         }
//         scs[taskId] = SyncCounter(depTasks.size)
//     }
//     (downstreamTasks, scs)
// }

// WHERE DID IT GO?
func specifyCjcJobs(args: ArrayList<String>, cjcParallelConfig: CjcParallelConfig): Unit {
    CUR_PARALLEL_SIZE.fetchAdd(1)

    if (futJudge(args)) {
        return
    }

    match (cjcParallelConfig) {
        case CjcParallelConfig.Default => ()
        case CjcParallelConfig.Heuristic =>
            let jobs = calculateParallel(CUR_PARALLEL_SIZE.load())
            args.add("-j${jobs}", at: 0)
        case CjcParallelConfig.Specific(v) =>
            args.add("-j${v}", at: 0)
    }
}

func calculateParallel(currentParalla: Int64): Int64 {
    return if (currentParalla <= 2) {
        maxParallelSize
    } else {
        2 * maxParallelSize / currentParalla
    }
}

func futJudge(args: ArrayList<String>): Bool {
    let tmp = args.toString()
    let fut1: Future<Bool> = spawn {
        let regexParallel = Regex("-j[[:space:]]*(?:[[:digit:]]|=[[:digit:]])")
        regexParallel.matches(tmp)
    }
    let fut2: Future<Bool> = spawn {
        tmp.contains("--jobs") || args.contains("-j")
    }
    return (fut1.get() || fut2.get())
}

// TODO. Why? Should ou remake it?
func getLogFile(dir: String, taskId: TaskId): (String, String) {
    match (taskId) {
        case Compile(fullName, target, _, _, _, compilePhase, sourceSetDir) =>
            let logFileName = "${swapOrgName(fullName)}.${target}.${sourceSetDir}.${compilePhase}".replace("/", "#").replace("\\", "#")
            let outLogFile = Path(dir).join("${logFileName}.outlog").toString()
            let errLogFile = Path(dir).join("${logFileName}.errlog").toString()
            return (outLogFile, errLogFile)
        case _ => throw Exception("TODO")
    }
}

var watcherCb: (String) -> Unit = { _ => }

@When[(os != "iOS") && (env != "ohos") && (env != "android")]
func watchFS(cjo2task: HashMap<String, TaskId>, listenDirs: HashSet<String>,
    notify: (TaskId, Notification) -> Unit, collectCjoFlagFiles: (String) -> Bool, verbose: Bool): Unit {
    let dirs = StringBuilder()
    for (dir in listenDirs) {
        if (!exists(dir)) {
            Directory.create(dir, recursive: true)
        }
        dirs.append("${dir},")
    }
    watcherCb = { cjo =>
        collectCjoFlagFiles(cjo)
        notify(cjo2task[cjo], CjoReady) 
    }
    unsafe {
        let cs = LibC.mallocCString(dirs.toString())
        let res = initFSWatcher(cs, { cjo => watcherCb(cjo.toString()) }, verbose)
        LibC.free(cs)
        if (!res) {
            return
        }
        startFSWatcher(verbose)
    }
}

@When[(os == "iOS") || (env == "ohos") || (env == "android")]
func watchFS(_: HashMap<String, TaskId>, _: HashSet<String>,
    _: (TaskId, Notification) -> Unit, _: (String) -> Bool, verbose: Bool): Unit {}

@When[(os != "iOS") && (env != "ohos") && (env != "android")]
func stopWatchFS(isVerbose: Bool): Unit {
    unsafe { stopFSWatcher(isVerbose) }
}

@When[(os == "iOS") || (env == "ohos") || (env == "android")]
func stopWatchFS(_: Bool): Unit {}

// TODO: remove redundant exceptions (intergrate `DAG<T>`)
// EXCEPTIONS
sealed abstract class IncrementalEngineException <: Exception {
    IncrementalEngineException(msg: String) { super(msg) }
}

class TaskNotFoundException <: IncrementalEngineException {
    TaskNotFoundException(id: TaskId) {
        super("Couldn't find task with id `${id}`")
    }
}

class AmbiguousTaskException <: IncrementalEngineException {
    AmbiguousTaskException(id: TaskId) {
        super("Task `${id}` is already defined")
    }
}

class FinalizedBuildException <: IncrementalEngineException {
    FinalizedBuildException() {
        super("IncrementalEngine object is finalized and cannot be changed")
    }
}

class DataMistypeException <: IncrementalEngineException {
    DataMistypeException(id: TaskId) {
        super("Task `${id}` stores the value of wrong type")
    }
}

class NotComputedInputException <: IncrementalEngineException {
    NotComputedInputException(id: TaskId) {
        super("Tried to get value from task `${id}` before calling `engine.execute()`")
    }
}

class MissingArtifactException <: IncrementalEngineException {
    MissingArtifactException(id: TaskId) {
        super("Task `${id}` was executed, but artifact didn't get produced or got missing")
    }
}

class TaskComputeException <: IncrementalEngineException {
    TaskComputeException(id: TaskId, error: Exception) {
        super("Task `${id}` threw an exception:\n${error}")
    }
}