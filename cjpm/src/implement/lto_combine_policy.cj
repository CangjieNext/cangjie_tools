// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import cjpm.config.*

// LTO/combine validation and user-facing messages, grouped by build pipeline phase:
//   1. dep-resolve  — checkProjectCombineAtDepResolve, registerModuleCombineFromToml
//   2. dep-model    — validateSuperPkgEntries, validateCombinedModulesConfigured
//   3. build-config — configureLtoAndCombineBuild, validateProjectCombineRootDependency
//
// State mutation lives in lto_combine_state.cj; platform checks in lto_combine_capabilities.cj.

private func combineAllDepsField(rootName: String): String {
    "'package.package-configuration.${rootName}.combine-all-deps = true'"
}

// True when either legacy string lto or struct [profile.build.lto].level is set.
func isLtoConfiguredInToml(tomlInfo: TomlInfo): Bool {
    !tomlInfo.profile.build.ltoValue.isEmpty() || !tomlInfo.profile.build.lto.level.isEmpty()
}

// Prefer struct lto.level over deprecated string profile.build.lto.
func resolveLtoLevelFromToml(tomlInfo: TomlInfo): String {
    if (!tomlInfo.profile.build.lto.level.isEmpty()) {
        tomlInfo.profile.build.lto.level
    } else {
        tomlInfo.profile.build.ltoValue
    }
}

func warnLtoDisablesCompilePipelineParallel(): Unit {
    eprintln("Warning: cjpm won't apply pipeline optimization to the build process when field 'profile.build.lto' is 'full' or 'thin'")
}

// Dep-resolve phase: validate and set isProjectCombine on globalConfig.
func checkProjectCombineAtDepResolve(globalConfig: GlobalConfig, tomlInfo: TomlInfo): Bool {
    if (!tomlInfo.pkg.packageConfiguration.contains(globalConfig.rootName) ||
        !tomlInfo.pkg.packageConfiguration[globalConfig.rootName].combineAllDeps) {
        return true
    }
    if (tomlInfo.profile.build.combined.isEmpty()) {
        eprintln("Error: field ${combineAllDepsField(globalConfig.rootName)} " +
            "is invalid when field 'profile.build.combined' is empty")
        return false
    }
    if (globalConfig.isForScript) {
        println("Warning: module '${tomlInfo.pkg.name}' will not be compiled as " +
            "combine-all-deps module when depended by build.cj")
    } else if (globalConfig.isTest) {
        println("Warning: module '${tomlInfo.pkg.name}' will not be compiled as " +
            "combine-all-deps module for test")
    } else {
        globalConfig.isProjectCombine = true
    }
    true
}

// Dep-resolve phase: register module-level combine from dependent modules.
func registerModuleCombineFromToml(globalConfig: GlobalConfig, tomlInfo: TomlInfo,
    superPkgMap: HashMap<String, SuperPackageConfig>): Unit {
    if (tomlInfo.profile.build.combined.isEmpty() || globalConfig.isProjectCombine) {
        return
    }
    if (globalConfig.isForScript) {
        println("Warning: module '${tomlInfo.pkg.name}' will not be compiled as " +
            "combined module when depended by build.cj")
        return
    }
    for ((pkgName, outputType) in tomlInfo.profile.build.combined) {
        superPkgMap[pkgName] = SuperPackageConfig(pkgName, outputType)
    }
}

// Build-config phase: apply LTO settings and validate lto-combine prerequisites.
func configureLtoAndCombineBuild(buildConfig: BuildConfig, tomlInfo: TomlInfo): Bool {
    let rootName = buildConfig.globalConfig.rootName
    if (buildConfig.globalConfig.isProjectCombine &&
        buildConfig.packageList.isMacro(rootName)) {
        eprintln("Error: field ${combineAllDepsField(rootName)} " +
            "is not support for macro root package '${rootName}'")
        return false
    }

    if (!isLtoConfiguredInToml(tomlInfo) && tomlInfo.profile.build.lto.keepPkgVisibility.isSome()) {
        eprintln("Error: field 'keep-pkg-visibility' of '[profile.build.lto]' is not supported when lto is disabled")
        return false
    }

    if (isLtoConfiguredInToml(tomlInfo)) {
        buildConfig.isLto = supportLtoForCurrentTarget()
        if (!buildConfig.isLto) {
            println("Warning: configuration failed, field 'profile.build.lto' " +
                "settings only support compilation for ${LTO_SUPPORTED_PLATFORMS}")
        }
        if (!tomlInfo.profile.build.ltoValue.isEmpty()) {
            println("Warning: The string-type 'lto' under 'profile.build' will be deprecated. " +
                "Please use the struct-type 'lto' to enable LTO functionality. For details, refer to the user guide.")
        }
        buildConfig.ltoValue = resolveLtoLevelFromToml(tomlInfo)
        buildConfig.keepPkgVisibility = tomlInfo.profile.build.lto.keepPkgVisibility ?? None
        if (let Some(keepPkgs) <- buildConfig.keepPkgVisibility) {
            for (fullName in keepPkgs) {
                if (!keepPkgVisibilityNameCheck(fullName)) {
                    return false
                }
            }
        }
    }

    if (supportLtoForCurrentTarget() && buildConfig.globalConfig.isProjectCombine &&
        buildConfig.ltoValue.isEmpty()) {
        eprintln("Error: field ${combineAllDepsField(rootName)} " +
            "is invalid when field 'profile.build.lto' is empty")
        return false
    } else if (!supportLtoForCurrentTarget() && buildConfig.globalConfig.isProjectCombine) {
        eprintln("Error: field ${combineAllDepsField(rootName)} " +
            "only supports compilation to ${LTO_SUPPORTED_PLATFORMS}")
        return false
    }
    true
}

// Build-config phase: root package must not be depended on when combine-all-deps is enabled.
func validateProjectCombineRootDependency(buildConfig: BuildConfig, depPkgName: String,
    depRequires: HashSet<String>): Bool {
    if (!buildConfig.globalConfig.isProjectCombine) {
        return true
    }
    let rootName = buildConfig.globalConfig.rootName
    if (depRequires.contains(rootName)) {
        eprintln("Error: root package '${rootName}' is depended by package ${depPkgName}, " +
            "it cannot be depended by any package when field 'combine-all-deps = true' is valid")
        return false
    }
    true
}

func validateIosProjectCombineRootOutputType(superPkgName: String, outputType: OutputType,
    configFilePath: String): Bool {
    // iOS project lto-combine root must be static; linked as native staticlib in Xcode later.
    if (outputType == Static) {
        return true
    }
    eprintln("Error: output-type of combined root package '${superPkgName}' in " +
        "${configFilePath} is not 'static' for iOS " +
        "when field 'combine-all-deps = true' is valid")
    false
}

func validateCombinedRootHasSource(superPkgName: String): Bool {
    eprintln("Error: field 'profile.build.combined' is not supported for root package '${superPkgName}' " +
        "which contain no source code for compilation")
    false
}

func warnCombinedMacroRoot(superPkgName: String): Unit {
    println("Warning: field 'profile.build.combined' is not supported for macro root package '${superPkgName}'")
}

func warnCombinedPlatformUnsupported(superPkgName: String): Unit {
    println("Warning: field 'profile.build.combined' only supports '${MODULE_COMBINE_SUPPORTED_PLATFORMS}', " +
        "so output targeted for module '${superPkgName}' will not be combined")
}

func warnCombinedRootOutputTypeNotDynamic(superPkgName: String, configFilePath: String): Unit {
    println("Warning: output-type of combined root package '${superPkgName}' in " +
        "${configFilePath} is not 'dynamic', " +
        "final output will be dynamic library")
}

// Every non-macro module in the dependency graph must configure combined when any module does.
func validateCombinedModulesConfigured(dm: DepModel, isProjectCombine: Bool,
    notSupportSet: HashSet<String>): Bool {
    if (dm.superPkgMap.isEmpty() || isProjectCombine) {
        return true
    }
    var flag = true
    for ((moduleName, _) in dm.depAnalysisEngine.finalDepMap) {
        if (!dm.superPkgMap.contains(moduleName) &&
            !notSupportSet.contains(moduleName) &&
            dm.packageDepMap.contains(moduleName) &&
            !dm.packageDepMap[moduleName].isMacroPackage) {
            eprintln("Error: field 'profile.build.combined' must be configured to combine module '${moduleName}'")
            flag = false
        }
    }
    flag
}

func shouldSkipModuleCombineOnPlatform(compileTarget: String, isProjectCombine: Bool): Bool {
    !supportModuleCombine(compileTarget, isProjectCombine)
}

// Returns modules that should be removed from superPkgMap (macro root or unsupported platform).
func validateSuperPkgEntries(dm: DepModel, isProjectCombine: Bool, compileTarget: String,
    configFilePath: String): (Bool, HashSet<String>) {
    let notSupportSet = HashSet<String>()
    var superPkgInValidFlag = false
    for ((superPkgName, _) in dm.superPkgMap) {
        if (isIosTarget(compileTarget) && isProjectCombine &&
            dm.packageDepMap.contains(superPkgName) &&
            !validateIosProjectCombineRootOutputType(superPkgName, dm.packageDepMap[superPkgName].outputType,
                configFilePath)) {
            superPkgInValidFlag = true
            continue
        }
        if (!dm.packageDepMap.contains(superPkgName)) {
            superPkgInValidFlag = true
            validateCombinedRootHasSource(superPkgName)
            continue
        }
        if (dm.packageDepMap[superPkgName].isMacroPackage) {
            warnCombinedMacroRoot(superPkgName)
            notSupportSet.add(superPkgName)
            continue
        }
        if (shouldSkipModuleCombineOnPlatform(compileTarget, isProjectCombine)) {
            warnCombinedPlatformUnsupported(superPkgName)
            notSupportSet.add(superPkgName)
            continue
        }
    }
    (!superPkgInValidFlag, notSupportSet)
}
