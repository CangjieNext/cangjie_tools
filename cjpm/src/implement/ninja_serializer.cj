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
import cjpm.util.*

private enum NinjaTaskType <: ToString {
    | Compile
    | Link
    public func toString(): String {
        match (this) {
            case Compile => "Compile"
            case Link => "Link"
        }
    }
}

class NinjaPlanSerializer <: PlanSerializer {
    private let writer: NinjaWriter
    private let buildConfig: BuildConfig
    private let targetDir: String
    private let testTargetDir: String
    private let defaultTargets = ArrayList<String>()
    private let taskIdToCjoOutput = HashMap<TaskId, String>()
    private let env: HashMap<String, String>
    
    init(buildConfig: BuildConfig, out: OutputStream) {
        this.writer = NinjaWriter(out: out)
        this.buildConfig = buildConfig
        this.targetDir = buildConfig.globalConfig.targetDir
        this.testTargetDir = Path(buildConfig.globalConfig.targetDir).join(UNITTEST_BIN).toString()
        this.env = getVariables() |> collectHashMap
    }
    
    public func writeHeader(): Unit {
        let targetDir = buildConfig.globalConfig.targetDir
        writer.variable("ninja_required_version", "1.3")
        writer.variable("builddir", targetDir)
        writer.variable("TARGET_DIR", targetDir)
        writer.variable("TEST_TARGET_DIR", testTargetDir)
        writer.variable("cjc", "cjc")
        writer.newline()

        writeCompileRule(writer)
        writer.newline()
        writeLinkRule(writer)
        
        writer.newline()
        writer.comment("Build entries")
        writer.newline()
    }

    @When[os == "Linux" || os == "macOS"]
    private func writeCompileRule(writer: NinjaWriter): Unit {
        writer.rule(
            "compile",
            "$env_vars $cjc -p $pkg_path $import_paths $cjc_flags $link_args $custom_options $output_config_flags $output $link_flags",
            description:"Compiling $meta_pkg [$meta_src_set] {$meta_features}",
            restat: true
        )
    }
    
    @When[os == "Windows"]
    private func writeCompileRule(writer: NinjaWriter): Unit {
        writer.rule(
            "compile",
            "cmd /V /C \"$env_vars $cjc -p $pkg_path $import_paths $cjc_flags $link_args $custom_options $output_config_flags $output $link_flags\"",
            description: "Compiling $meta_pkg [$meta_src_set] {$meta_features}",
            restat: true
        )
    }

    @When[os == "Linux" || os == "macOS"]
    private func writeLinkRule(writer: NinjaWriter): Unit {
        writer.rule(
            "link",
            "$env_vars $cjc $obj_path $import_paths $cjc_flags $link_args $custom_options $link_options $output_config_flags $output",
            description:"Linking $meta_pkg [$meta_src_set]",
            restat: true
        )
    }
    
    @When[os == "Windows"]
    private func writeLinkRule(writer: NinjaWriter): Unit {
        writer.rule(
            "link",
            "cmd /V /C \"$env_vars $cjc $obj_path $import_paths $cjc_flags $link_args $custom_options $link_options $output_config_flags $output\"",
            description: "Linking $meta_pkg [$meta_src_set]",
            restat: true
        )
    }
    
    public func writeCompileTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTaskCommon(invocation, task, NinjaTaskType.Compile)
    }
    
    public func writeLinkTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTaskCommon(invocation, task, NinjaTaskType.Link)
    }

    private func getPackageSourceFiles(invocation: CompilerInvocation): Array<String> {
        let dirpath = invocation.pkgPath.getOrDefault { "" }
        var files: Array<String> = []
        if (exists(dirpath) && FileInfo(canonicalize(dirpath)).isDirectory()) {
            let packageFiles = Directory.readFrom(dirpath).iterator().filterMap { it => 
                if (it.isRegular() && it.name.endsWith(".cj")) { 
                    it.path.toString() 
                } else { None }
            } |> collectArray
            files = [dirpath].concat(packageFiles)
        }
        files
    }
    
    private func writeTaskCommon(invocation: CompilerInvocation, task: CompileTask, taskType: NinjaTaskType): Unit {
        let (primaryOutput, cjoOutput) = makeOutputFile(invocation.metadata.artifacts)
        taskIdToCjoOutput[task.id] = cjoOutput
        
        let envVars = envBuilderToString(invocation.envBuilder).replace(targetDir, "$TARGET_DIR")
        let importPaths = invocation.importPaths
            .map { p => "--import-path=${p.replace(targetDir, "$TARGET_DIR")}" } |> collectString(delimiter: " ")
        let cjcFlags = invocation.cjcFlags
            .map { p => p.replace(targetDir, "$TARGET_DIR") } |> collectString(delimiter: " ")
        let customOptions = invocation.customOptions
            .map { p => p.replace(targetDir, "$TARGET_DIR") } |> collectString(delimiter: " ")
        let linkArgs = invocation.linkArgs
            .map { p => p.replace(targetDir, "$TARGET_DIR") } |> collectString(delimiter: " ")
        let linkOptions = invocation.linkOptions
            .map { p => p.replace(targetDir, "$TARGET_DIR") } |> collectString(delimiter: " ")
        let outputConfigFlags = invocation.outputConfigFlags
            .map { p => p.replace(targetDir, "$TARGET_DIR") } |> collectString(delimiter: " ")
        let outputVal = invocation.output.map { it => "-o=${it}"}.getOrDefault { "" }
        
        let implicitDeps = resolveCjoDependencies(task.requireTasks)
        
        let metadata = invocation.metadata
        let featuresStr = metadata.features |> collectString(delimiter: ", ")
        
        writer.comment("========================================")
        writer.comment("${taskType.toString()}: ${metadata.fullName}")
        writer.comment("Target: ${metadata.target}")
        writer.comment("Debug: ${metadata.debug}")
        writer.comment("Mock: ${metadata.mock}")
        writer.comment("OutputType: ${metadata.outputType}")
        writer.comment("SourceSet: ${metadata.sourceSetDir}")
        writer.comment("Features: ${featuresStr}")
        writer.comment("========================================")

        let (inputs, entry) = match (taskType) { 
            case Compile => 
                let sourceFiles = getPackageSourceFiles(invocation) 
                (sourceFiles, ("pkg_path", invocation.pkgPath.getOrDefault { "" }))
            case Link => 
                let objPath = invocation.objPath.getOrDefault { "" }
                ([objPath], ("obj_path", objPath))
        }
        
        writer.build(
            [primaryOutput, cjoOutput],
            taskType.toString().toAsciiLower(),
            inputs: inputs,
            implicit: Some(implicitDeps),
            variables: HashMap<String, String>([
                ("env_vars", envVars),
                entry,
                ("import_paths", importPaths),
                ("cjc_flags", cjcFlags),
                ("link_args", linkArgs),
                ("custom_options", customOptions),
                ("link_options", linkOptions),
                ("output_config_flags", outputConfigFlags),
                ("output", outputVal),
                ("meta_pkg", metadata.fullName),
                ("meta_src_set", metadata.sourceSetDir),
                ("meta_features", featuresStr)
            ])
        )
        writer.newline()
        
        defaultTargets.add(cjoOutput)
    }

    @When[os == "Linux" || os == "macOS"]
    private func envBuilderToString(envBuilder: EnvironmentBuilder): String {
        let envMap = envBuilder.asCliStrings(env).iterator()
            .map { it => (it[0], escapePath(ninjaEscape(it[1]))) } |> collectHashMap
        if (envMap.isEmpty()) { return "" }
        var res = ""
        for ((key, value) in envMap) {
            res += "${key}=${value} "
        }
        res
    }

    @When[os == "Windows"]
    private func envBuilderToString(envBuilder: EnvironmentBuilder): String {
        let envMap = envBuilder.asCliStrings(env).iterator()
            .map { it => (it[0], escapePath(ninjaEscape(it[1]))) } |> collectHashMap
        if (envMap.isEmpty()) { return "" }
        var res = ""
        for ((key, value) in envMap) {
            res += "set ${key}=${value}&& "
        }
        res
    }

    private func resolveCjoDependencies(deps: HashSet<TaskId>): Array<String> {
        let result = ArrayList<String>()
        for (depId in deps) {
            if (let Some(cjoOutput) <- taskIdToCjoOutput.get(depId)) {
                result.add(cjoOutput)
            }
        }
        result.toArray()
    }
    
    public func writeFooter(): Unit {
        writer.defaultTargets(defaultTargets.toArray())
        writer.flush()
    }

    private func makeOutputFile(artifacts: Array<String>): (String, String) {
        let primary = artifacts[0].replace(targetDir, "$TARGET_DIR")
        let cjo = artifacts[1].replace(targetDir, "$TARGET_DIR")
        (primary, cjo)
    }
}

