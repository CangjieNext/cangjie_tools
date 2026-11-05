// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.argopt.*
import std.collection.*
import std.regex.*
import cjpm.util.*
import cjpm.config.*

func getArgValue(argMap: HashMap<String, String>, longOpt: String, shortOpt: String): (String, Bool) {
    if (argMap.contains(longOpt)) {
        return (argMap[longOpt], true)
    }
    if (argMap.contains(shortOpt)) {
        return (argMap[shortOpt], true)
    }
    return ("", false)
}

func getArgValue(argMap: HashMap<String, String>, opt: String): (String, Bool) {
    if (argMap.contains(opt)) {
        return (argMap[opt], true)
    }
    return ("", false)
}

func getBoolOption(argMap: HashMap<String, String>, arg: String): Bool {
    return argMap.contains(arg)
}

func getBoolOption(argMap: HashMap<String, String>, shortArg: String, longArg: String): Bool {
    return (argMap.contains(shortArg) || argMap.contains(longArg))
}

// The compatibility target version option is handled before `std.argopt`, so that a
// duplicate specification can be detected and the option never leaks into unparseArgs.
let COMPAT_VERSION_OPTION: String = "--compat-version"
let REGEXP_COMPAT_VERSION: String = "^([0-9]+)[.]([0-9]+)$"

func isCompatVersionValue(value: String): Bool {
    if (value.isEmpty()) {
        return false
    }
    var matched = Regex(REGEXP_COMPAT_VERSION)
    match (matched.find(value)) {
        case Some(_) => return true
        case None => return false
    }
}

/*
 * Extract and validate all `--compat-version <value>` occurrences from the raw arguments.
 *
 * The option is removed from the returned arguments. Values are only checked against the
 * two-segment `major.minor` format: the supported version whitelist is owned by cjc, cjpm
 * forwards the value and lets the compiler report unsupported versions.
 *
 * Returns None when a value is missing, malformed or specified with conflicting values.
 */
public func handleCompatVersionOption(arguments: Array<String>): Option<Array<String>> {
    let filtered = ArrayList<String>()
    var version = ""
    var hasVersion = false
    var index = 0
    while (index < arguments.size) {
        let arg = arguments[index]
        var current = ""
        var hasCurrent = false
        if (arg == COMPAT_VERSION_OPTION) {
            if (index + 1 >= arguments.size) {
                eprintln("Error: the '${COMPAT_VERSION_OPTION}' option needs a value")
                return None
            }
            current = arguments[index + 1]
            hasCurrent = true
            index += 2
        } else if (arg.startsWith(COMPAT_VERSION_OPTION + "=")) {
            current = arg.removePrefix(COMPAT_VERSION_OPTION + "=")
            hasCurrent = true
            index += 1
        } else {
            filtered.add(arg)
            index += 1
        }

        if (hasCurrent) {
            if (!isCompatVersionValue(current)) {
                eprintln("Error: invalid value '${current}' for '${COMPAT_VERSION_OPTION}', " +
                    "the example is '1.3'")
                return None
            }
            if (hasVersion && version != current) {
                eprintln("Error: the '${COMPAT_VERSION_OPTION}' option is specified with different values " +
                    "'${version}' and '${current}'")
                return None
            }
            version = current
            hasVersion = true
        }
    }

    if (hasVersion) {
        compatVersion = version
    }
    return Some(filtered.toArray())
}

public func getParseOption(sysArgs: Array<String>, shortArg: String, longArgList: Array<String>):
    (HashMap<String, String>, Array<String>) {
    var ao = ArgOpt(sysArgs, shortArg, longArgList)
    let argMap: HashMap<String, String> = ao.getArgumentsMap()
    let unparseArgs = ao.getUnparseArgs()
    return (argMap, unparseArgs)
}

public func hasHelpCommand(cmdHandler: Handle): Bool {
    if (cmdHandler.isHelp()) {
        cmdHandler.printHelp()
        if (cmdHandler.arguments.size > 1) {
            eprintln("Warning: cjpm doesn't do job in show usage mode")
        }
        return true
    }
    return false
}

func handleTargetDirOption(argMap: HashMap<String, String>): Option<String> {
    let (newDir, dirFlag) = getArgValue(argMap, "--target-dir")
    if (dirFlag) {
        if (newDir.isEmpty()) {
            eprintln("Error: the target directory can't be empty")
            return None
        }
        if (!safeCheck(newDir, "target-dir")) {
            return None
        }
    }
    return Some(newDir)
}

func handleEnableFeaturesOption(argMap: HashMap<String, String>): Result<Array<Feature>, Array<CJMPError>> {
    let flag = "enable-features"
    let (featureString, featuresFlag) = getArgValue(argMap, "--${flag}")
    if (featuresFlag && !safeCheck(featureString, flag)) {
        return Err([])
    }
    if (featureString.isEmpty()) {
        return Ok([])
    }

    featureString.trimAscii().split(",", removeEmpty: true).allResult { it =>
        QName.fromString(it)
            .mapErr { e => ErrorWrapper(None, e) }
            .flatMap { it => Feature.create(it) }.mapErr { e => [ e ] } }
}

func handleFilterOption(argMap: HashMap<String, String>): Option<String> {
    let flag = "filter"
    let (filterOptions, filterFlag) = getArgValue(argMap, "--${flag}")
    if (filterFlag && !safeCheck(filterOptions, flag)) {
        return None
    }
    if (filterOptions.isEmpty()) {
        return None
    }
    return Some(filterOptions)
}

func handleTagsOption(argMap: HashMap<String, String>, isInclude!: Bool = true): Option<String> {
    let flag = if (isInclude) {
        "include-tags"
    } else {
        "exclude-tags"
    }
    let (tagsOptions, tagsFlag) = getArgValue(argMap, "--${flag}")
    if (tagsFlag && !safeCheck(tagsOptions, flag)) {
        return None
    }
    if (tagsOptions.isEmpty()) {
        return None
    }
    return Some(tagsOptions)
}

public func handleMemberOption(argMap: HashMap<String, String>): Option<String> {
    let (member, memFlag) = getArgValue(argMap, "--member", "-m")
    if (memFlag) {
        if (member.isEmpty()) {
            eprintln("Error: the member value can't be empty")
            return None
        }
    }
    return Some(member)
}

func handleCrossTarget(argMap: HashMap<String, String>): Option<Bool> {
    let (target, crossCompileFlag) = getArgValue(argMap, "--target")
    if (crossCompileFlag) {
        let (hasTarget, validTarget) = crossCompileCheck(target)
        if (!hasTarget) {
            return None
        }
        return Some(validTarget)
    }
    return Some(crossCompileFlag)
}

func crossCompileCheck(target: String): (Bool, Bool) {
    if (target.size == 0) {
        eprintln("Error: the cross-compile target can't be empty")
        return (false, false)
    }

    let invalidTargetMessage =
        "Error: the cross-compile target '${target}' is wrong, the example is 'aarch64-hm-gnu'"
    if (target == "." || target.contains("..")) {
        eprintln(invalidTargetMessage)
        return (false, false)
    }
    for (segment in target.split("-")) {
        if (segment == ".") {
            eprintln(invalidTargetMessage)
            return (false, false)
        }
    }

    let REGEXP_CROSS_COMPILE = "^([-_a-zA-Z0-9.]*)$"
    var matched = Regex(REGEXP_CROSS_COMPILE)
    match (matched.find(target)) {
        case Some(_) =>
            if (target == targetConfigName) {
                eprintln("Warning: the current platform is '${target}', the '--target' option will be ignored")
                return (true, false)
            }
            if (normalizeTarget(target) == normalizeTarget(targetConfigName)) {
                eprintln("Warning: target '${target}' is equivalent to the local target " +
                    "'${targetConfigName}', cjpm will perform native-compilation instead of cross-compilation")
                return (true, false)
            }
            crossCompileTarget = target
            return (true, true)
        case None => eprintln(invalidTargetMessage)
    }

    return (false, false)
}

public func lackConfigInfo(): Bool {
    !FINAL_CJPM_CONFIG_INFO.isValid
}

public func findCjc(): Unit {
    targetConfigName = state.cjcInfo.target.toString()
}
