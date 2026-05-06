// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import std.convert.*
import cjpm.implement.*
import cjpm.config.*

let buildOptions: Array<CliOptionBase> = [
    CliOption(name: "incremental", short: "i", action: Action.SetTrue, category: "build",
        help: "enable incremental compilation"),
    CliOption<Int32>(name: "jobs", short: "j", action: Action.Set, category: "build", helpValue: "<N>",
        help: "the number of jobs to spawn in parallel during the build process", validator: jobsValidator),
    CliOption(name: "verbose", short: "V", action: Action.SetTrue,
        help: "enable verbose", category: "build"),
    CliOption(name: "g", short: "g", action: Action.SetTrue,
        help: "enable compile debug version target", category: "build"),
    CliOption(name: "coverage", action: Action.SetTrue,
        help: "enable coverage", category: "build"),
    CliOption(name: "cfg", action: Action.Set, 
        help: "enable the customized option 'cfg'", category: "build"),
    CliOption<Feature>(name: "enable-features", action: Action.Append, delimiter: ",", category: "build",
        help: "explicitly specify comma-separated list of features to be enabled", validator: featureValidator),
    CliOption(name: "no-feature-deduce", action: Action.SetTrue,
        help: "disables auto-enabling of features, deduced from other options, machine properties, etc.", category: "build"),
    CliOption(name: "member", short: "m", action: Action.Set,
        help: "specify a member module of the workspace", category: "build"),
    CliOption(name: "target", action: Action.Set, category: "build",
        help: "generate code for the given target platform", validator: targetValidator),
    CliOption(name: "target-dir", action: Action.Set, category: "build",
        help: "specify target directory", validator: targetDirValidator),
    CliOption(name: "output", short: "o", action: Action.Set, category: "build",
        help: "specify product name when compiling an executable file", validator: outputValidator),
    CliOption(name: "lint", short: "l", action: Action.SetTrue,
        help: "enable cjlint code check", category: "build"),
    CliOption(name: "mock", action: Action.SetTrue,
        help: "enable support of mocking classes in tests", category: "build"),
    CliOption(name: "skip-script", action: Action.SetTrue,
        help: "disable script 'build.cj'", category: "build")
]

let buildScope = CliScope(name: "build", description: "Compile a local module and all of its dependencies.", allowLateBinding: true)
    .addOptions(buildOptions)

// BuildCommand represents the build command
class BuildCommand <: Handle {
    private let buildParser: CliParser
    private let parseResult: ParseResult

    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args
        this.buildParser = CliParser(CliScope(name: "cjpm", description: "").addSubScope(buildScope))
        this.parseResult = buildParser.parse(args) 
    }

    public override func isHelp(): Bool {
        parseResult.namespace.helpText.isSome()
    }
    
    public override func printHelp(): Unit {
        println(parseResult.namespace.helpText.getOrDefault { "" })
        return
    }

    public override func handleCommand(): Bool {
        if (let Some(helpText) <- parseResult.namespace.helpText) {
            println(helpText)
            return true
        }

        if (!parseResult.errors.isEmpty()) {
            printCliErrors(parseResult)
            return false
        }

        if (this.arguments.size == 0) {
            let buildConfig = BuildConfig()
            buildConfig.globalConfig.resolveType = RESOLVE_BUILD
            buildConfig.globalConfig.isPureBuild = true
            return doBuild(buildConfig)
        }

        let buildConfig = constructBuildConfig(parseResult) ?? return false

        return doBuild(buildConfig)
    }
}

private func setCrossCompileTarget(target: String): Bool {
    if (target == targetConfigName) {
        eprintln("Warning: the current platform is '${target}', the '--target' option will be ignored")
        false
    } else {
        crossCompileTarget = target
        true
    }
} 

private func setMaxParallelSize(jobs: Int32): Unit {
    let newSize = Int64(jobs)
    if (newSize > 2 * getCpus()) {
        println(
            "Warning: the number of jobs '-j${newSize}' exceeds limit '${2 * getCpus()}', the number of jobs will be set to '-j${2 * getCpus()}'"
        )
    }
    maxParallelSize = min(newSize, 2 * getCpus())
}

public func handleParallelSize(argMap: HashMap<String, String>): Bool {
    let (sizeStr, sizeFlag) = getArgValue(argMap, "-j", "--jobs")
    if (sizeFlag) {
        var size: Int32 = 0
        try {
            size = Int32.parse(sizeStr)
        } catch (e: Exception) {
            eprintln("Error: the '-j/--jobs' option requires a positive integer argument, " +
                "its valid range is (0, 2147483647]")
            return false
        }

        if (size <= 0) {
            eprintln("Error: the '-j/--jobs' option requires a positive integer argument, " +
                "its valid range is (0, 2147483647]")
            return false
        } else {
            setMaxParallelSize(size)
            return true
        }
    }
    return true
}

private func handleUnresolvedOptions(result: ParseResult, buildConfig: BuildConfig): Bool {
    for (raw in result.unresolvedOptions) {
        let option = raw[2..]
        if (!customizedOptionCheck(option, "")) {
            return false
        }
        buildConfig.customizedOption.add(option)
    }
    return true
}

public func constructBuildConfig(parseResult: ParseResult): ?BuildConfig {
        let buildConfig = BuildConfig()
        buildConfig.isVerbose = parseResult.namespace.get<Bool>(key: "verbose") ?? false
        buildConfig.isIncremental = parseResult.namespace.get<Bool>(key: "incremental") ?? false
        buildConfig.isCov = parseResult.namespace.get<Bool>(key: "coverage") ?? false
        buildConfig.isDebug = parseResult.namespace.get<Bool>(key: "g") ?? false
        buildConfig.isLint = parseResult.namespace.get<Bool>(key: "lint") ?? false
        buildConfig.mockSupported = parseResult.namespace.get<Bool>(key: "mock") ?? false
        buildConfig.globalConfig.isSkipScript = parseResult.namespace.get<Bool>(key: "skip-script") ?? false

        buildConfig.globalConfig.enabledFeatures = parseResult.namespace.getArray<Feature>(key: "enable-features") ?? []
        buildConfig.globalConfig.noFeatureDeduce = parseResult.namespace.get<Bool>(key: "no-feature-deduce") ?? false

        buildConfig.exeName = parseResult.namespace.get<String>(key: "output") ?? "main"
        buildConfig.targetDir = parseResult.namespace.get<String>(key: "target-dir") ?? ""

        let target = parseResult.namespace.get<String>(key: "target")
        if (let Some(target) <- target) {
            buildConfig.isCrossCompile = setCrossCompileTarget(target)
        }

        let jobs = parseResult.namespace.get<Int32>(key: "jobs") ?? Int32(maxParallelSize)
        setMaxParallelSize(jobs)

        buildConfig.memberModule = parseResult.namespace.get<String>(key: "member") ?? ""

        // Adding/removing any `CLI` options is a breaking change
        if (!handleUnresolvedOptions(parseResult, buildConfig)) {
            return None
        }

        buildConfig.globalConfig.isVerbose = buildConfig.isVerbose
        buildConfig.globalConfig.isDebug = buildConfig.isDebug
        buildConfig.globalConfig.isCross = buildConfig.isCrossCompile
        buildConfig.globalConfig.isIncremental = buildConfig.isIncremental
        buildConfig.globalConfig.resolveType = RESOLVE_BUILD
        buildConfig.globalConfig.isPureBuild = true

        return buildConfig
}
