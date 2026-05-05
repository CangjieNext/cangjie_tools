// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import cjpm.implement.*
import cjpm.config.*

let runOptions: Array<CliOptionBase> = [
    CliOption(name: "name", action: Action.Set, category: "run",
        help: "name of the executable product to run, default to 'main'"),
    CliOption(name: "build-args", action: Action.Set, category: "run",
        help: "the arguments to pass to the build process"),
    CliOption(name: "skip-build", action: Action.SetTrue, category: "run",
        help: "skip compile, only run the executable product"),
    CliOption(name: "run-args", action: Action.Set, category: "run",
        help: "the arguments to pass to the executable product"),
    CliOption(name: "target-dir", action: Action.Set, category: "run",
        help: "specify target directory", validator: targetDirValidator),
    CliOption<Feature>(name: "enable-features", action: Action.Append, delimiter: ",", category: "run",
        help: "explicitly specify comma-separated list of features to be enabled", validator: featureValidator),
    CliOption(name: "no-feature-deduce", action: Action.SetFalse,
        help: "disables auto-enabling of features, deduced from other options, machine properties, etc.", category: "run"),
    CliOption(name: "g", short: "g", action: Action.SetTrue,
        help: "enable debug version", category: "run"),
    CliOption(name: "verbose", short: "V", action: Action.SetTrue,
        help: "enable verbose", category: "run"),
    CliOption(name: "skip-script", action: Action.SetTrue,
        help: "disable script 'build.cj'", category: "run"),
]

let runScope = CliScope(name: "run", description: "Compile and run an executable product.",
    finalArgsDescription: "<executable-args>")
    .addOptions(runOptions)

class RunCommand <: Handle {

    private let runParser: CliParser
    private let parseResult: ParseResult

    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args[1..] // remove run itself
        this.runParser = CliParser(CliScope(name: "cjpm", description: "").addSubScope(runScope))
        this.parseResult = runParser.parse(args) 
    }

    public override func printPass(): Unit {
        print("\ncjpm run finished\n")
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

        let runConfig = RunConfig()
        runConfig.isVerbose = parseResult.namespace.get<Bool>(key: "verbose") ?? false
        runConfig.isDebug = parseResult.namespace.get<Bool>(key: "g") ?? false
        runConfig.skipBuild = parseResult.namespace.get<Bool>(key: "skip-build") ?? false
        runConfig.buildArgs = parseResult.namespace.get<String>(key: "build-args") ?? ""
        runConfig.runArgs = parseResult.namespace.get<String>(key: "run-args") ?? ""
        runConfig.enableFeatures = parseResult.namespace.getArray<Feature>(key: "enable-features") ?? []
        runConfig.newRunArgs = ArrayList(parseResult.finalArgs)
        runConfig.globalConfig.isSkipScript = parseResult.namespace.get<Bool>(key: "skip-script") ?? false
        runConfig.globalConfig.noFeatureDeduce = parseResult.namespace.get<Bool>(key: "no-feature-deduce") ?? false
        if (!state.isExperimental && runConfig.globalConfig.noFeatureDeduce) {
            eprintln("Error: `--no-feature-deduce` is part of experimental Cangjie Multiplatform feature, " +
                "please enable `experimental = true` in cjpm.toml of this project")
            return false
        }

        runConfig.name = parseResult.namespace.get<String>(key: "name") ?? "main"
        runConfig.targetPath = parseResult.namespace.get<String>(key: "target-dir") ?? ""

        if (!execPreRun(runConfig)) {
            return false
        }

        return doRun(runConfig)
    }
}

func execPreRun(runConfig: RunConfig): Bool {
    if (runConfig.skipBuild) {
        return true
    }

    let buildOption = ArrayList<String>()
    buildOption.add("build")
    buildOption.add(all: extractOptionByString(runConfig.buildArgs))
    buildOption.add("-i")
    if (runConfig.isDebug) {
        buildOption.add("-g")
    }
    if (runConfig.isVerbose) {
        buildOption.add("-V")
    }
    if (!runConfig.enableFeatures.isEmpty()) {
        if (state.isExperimental) {
            let features = runConfig.enableFeatures |> collectString(delimiter: ",")
            buildOption.add("--enable-features=${features}")
        } else {
            eprintln("Error: `--enable-features` is part of experimental Cangjie Multiplatform feature, " +
                "please enable `experimental = true` in cjpm.toml of this project")
            return false
        }
    }
    if (!runConfig.targetPath.isEmpty()) {
        buildOption.add("--target-dir=${runConfig.targetPath}")
    }
    if (runConfig.globalConfig.isSkipScript) {
        buildOption.add("--skip-script")
    }
    if (runConfig.globalConfig.noFeatureDeduce) {
        buildOption.add("--no-feature-deduce")
    }

    let cmdHandler = CommandFactory().createCommandHandler(buildOption.toArray())
    if (hasHelpCommand(cmdHandler)) {
        return true
    }
    return cmdHandler.handleCommand()
}
