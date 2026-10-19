// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.regex.*
import std.sort.*
import cjpm.config.*

// Resolution and nested build scripts share pending sections. Only main's
// success path commits them, after compilation, tests and post scripts finish.
let COMMAND_LOCKS = HashMap<String, LockFile>()
let CHANGED_LOCKS = HashSet<String>()

func commandLock(rootPath: String): ?LockFile {
    // Nested commands may reach the same project through different paths;
    // canonicalization ensures they share one pending lock and its sections.
    let path = getCanonicalPath(rootPath)
    if (let Some(locked) <- COMMAND_LOCKS.get(path)) { return locked }
    let (ok, locked) = loadCompatibleLockFile(path)
    if (!ok) { return None }
    COMMAND_LOCKS.add(path, locked)
    return locked
}

// Local modules are resolved from the manifest; a lock never supplies paths.
// Apply Git pins only to declarations reached in the active command's graph.
func applyLockedGit(declaration: DepInfo, locked: ?LockEntry): Bool {
    if (!declaration.isGitDep()) { return true }
    try {
        let url = normalizeLockSourceUrl(declaration.git.getOrThrow())
        let entry = locked ?? return true
        var previousUrl = ""
        var commit: ?String = None
        if (let Some(legacy) <- entry.legacy) {
            if (!legacy.isGitDep()) { return true }
            previousUrl = normalizeLockSourceUrl(legacy.git.getOrThrow())
            if (legacy.branch != declaration.branch || legacy.tag != declaration.tag) {
                throw Exception("Git selection changed")
            }
            commit = legacy.commitId
        } else if (let Some(source) <- entry.source && source.startsWith("git+")) {
            let (sourceUrl, sourceBranch, sourceTag, sourceCommit) = parseGitLockSource(source)
            previousUrl = normalizeLockSourceUrl(sourceUrl)
            commit = Some(sourceCommit)
            // Prefer selectors embedded in source; fall back to compatibility
            // fields when reading v1 locks written before this representation.
            let lockedBranch = if (sourceBranch.isSome()) { sourceBranch } else { entry.branch }
            let lockedTag = if (sourceTag.isSome()) { sourceTag } else { entry.tag }
            // A source-only older lock has no evidence of the original
            // selector. Keep its commit usable; otherwise require update when
            // the recorded selector differs from the current declaration.
            if ((lockedBranch.isSome() || lockedTag.isSome() || entry.git.isSome()) &&
                (lockedBranch != declaration.branch || lockedTag != declaration.tag)) {
                throw Exception("Git selection changed")
            }
        } else { return true }
        if (url.removeSuffix(".git") != previousUrl.removeSuffix(".git") ||
            (declaration.commitId.isSome() && declaration.commitId != commit)) {
            throw Exception("Git selection changed")
        }
        declaration.commitId = commit
        // v0 resolved Git lock entries before manifest declarations, retaining
        // their output override and rejecting conflicting explicit overrides.
        if (let Some(outputType) <- entry.outputType) {
            if (declaration.outputType.isSome() && declaration.outputType != entry.outputType) {
                throw Exception("Git output-type changed")
            }
            declaration.outputType = Some(outputType)
        }
        return true
    } catch (_: Exception) {
        eprintln("Error: cannot use locked Git dependency or record its source, please try 'cjpm update'")
        return false
    }
}

// Replace only the section resolved by this phase. Rebuilding entries from the
// selected graph drops obsolete dependencies and completes v0 metadata without
// copying stale fields from the old lock.
func stageResolvedLock(rootPath: String, engine: DepAnalysisEngine, rootName: String,
    isScriptDep!: Bool = false): Bool {
    let locked = commandLock(rootPath) ?? return false
    try {
        let resolved = createLockFileFromResolution(engine, rootName, isScriptDep: isScriptDep)
        completeLockedSources(engine, resolved)
        // Validate the new section before changing command state.
        encodeLockFile(resolved)
        let destination = if (isScriptDep) { locked.scripts } else { locked.requires }
        let entries = if (isScriptDep) { resolved.scripts } else { resolved.requires }
        destination.clear()
        destination.add(all: entries)
        // The other section may still contain unresolved v0 entries. Final
        // validation in flushCommandLocks prevents writing a partially migrated lock.
        locked.formatVersion = CURRENT_LOCK_VERSION
        CHANGED_LOCKS.add(getCanonicalPath(rootPath))
        return true
    } catch (_: Exception) {
        eprintln("Error: cannot collect complete dependency metadata for '${LOCK_FILE_NAME}'")
        return false
    }
}

public func flushCommandLocks(): Bool {
    // Validate every pending file before touching any existing lock.
    // Replacement is atomic per file; this is not a transaction across projects.
    try {
        for (path in CHANGED_LOCKS) { encodeLockFile(COMMAND_LOCKS[path]) }
    } catch (_: Exception) {
        eprintln("Error: unresolved lock metadata; resolve build script dependencies before migrating cjpm.lock")
        return false
    }
    for (path in CHANGED_LOCKS) {
        if (!writeLockFile(path, COMMAND_LOCKS[path])) { return false }
    }
    CHANGED_LOCKS.clear()
    COMMAND_LOCKS.clear()
    return true
}

// Called only after successful dependency resolution. Normalized local/Git
// declarations and the selected registry index are the resolver's module graph;
// ResolveItem.requires is a package/import graph and must not be used here.
// rootName is the standalone project root (excluded by the v1 specification),
// or WORKSPACE_NAME for workspace/script resolution (members remain included).
public func createLockFileFromResolution(engine: DepAnalysisEngine, rootName: String,
    isScriptDep!: Bool = false): LockFile {
    let result = LockFile()
    let entries = if (isScriptDep) { result.scripts } else { result.requires }
    for ((name, module) in engine.finalDepMap) {
        if (name == rootName) {
            continue
        }
        let info = LockEntry()
        // Preserve the resolver's optional override; do not turn an unspecified
        // output type into a default that would constrain later resolution.
        info.outputType = module.sourceInfo.outputType
        if (module.sourceInfo.isGitDep()) {
            info.branch = module.sourceInfo.branch
            info.tag = module.sourceInfo.tag
        }
        var dependencies: HashSet<String>
        if (module.sourceInfo.isCenterRepoDep()) {
            let version = module.sourceInfo.version.getOrThrow()
            if (!engine.indexMap.contains(name) || !engine.indexMap[name].contains(version)) {
                throw Exception("missing selected registry index for lock module '${name}'")
            }
            info.version = Some(version)
            // Reuse exactly the target/profile/test selection used by the solver.
            dependencies = HashSet<String>(engine.collectDepMap(name, version).keys())
        } else {
            if (module.tomlInfo.pkg.name != name) {
                throw Exception("missing resolved metadata for lock module '${name}'")
            }
            info.version = Some(module.tomlInfo.pkg.version)
            dependencies = module.resolvedDependencies ??
                throw Exception("missing resolved dependencies for lock module '${name}'")
        }
        if (!Regex(REGEXP_VERSION).matches(info.version.getOrThrow())) {
            throw Exception("missing exact version for lock module '${name}'")
        }
        let names = dependencies.toArray()
        sort(names)
        info.dependencies = ArrayList<String>(names)
        info.sourcePending = !module.sourceInfo.isPathDep()
        entries.add(name, info)
    }
    for ((name, info) in entries) {
        for (dependency in info.dependencies) {
            if (!entries.contains(dependency)) {
                // Never hide an incomplete resolution by silently dropping an edge.
                throw Exception("unresolved dependency '${dependency}' of lock module '${name}'")
            }
        }
    }
    return result
}

// Fill source/checksum after the resolver has completed. Registry checksums
// come from the selected index; Git commits come from the resolved DepInfo.
// Local path dependencies intentionally remain without source/checksum.
public func completeLockedSources(engine: DepAnalysisEngine, locked: LockFile): Unit {
    let baseRegistry = engine.depot.baseUrl
    for ((name, info) in locked.requires) {
        completeLockedSource(engine, name, info, baseRegistry)
    }
    for ((name, info) in locked.scripts) {
        completeLockedSource(engine, name, info, baseRegistry)
    }
}

func completeLockedSource(engine: DepAnalysisEngine, name: String, info: LockEntry,
    registryUrl: String): Unit {
    let module = engine.finalDepMap[name]
    if (module.sourceInfo.isPathDep() && !module.sourceInfo.isGitDep()) {
        info.sourcePending = false
        return
    }
    if (module.sourceInfo.isCenterRepoDep()) {
        if (registryUrl.isEmpty()) {
            throw Exception("missing registry source for lock module '${name}'")
        }
        let version = module.sourceInfo.version.getOrThrow()
        let index = engine.indexMap[name][version]
        let checksum = registryLockChecksum(index.sha256sum)
        info.source = Some("registry+${normalizeLockSourceUrl(registryUrl, registry: true)}")
        info.checksum = Some(checksum)
    } else if (module.sourceInfo.isGitDep()) {
        let url = module.sourceInfo.git ?? throw Exception("missing Git source for lock module '${name}'")
        let commit = module.sourceInfo.commitId ?? throw Exception("missing resolved Git commit for lock module '${name}'")
        if (!Regex("^([0-9a-f]{40}|[0-9a-f]{64})$").matches(commit)) {
            throw Exception("missing full resolved Git commit for lock module '${name}'")
        }
        // The resolved commit and the original branch/tag request form one
        // canonical source value; no separate Git fields are emitted in v1.
        info.source = Some(formatGitLockSource(normalizeLockSourceUrl(url),
            module.sourceInfo.branch, module.sourceInfo.tag, commit))
        info.checksum = None
    } else {
        throw Exception("unsupported source for lock module '${name}'")
    }
    info.sourcePending = false
}
