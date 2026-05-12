// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import cjpm.implement.*
import cjpm.config.*

let planOptions: Array<CliOptionBase> = [
    CliOption(name: "json", action: Action.Append, defaultValue: "-",
        help: "output JSON plan", category: "plan"),
    CliOption(name: "ninja", action: Action.Append, defaultValue: "build.ninja",
        help: "output Ninja file", category: "plan"),
]

let planScope = CliScope(
        name: "plan", description: "Generate build plan for given profile without executing",
        helpNoArgs: true
    )
    .addSubScope(CliScope(name: "build", description: "Generate `build` plan",
        allowLateBinding: true, helpNoArgs: true, autoHelp: "plan")
        .addOptions(planOptions)
        .addOptions(buildOptions)
    )

public class PlanCommand <: Handle {
    private let planParser: CliParser
    private let parseResult: CliParseResult

    
    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args
        // Dummy scope to generate proper `Usage: ` string
        this.planParser = CliParser(CliScope(name: "cjpm", description: "").addSubScope(planScope))
        this.parseResult = planParser.parse(args)
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

        match (parseResult.scope.last) {
            case Some("build") => 
                return handlePlanBuild(parseResult)
            case _ => return false // should be unreachable
        }
    }
    
    private func handlePlanBuild(result: CliParseResult): Bool {
        let buildConfig = constructBuildConfig(parseResult) ?? return false
        buildConfig.linkMode = LinkMode.Always

        for (output in result.namespace.getArray<String>(key: "json") ?? []) {
            let outputPath: ?String = if (output == "-") { None } else { output }
            buildConfig.plans.add(PlanOption(format: PlanFormat.Json, outputPath: outputPath))
        }

        for (output in result.namespace.getArray<String>(key: "ninja") ?? []) {
            let outputPath: ?String = if (output == "-") { None } else { output }
            buildConfig.plans.add(PlanOption(format: PlanFormat.Ninja, outputPath: outputPath))
        }

        if (buildConfig.plans.isEmpty()) {
            eprintln("Error: need to specify at least one of existing build plat formats: --json/--ninja")
            return false
        }
        
        doBuild(buildConfig)
    }
}
