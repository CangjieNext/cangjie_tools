// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.fs.*
import cjpm.config.*

// ---------------------------------------------------------------------------
// Data carrier
// ---------------------------------------------------------------------------

/// Complete compilation plan produced by the library-analysis engine.
/// Consumed by runLibFoldCompile to drive the cjc invocation.
public class LibFoldPlan {
    /// .bc / .a paths to LTO-merge into the output .so (cjc inputs).
    public var foldBcPaths: ArrayList<String> = ArrayList<String>()

    /// Full paths of .so files that must remain as standalone outputs.
    public var keptSoPaths: ArrayList<String> = ArrayList<String>()

    /// Basenames of kept .so files (for quick membership test).
    public var keptSoNames: HashSet<String> = HashSet<String>()

    /// Basenames of .so files that are LTO-merged into the output.
    public var foldedSoNames: HashSet<String> = HashSet<String>()

    // Internal: soName → registered .bc/.a paths (needed by unfoldSo).
    let foldedSoToPaths: HashMap<String, ArrayList<String>> =
        HashMap<String, ArrayList<String>>()

    // BFS visited set (prevents re-processing during keep-expansion).
    var bfsVisited: HashSet<String> = HashSet<String>()

    public init() {}
}

// ---------------------------------------------------------------------------
// .so name helpers
// ---------------------------------------------------------------------------

/// Return true if `name` looks like a shared-library filename.
func isSoFile(name: String): Bool {
    return name.endsWith(".so") || name.contains(".so.")
}

/// Derive the .a name that corresponds to a .so name:
///   libX.so   → libX.a
///   libX.so.Y → libX.a   (strip the version suffix)
func soNameToAName(soName: String): String {
    if (soName.endsWith(".so")) {
        return soName.removeSuffix(".so") + ".a"
    }
    let parts = soName.split(".so.")
    if (parts.size >= 2) {
        return parts[0] + ".a"
    }
    return soName + ".a"
}

// ---------------------------------------------------------------------------
// Unified bc-dir index
// ---------------------------------------------------------------------------

/// Walk all `bcDirs` recursively; return two maps:
///   bcIndex : basename.bc  → full path
///   aIndex  : basename.a   → full path
/// Later entries (from later bcDirs or deeper subdirs) overwrite earlier ones.
func buildBcIndex(bcDirs: ArrayList<String>):
        (HashMap<String, String>, HashMap<String, String>) {
    let bcIndex = HashMap<String, String>()
    let aIndex  = HashMap<String, String>()
    for (dir in bcDirs) {
        collectBcARecursive(dir, bcIndex, aIndex)
    }
    return (bcIndex, aIndex)
}

private func collectBcARecursive(
        dir: String,
        bcIndex: HashMap<String, String>,
        aIndex: HashMap<String, String>): Unit {
    if (!directoryExists(dir)) { return }
    try {
        for (info in Directory.readFrom(Path(dir))) {
            let name = info.name
            let full = Path(dir).join(name).toString()
            if (info.isDirectory()) {
                collectBcARecursive(full, bcIndex, aIndex)
            } else if (name.endsWith(".bc")) {
                bcIndex[name] = full
            } else if (name.endsWith(".a")) {
                aIndex[name] = full
            }
        }
    } catch (e: Exception) {
        eprintln("[lib-fold] DIAG: collectBcARecursive error at ${dir}: ${e.message}")
    }
}

// ---------------------------------------------------------------------------
// Bc/a matching rules
// ---------------------------------------------------------------------------

/// Attempt to find a matching .bc or .a for `soName` in the bc indexes.
///
/// Rules applied in order (first match wins):
///
///   A  libcangjie-<suffix>.so → lib<suffix-with-dots>.bc
///      (e.g. libcangjie-std-collection.so → libstd.collection.bc)
///      Companion: <soStem>FFI.a in the same dir as the .bc
///
///   B  Direct .a:  soNameToAName(soName) looked up in aIndex
///      (e.g. libohos.ark_interop.so → libohos.ark_interop.a)
///
///   D  libstdx.xxx.so → libstdx.xxx.bc  (keep full stem)
///      Companion: <stem>FFI.a in the same dir
///
///   C  libX.so → X.bc  (strip "lib" prefix)
///      Companion: <stem>FFI.a in the same dir
///
/// Returns (primaryPath, ffiPath) where ffiPath is "" if no FFI archive found.
/// Returns None when no match is found.
func findBcForSo(soName: String,
                 bcIndex: HashMap<String, String>,
                 aIndex:  HashMap<String, String>): Option<(String, String)> {
    // --- Rule A ---
    if (soName.startsWith("libcangjie-") && soName.endsWith(".so")) {
        let base  = soName.removeSuffix(".so")          // "libcangjie-std-collection"
        let suf   = base.removePrefix("libcangjie-")    // "std-collection"
        let bcName = "lib" + suf.replace("-", ".") + ".bc"  // "libstd.collection.bc"
        if (let Some(bcPath) <- bcIndex.get(bcName)) {
            let ffiName = base + "FFI.a"
            let ffiPath = Path(bcPath).parent.join(ffiName).toString()
            let ffi = if (fileExists(ffiPath)) { ffiPath } else { "" }
            return Some((bcPath, ffi))
        }
    }

    // --- Rule B: direct .a lookup ---
    let aName = soNameToAName(soName)
    if (let Some(aPath) <- aIndex.get(aName)) {
        return Some((aPath, ""))
    }

    // --- Rule D: libstdx.xxx.so → libstdx.xxx.bc ---
    if (soName.startsWith("libstdx.") && soName.endsWith(".so")) {
        let stem   = soName.removeSuffix(".so")   // "libstdx.io"
        let bcName = stem + ".bc"
        if (let Some(bcPath) <- bcIndex.get(bcName)) {
            let ffiPath = Path(bcPath).parent.join(stem + "FFI.a").toString()
            let ffi = if (fileExists(ffiPath)) { ffiPath } else { "" }
            return Some((bcPath, ffi))
        }
    }

    // --- Rule C: libX.so → X.bc ---
    if (soName.endsWith(".so")) {
        let stem   = soName.removeSuffix(".so").removePrefix("lib")
        let bcName = stem + ".bc"
        if (let Some(bcPath) <- bcIndex.get(bcName)) {
            let ffiPath = Path(bcPath).parent.join(stem + "FFI.a").toString()
            let ffi = if (fileExists(ffiPath)) { ffiPath } else { "" }
            return Some((bcPath, ffi))
        }
    }

    return None
}

// ---------------------------------------------------------------------------
// Register / un-fold
// ---------------------------------------------------------------------------

/// Record `soName` as folded with associated `.bc`/`.a` paths.
func registerFold(plan: LibFoldPlan, soName: String, paths: Array<String>): Unit {
    plan.foldedSoNames.add(soName)
    let lst = ArrayList<String>()
    for (p in paths) {
        plan.foldBcPaths.add(p)
        lst.add(p)
    }
    plan.foldedSoToPaths[soName] = lst
}

/// Record `soPath` as kept (won't be merged into the output).
func registerKeep(plan: LibFoldPlan, soPath: String): Unit {
    let name = Path(soPath).fileName
    if (!plan.keptSoNames.contains(name)) {
        plan.keptSoNames.add(name)
        plan.keptSoPaths.add(soPath)
    }
}

/// Move `soName` from the fold set back to the kept set.
/// Called when BFS finds that a kept lib DT_NEEDs this lib at runtime.
func unfoldSo(cfg: LibFoldConfig, plan: LibFoldPlan,
              soName: String, soPath: String): Unit {
    if (!plan.foldedSoNames.contains(soName)) { return }

    // Remove from foldedSoNames
    let newFolded = HashSet<String>()
    for (n in plan.foldedSoNames) {
        if (n != soName) { newFolded.add(n) }
    }
    plan.foldedSoNames = newFolded

    // Remove associated .bc/.a paths from foldBcPaths
    if (plan.foldedSoToPaths.contains(soName)) {
        let toRemove = HashSet<String>()
        for (p in plan.foldedSoToPaths[soName]) { toRemove.add(p) }
        let newPaths = ArrayList<String>()
        for (p in plan.foldBcPaths) {
            if (!toRemove.contains(p)) { newPaths.add(p) }
        }
        plan.foldBcPaths = newPaths
    }

    // Add to kept set
    registerKeep(plan, soPath)
    logInfo(cfg, "[lib-fold]   un-fold: ${soName}  ← needed by kept chain")
}

// ---------------------------------------------------------------------------
// Load strict-kept file
// ---------------------------------------------------------------------------

/// Read a list of .so basenames (one per line) from `filePath`.
/// Lines that are empty or start with '#' are skipped.
func loadStrictKeptFile(filePath: String): Array<String> {
    if (filePath.isEmpty() || !fileExists(filePath)) { return Array<String>() }
    try {
        let content = String.fromUtf8(File.readFrom(Path(filePath)))
        let names = ArrayList<String>()
        for (rawLine in content.split("\n")) {
            let line = rawLine.trimAscii()
            if (!line.isEmpty() && !line.startsWith("#")) {
                names.add(line)
            }
        }
        return names.toArray()
    } catch (_: Exception) {
        eprintln("[lib-fold] WARN: cannot read strict-kept file: ${filePath}")
        return Array<String>()
    }
}

// ---------------------------------------------------------------------------
// Phase 1: scan input-dirs and classify each .so
// ---------------------------------------------------------------------------

/// Scan all `cfg.inputDirs` for .so files, look up matching .bc/.a, and
/// classify each as fold-candidate or kept.
///
/// Strict-kept names (from `cfg.strictKeptFile`) are pre-seeded into the
/// keep set so they are never folded even when a matching .bc exists.
func scanInputDirs(cfg: LibFoldConfig, tools: LibFoldTools,
                   targetTriple: String, sanitizedFfiDir: String,
                   bcIndex: HashMap<String, String>,
                   aIndex:  HashMap<String, String>,
                   plan: LibFoldPlan): Bool {
    // Pre-populate keep set from --strict-kept file
    for (name in loadStrictKeptFile(cfg.strictKeptFile)) {
        plan.keptSoNames.add(name)
        logInfo(cfg, "[lib-fold]   strict-keep: ${name}")
    }

    for (inputDir in cfg.inputDirs) {
        if (!directoryExists(inputDir)) {
            logInfo(cfg, "[lib-fold]   WARN: input-dir not found: ${inputDir}")
            continue
        }
        try {
            for (info in Directory.readFrom(Path(inputDir))) {
                if (!info.isRegular()) { continue }
                let soName = info.name
                if (!isSoFile(soName)) { continue }
                let soPath = Path(inputDir).join(soName).toString()

                // Strict-keep: already in the keep set → ensure path is recorded
                if (plan.keptSoNames.contains(soName)) {
                    registerKeep(plan, soPath)
                    logInfo(cfg, "[lib-fold]   strict-keep: ${soName}")
                    continue
                }

                // Try to find a matching .bc / .a
                if (let Some((primary, ffiA)) <- findBcForSo(soName, bcIndex, aIndex)) {
                    var paths = [primary]
                    var logSuffix = ""
                    if (!ffiA.isEmpty()) {
                        let sanitized = sanitizeFfiArchive(tools, ffiA, targetTriple, sanitizedFfiDir) ?? ffiA
                        paths = [primary, sanitized]
                        logSuffix = " + ${Path(sanitized).fileName}"
                    }
                    registerFold(plan, soName, paths)
                    logInfo(cfg, "[lib-fold]   fold: ${soName} → ${Path(primary).fileName}${logSuffix}")
                } else {
                    registerKeep(plan, soPath)
                    logInfo(cfg, "[lib-fold]   keep: ${soName}")
                }
            }
        } catch (_: Exception) {
            eprintln("[lib-fold] ERROR: cannot read input-dir: ${inputDir}")
            return false
        }
    }
    return true
}

// ---------------------------------------------------------------------------
// Phase 2: BFS keep-expansion via DT_NEEDED
// ---------------------------------------------------------------------------

/// Search all `cfg.inputDirs` for `soName` and return the full path.
func findSoInInputDirs(cfg: LibFoldConfig, soName: String): Option<String> {
    for (dir in cfg.inputDirs) {
        let p = Path(dir).join(soName).toString()
        if (fileExists(p)) { return Some(p) }
    }
    return None
}

/// BFS: starting from every kept .so, trace transitive DT_NEEDED dependencies
/// that live in any input-dir.  Un-fold any dependency that was previously
/// classified as a fold-candidate (it must remain as a real file because a
/// kept .so's NEEDED entry requires it at runtime).
func expandKeptByDeps(cfg: LibFoldConfig, tools: LibFoldTools,
                      plan: LibFoldPlan): Unit {
    let queue = ArrayList<String>()
    for (name in plan.keptSoNames) {
        queue.add(name)
    }

    var head = 0
    while (head < queue.size) {
        let cur = queue[head]
        head += 1
        if (plan.bfsVisited.contains(cur)) { continue }
        plan.bfsVisited.add(cur)

        let curPath = findSoInInputDirs(cfg, cur) ?? continue

        for (dep in getNeeded(tools, curPath)) {
            if (plan.bfsVisited.contains(dep)) { continue }
            // Only care about deps that live in our input-dirs.
            let depPath = findSoInInputDirs(cfg, dep) ?? continue

            if (plan.foldedSoNames.contains(dep)) {
                unfoldSo(cfg, plan, dep, depPath)
            } else if (!plan.keptSoNames.contains(dep)) {
                registerKeep(plan, depPath)
            }
            queue.add(dep)
            logInfo(cfg, "[lib-fold]   dep-chain: ${dep}  ← needed by ${cur}")
        }
    }
}

// ---------------------------------------------------------------------------
// Orchestrator
// ---------------------------------------------------------------------------

/// Run the full analysis pipeline and return the build plan, or None on error.
public func buildLibFoldPlan(cfg: LibFoldConfig, tools: LibFoldTools,
                              targetTriple: String,
                              sanitizedFfiDir: String): Option<LibFoldPlan> {
    let plan = LibFoldPlan()

    logInfo(cfg, "[lib-fold] [scan] building bc/a index ...")
    let (bcIndex, aIndex) = buildBcIndex(cfg.bcDirs)
    logInfo(cfg, "[lib-fold] [scan]   .bc: ${bcIndex.size}  .a: ${aIndex.size}")

    logInfo(cfg, "[lib-fold] [scan] Phase 1: classifying .so files ...")
    if (!scanInputDirs(cfg, tools, targetTriple, sanitizedFfiDir,
                       bcIndex, aIndex, plan)) {
        return None
    }

    logInfo(cfg, "[lib-fold] [scan] Phase 2: BFS keep-expansion ...")
    expandKeptByDeps(cfg, tools, plan)

    logInfo(cfg,
        "[lib-fold] [scan] done: fold=${plan.foldedSoNames.size}" +
        "  keep=${plan.keptSoNames.size}")

    eprintln("[lib-fold] DIAG folded: ${plan.foldedSoNames |> collectArray}")
    eprintln("[lib-fold] DIAG kept:   ${plan.keptSoNames |> collectArray}")

    return Some(plan)
}
