// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.fs.*
import cjpm.config.*

// Dependency resolve.
func resolve(globalConfig: GlobalConfig, isScriptDep!: Bool = false): (DepModel, Bool, Bool) {
    var dm = DepModel(globalConfig)

    let (loadFlag, lockFile) = if (isScriptDep) {
        (true, ModuleLock())
    } else {
        loadLockFile(globalConfig.rootPath)
    }
    if (!loadFlag) {
        return (dm, false, false)
    }

    let (checkSuccessful, mockUsed) = dm.resolve(globalConfig.rootPath, lockFile.requires, isScriptDep)
    if (!checkSuccessful) {
        return (dm, false, mockUsed)
    }

    let depMap = dm.depAnalysisEngine.getDepMap()
    // Don't lock path dependencies since it would only cause problems for transitive path dependencies
    depMap.removeIf {_, v => v.path.isSome()}

    if (!isScriptDep) {
        var lockFilePath: String = Path(globalConfig.rootPath).join(LOCK_FILE_NAME).toString()
        // Overwrite file only if we have meaningful changes in case field serialization order got changed
        if (lockFile.requires != depMap || !fileExists(lockFilePath)) {
            if (!lockFile.requires.isEmpty() && !deleteFile(lockFilePath)) {
                return (dm, false, mockUsed)
            }

            if (!creatLockFile(globalConfig.rootPath, ModuleLock(depMap, lockFile.scripts))) {
                return (dm, false, mockUsed)
            }
        }
    }

    // combined is only used in cjpm build
    if (!dm.globalConfig.isPureBuild) {
        dm.superPkgMap.clear()
    } else if (!dealWithSuperPkg(dm, globalConfig.isProjectCombine)) {
        return (dm, false, mockUsed)
    }

    // Circular dependency check.
    if (!topoSort(dm)) {
        return (dm, false, mockUsed)
    }

    if (globalConfig.isProjectCombine) {
        dm.readyPkgs.removeIf {v => v == globalConfig.rootName}
        dm.readyPkgs.add(globalConfig.rootName)
    }

    // Check parent-child package accessLevel.
    if (!checkAccessLevel(dm)) {
        return (dm, false, mockUsed)
    }

    for ((superPkgName, _) in dm.superPkgMap) {
        dm.packageDepMap[superPkgName].requires.remove(all: dm.packageDepMap[superPkgName].forcedRequires)
    }

    return (dm, true, mockUsed)
}

func checkCombinedConfiguration(dm: DepModel, isProjectCombine: Bool, notSupportSet: HashSet<String>): Bool {
    if (!dm.superPkgMap.isEmpty() && !isProjectCombine) {
        let rootName = dm.globalConfig.rootName
        if (!COMMON_INFO.inWorkspace && !dm.superPkgMap.contains(rootName)) {
            eprintln("Error: field 'profile.build.combined' must be configured in the root module '${rootName}'")
            return false
        }
        // Workspace scenario: To avoid over-propagation
        // (forcing unconfigured members into combined mode), ALL currentMembers must
        // explicitly configure combined before auto-propagation is triggered.
        if (COMMON_INFO.inWorkspace) {
            for (memberPath in COMMON_INFO.currentMembers) {
                let canonicalMemberPath = getCanonicalPath(memberPath)
                var memberIsCombined = false
                for ((moduleName, finalDep) in dm.depAnalysisEngine.finalDepMap) {
                    if (getCanonicalPath(finalDep.tomlInfo.pkg.path) == canonicalMemberPath &&
                        dm.superPkgMap.contains(moduleName)) {
                        memberIsCombined = true
                        break
                    }
                }
                if (!memberIsCombined) {
                    eprintln("Error: field 'profile.build.combined' must be configured in workspace member '${memberPath}'")
                    return false
                }
            }
        }
        return true
    }
    true
}

func isValidSuperPkgEntry(dm: DepModel, pkgName: String): Bool {
    if (!dm.packageDepMap.contains(pkgName)) {
        return false
    }
    if (dm.packageDepMap[pkgName].isMacroPackage) {
        return false
    }
    if (!isSupportCombine()) {
        return false
    }
    return true
}

func processSingleSuperPkg(
    dm: DepModel,
    superPkgName: String,
    superPkgCfg: SuperPackageConfig,
    notSupportSet: HashSet<String>
): Bool {
    if (!dm.packageDepMap.contains(superPkgName)) {
        eprintln("Error: field 'profile.build.combined' is not supported for root package '${superPkgName}' " +
            "which contain no source code for compilation")
        return false
    }
    if (dm.packageDepMap[superPkgName].isMacroPackage) {
        println("Warning: field 'profile.build.combined' is not supported for macro root package '${superPkgName}'")
        notSupportSet.add(superPkgName)
        return true
    }
    if (!isSupportCombine()) {
        println("Warning: field 'profile.build.combined' only supports 'Linux/OHOS/Windows', " +
            "so output targeted for module '${superPkgName}' will not be combined")
        notSupportSet.add(superPkgName)
        return true
    }
    for (pkgName in dm.packageDepMap.keys()) {
        if (superPkgName != pkgName.split(".")[0] || superPkgName == pkgName) {
            continue
        }
        superPkgCfg.subPkgSet.add(pkgName)
        if (!dm.packageDepMap[pkgName].isMacroPackage) {
            dm.packageDepMap[pkgName].outputType = Static
        }
    }
    dm.packageDepMap[superPkgName].superPkgCfg = superPkgCfg
    if (dm.packageDepMap[superPkgName].outputType != Dynamic) {
        println("Warning: output-type of combined root package '${superPkgName}' in " +
            "${Path(dm.globalConfig.rootPath).join(CONFIG_FILE_NAME).toString()} is not 'dynamic', " +
            "final output will be dynamic library")
        dm.packageDepMap[superPkgName].outputType = Dynamic
    }
    return true
}

func dealWithSuperPkg(dm: DepModel, isProjectCombine: Bool): Bool {
    let notSupportSet = HashSet<String>()
    var superPkgInValidFlag = false;

    // If the root module is configured with combined, treat all dependent modules as combined too.
    if (!isProjectCombine && !dm.superPkgMap.isEmpty()) {
        var rootIsCombined = false
        let rootName = dm.globalConfig.rootName
        if (dm.superPkgMap.contains(rootName) && isValidSuperPkgEntry(dm, rootName)) {
            rootIsCombined = true
        // Workspace scenario: Only if EVERY member
        // configures combined do we treat the workspace as a combined build and trigger
        // auto-propagation to dependencies. This prevents members that did NOT request
        // combined from being silently forced into combined mode.
        } else if (COMMON_INFO.inWorkspace) {
            rootIsCombined = true
            for (memberPath in COMMON_INFO.currentMembers) {
                let canonicalMemberPath = getCanonicalPath(memberPath)
                var memberIsCombined = false
                for ((moduleName, finalDep) in dm.depAnalysisEngine.finalDepMap) {
                    if (getCanonicalPath(finalDep.tomlInfo.pkg.path) == canonicalMemberPath &&
                        dm.superPkgMap.contains(moduleName) &&
                        isValidSuperPkgEntry(dm, moduleName)) {
                        memberIsCombined = true
                        break
                    }
                }
                if (!memberIsCombined) {
                    rootIsCombined = false
                    break
                }
            }
        }
        if (rootIsCombined) {
            for ((moduleName, _) in dm.depAnalysisEngine.finalDepMap) {
                if (!dm.superPkgMap.contains(moduleName) &&
                    dm.packageDepMap.contains(moduleName) &&
                    !dm.packageDepMap[moduleName].isMacroPackage) {
                    dm.superPkgMap[moduleName] = SuperPackageConfig(moduleName, Dynamic)
                }
            }
        }
    }

    for ((superPkgName, superPkgCfg) in dm.superPkgMap) {
        if (!processSingleSuperPkg(dm, superPkgName, superPkgCfg, notSupportSet)) {
            superPkgInValidFlag = true
        }
    }
    if (superPkgInValidFlag) {
        return false
    }

    dm.superPkgMap.remove(all: notSupportSet)
    
    if (!checkCombinedConfiguration(dm, isProjectCombine, notSupportSet)) {
        return false
    }

    dm.checkSuperPkgDep()
    true
}

public func doCheck(memberModule: String, testCheck: Bool): Bool {
    let tomlInfo: TomlInfo = loadModuleFile(DIR_CURRENT) ?? return false
    if (!checkForWorkspace(DIR_CURRENT, tomlInfo, memberModule, isTest: testCheck)) {
        return false
    }

    var globalConfig: GlobalConfig = GlobalConfig()
    globalConfig.isTest = testCheck
    let (dataModel, flag, _) = resolve(globalConfig)
    if (!flag) {
        return false
    }
    let res: String = String.join(dataModel.readyPkgs.toArray(), delimiter: " -> ")
    println("The valid serial compilation order is:\n    ${res}")
    return true
}

func checkAccessLevel(topoData: DepModel): Bool {
    var levelMap = HashMap<String, String>()
    for ((k, v) in topoData.packageDepMap) {
        levelMap[k] = v.accessLevel
    }

    var checkFlag: Bool = true
    for (subPkg in topoData.readyPkgs) {
        if (!subPkg.contains(".")) {
            continue
        }
        let parentPkg: String = getParentPkg(subPkg)
        if (!levelMap.contains(parentPkg) || !levelMap.contains(subPkg)) {
            continue
        }
        if (!checkParentChildPkg(parentPkg, levelMap[parentPkg], subPkg, levelMap[subPkg])) {
            checkFlag = false
        }
    }
    return checkFlag
}

func getParentPkg(subPkg: String): String {
    let pos = subPkg.lastIndexOf(".").getOrThrow()
    return subPkg[0..pos]
}

func checkParentChildPkg(parentPkg: String, parentLevel: String, subPkg: String, subLevel: String): Bool {
    let leverErr: Bool = (parentLevel == "protected" && subLevel == "public") ||
        (parentLevel == "internal" && subLevel != "internal")

    if (leverErr) {
        eprintln("Error: the access level of child package can't be higher than that of parent package")
        eprintln("    parent package '${parentPkg}' access level: '${parentLevel}'")
        eprintln("    child package '${subPkg}' access level: '${subLevel}'")
        return false
    }

    return true
}