// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import cjpm.implement.*
import cjpm.config.*

class RunCommand <: Handle {
    private let RUN_USAGE: String = """
Compile and run an executable product.

Usage:
  cjpm run [option]

Available options:
  --name <value>              name of the executable product to run, default to 'main'
  --build-args <value>        the arguments to pass to the build process
  --skip-build                skip compile, only run the executable product
  --run-args <value>          the arguments to pass to the executable product
  --target-dir <value>        specify target directory
  -g                          enable debug version
  -h, --help                  help for run
  -V, --verbose               enable verbose
  --skip-script               disable script 'build.cj'."""

    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args[1..] // remove run itself
    }

    public override func printPass(): Unit {
        print("\ncjpm run finished\n")
    }

    func handleUnparseArgs(unparseArgs: Array<String>): Bool {
        if (unparseArgs.isEmpty()) {
            return true
        }

        if (unparseArgs.contains("--name")) {
            eprintln("Error: the '--name' option needs a value")
            return false
        }

        if (unparseArgs.contains("--build-args")) {
            eprintln("Error: the '--build-args' option needs a value")
            return false
        }

        if (unparseArgs.contains("--run-args")) {
            eprintln("Error: the '--run-args' option needs a value")
            return false
        }

        if (unparseArgs.contains("--target-dir")) {
            eprintln("Error: the '--target-dir' option needs a value")
            return false
        }

        eprintln("Error: unknow command '${unparseArgs[0]}' for cjpm run")
        return false
    }

    public override func printHelp(): Unit {
        println(RUN_USAGE)
        return
    }

    public override func handleCommand(): Bool {
        let (argMap, unparseArgs) = getParseOption(this.arguments, "gV",
            ["verbose", "skip-build", "name=", "build-args=", "run-args=", "target-dir=", "skip-script"])

        if (!handleUnparseArgs(unparseArgs)) {
            return false
        }

        let runConfig = RunConfig()
        runConfig.isVerbose = getBoolOption(argMap, "-V", "--verbose")
        runConfig.isDebug = getBoolOption(argMap, "-g")
        runConfig.skipBuild = getBoolOption(argMap, "--skip-build")
        runConfig.buildArgs = getArgValue(argMap, "--build-args")[0]
        runConfig.runArgs = getArgValue(argMap, "--run-args")[0]
        runConfig.globalConfig.isSkipScript = getBoolOption(argMap, "--skip-script")
        let (runName, nameFlag) = getArgValue(argMap, "--name")
        if (nameFlag) {
            if (runName.isEmpty()) {
                eprintln("Error: the name of the executable product can't be empty")
                return false
            }
            runConfig.name = runName
        }

        if (let Some(dir) <- handleTargetDirOption(argMap)) {
            runConfig.targetPath = dir
        } else {
            return false
        }

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
    if (!runConfig.targetPath.isEmpty()) {
        buildOption.add("--target-dir=${runConfig.targetPath}")
    }
    if (runConfig.globalConfig.isSkipScript) {
        buildOption.add("--skip-script")
    }

    let cmdHandler = CommandFactory().createCommandHandler(buildOption.toArray())
    if (hasHelpCommand(cmdHandler)) {
        return true
    }
    return cmdHandler.handleCommand()
}
