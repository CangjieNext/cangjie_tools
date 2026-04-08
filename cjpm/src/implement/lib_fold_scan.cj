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

/// Complete compilation plan produced by the library-analysis engine (Step 4).
/// Steps 5 and 6 consume this to drive cjc invocations.
public class LibFoldPlan {
    // --- App side ---

    /// All .bc / FFI.a paths to fold into entry.so (cjc Pass 1).
    /// The entry .bc (ohos_app_cangjie_entry.bc) is always the first element.
    public var appBcPaths: ArrayList<String> = ArrayList<String>()

    /// app/ .so names that have no matching .bc; linked as -l:<name> in Pass 1.
    public var appNoBcSoNames: ArrayList<String> = ArrayList<String>()

    // --- System side ---

    /// .bc / .a paths to fold into the new ark_interop.so (cjc Pass 2).
    public var ltoAPaths: ArrayList<String> = ArrayList<String>()

    /// Full paths of lib/ .so files that must be copied as standalone outputs.
    public var noASoPaths: ArrayList<String> = ArrayList<String>()

    /// Names of .so files that have been folded into ark_interop.so.
    public var foldedSoNames: HashSet<String> = HashSet<String>()

    // --- Internal: BFS state shared across Round 1 and Round 2 ---

    /// Maps each folded .so name to the .a/.bc paths that represent it.
    /// Required by un-fold to remove the right entries from ltoAPaths.
    let foldedSoToAPaths: HashMap<String, ArrayList<String>> = HashMap<String, ArrayList<String>>()

    /// Union of strictKeep + all PINNED names.  No lib in this set will fold.
    var excludeSet: HashSet<String> = HashSet<String>()

    // --- Internal: app-fold tracking (mirrors foldedSoNames / foldedSoToAPaths) ---

    /// Set of app .so names currently scheduled for folding into entry.so.
    /// Maintained in sync with appFoldedSoToPaths; used for O(1) membership checks.
    var appFoldedSoNames: HashSet<String> = HashSet<String>()

    /// Maps each folded app .so name to its registered .bc/.a paths.
    /// Required by unfoldAppSo to remove the right entries from appBcPaths.
    let appFoldedSoToPaths: HashMap<String, ArrayList<String>> = HashMap<String, ArrayList<String>>()

    /// Visited set shared between Phase A-2, Round 1 and Round 2 BFS to avoid re-processing.
    var pinnedVisited: HashSet<String> = HashSet<String>()

    public init() {}
}

// ---------------------------------------------------------------------------
// .so / .a name helpers
// ---------------------------------------------------------------------------

/// Return true if `name` looks like a shared-library filename:
///   libfoo.so        — unversioned
///   libfoo.so.0      — versioned (one numeric suffix)
///   libfoo.so.11.3   — versioned (multi-part)
func isSoFile(name: String): Bool {
    if (name.endsWith(".so")) {
        return true
    }
    // Any name that contains ".so." (e.g. libpcre2-8.so.0)
    if (name.contains(".so.")) {
        return true
    }
    return false
}

/// Derive the .a archive name that corresponds to a .so name:
///   libX.so      → libX.a
///   libX.so.Y    → libX.a   (strip the version suffix after ".so.")
func soNameToAName(soName: String): String {
    if (soName.endsWith(".so")) {
        return soName.removeSuffix(".so") + ".a"
    }
    // Versioned form: split on ".so." and take the first segment
    let parts = soName.split(".so.")
    if (parts.size >= 2) {
        return parts[0] + ".a"
    }
    return soName + ".a"  // should not happen for a valid .so name
}

// ---------------------------------------------------------------------------
// .bc / .a discovery rules
// ---------------------------------------------------------------------------

/// Rule A: libcangjie-std-X.a  →  stdBcDir/libstd.X.bc
///
///   Strip "libcangjie-"  →  "std-X"
///   Replace all '-' with '.'  →  "std.X"
///   Prepend "lib"             →  "libstd.X.bc"
///
/// Only applies to non-FFI std archives (base name must NOT end in "FFI").
/// `stdBcDir` should point to the SDK std bitcode directory:
///   build-tools/modules/linux_ohos_aarch64_cjnative/std/
/// Returns the full path if the .bc file exists, None otherwise.
func findBcForA(stdBcDir: String, aName: String): Option<String> {
    let base = aName.removeSuffix(".a")
    if (!base.startsWith("libcangjie-std-") || base.endsWith("FFI")) {
        return None
    }
    let suffix = base.removePrefix("libcangjie-")       // "std-X"
    let bcName = "lib" + suffix.replace("-", ".") + ".bc"  // "libstd.X.bc"
    let bcPath = Path(stdBcDir).join(bcName).toString()
    if (fileExists(bcPath)) {
        return Some(bcPath)
    }
    return None
}

/// Companion FFI archive for a system .a:
///   libcangjie-std-X.a  →  stdBcDir/libcangjie-std-XFFI.a
func findFfiForA(stdBcDir: String, aName: String): Option<String> {
    let base = aName.removeSuffix(".a")
    let ffiPath = Path(stdBcDir).join(base + "FFI.a").toString()
    if (fileExists(ffiPath)) {
        return Some(ffiPath)
    }
    return None
}

/// Rule C / Rule D: app/libX.so  →  appBcDir/X.bc  (or libstdx.xxx.so → libstdx.xxx.bc)
///
///   Rule D: libstdx.xxx.so  →  libstdx.xxx.bc   (keep full name, swap extension)
///   Rule C: libX.so         →  X.bc              (strip "lib" prefix)
///
/// `bcIndex` is a map from bare filename (e.g. "foo.bc") to full path, built by
/// `collectAppBcFiles` from a recursive walk of appBcDir.  This handles the case
/// where sub-packages place their .bc files under per-package subdirectories.
/// Returns the full .bc path if found, None otherwise.
func findBcForAppSo(bcIndex: HashMap<String, String>, soName: String): Option<String> {
    let stem = soName.removeSuffix(".so")
    // Rule D: libstdx.* namespace
    if (soName.startsWith("libstdx.") && soName.endsWith(".so")) {
        let bcName = stem + ".bc"
        if (let Some(p) <- bcIndex.get(bcName)) {
            return Some(p)
        }
    }
    // Rule C: strip "lib" prefix
    let bcName = stem.removePrefix("lib") + ".bc"
    if (let Some(p) <- bcIndex.get(bcName)) {
        return Some(p)
    }
    return None
}

/// Companion FFI archive for an app-side .bc:
///   <some-subdir>/foo.bc  →  <same-subdir>/fooFFI.a
/// The FFI archive lives next to the .bc file, not necessarily in the root appBcDir.
func findFfiForAppBc(_: String, bcPath: String): Option<String> {
    let bcName = Path(bcPath).fileName        // e.g. "foo.bc" or "libstdx.io.bc"
    let ffiPath = Path(bcPath).parent.join(bcName.removeSuffix(".bc") + "FFI.a").toString()
    if (fileExists(ffiPath)) {
        return Some(ffiPath)
    }
    return None
}

/// Recursively walk `bcDir` and collect every `.bc` file into a map of
/// bare filename → full path.  When sub-packages place their bitcode under
/// per-package subdirectories (e.g. `appBcDir/<pkgName>/foo.bc`) this map
/// allows callers to find a `.bc` by name without knowing the subdirectory.
///
/// If two files share the same basename the last one wins (in practice every
/// Cangjie package produces a uniquely named `.bc`).
func collectAppBcFiles(bcDir: String): HashMap<String, String> {
    let index = HashMap<String, String>()
    if (!directoryExists(bcDir)) { return index }
    Directory.walk(bcDir) { info =>
        if (info.isRegular()) {
            let name = info.path.fileName
            if (name.endsWith(".bc")) {
                index[name] = info.path.toString()
            }
        }
    }
    return index
}

// ---------------------------------------------------------------------------
// C++ bitcode detection in .a archives
// ---------------------------------------------------------------------------

/// Return true if `archivePath` contains at least one `.o` member whose first
/// bytes are LLVM bitcode magic (i.e., compiled by clang/clang++ with
/// -emit-llvm and therefore incompatible with Cangjie LTO).
///
/// Strategy: find the first `.o` member name, extract just that member to a
/// temporary directory, check its magic bytes, then remove the temp dir.
func archiveHasCppBitcode(tools: LibFoldTools, archivePath: String): Bool {
    // Find the first .o member
    let members = listArchiveMembers(tools.llvmAr, archivePath)
    var firstObj = ""
    for (member in members) {
        if (member.endsWith(".o")) {
            firstObj = member
            break
        }
    }
    if (firstObj.isEmpty()) {
        return false
    }

    // Extract only that member to a throw-away temp dir
    let tempDir = archivePath + ".cppbc_tmp"
    if (!createDirectory(tempDir)) {
        return false
    }

    let args = ArrayList<String>()
    args.add("x")
    args.add(canonicalize(Path(archivePath)).toString())
    args.add(firstObj)
    let (ok, _, _) = execWithOutput(tools.llvmAr, args,
                                    workingDirectory: Some(Path(tempDir)))

    var result = false
    if (ok) {
        let objPath = Path(tempDir).join(firstObj).toString()
        if (let Some(hdr) <- readFirstBytes(objPath, 4)) {
            result = isBitcodeMagic(hdr)
        }
    }
    deleteDirectory(tempDir)
    return result
}

// ---------------------------------------------------------------------------
// Fold / keep registration
// ---------------------------------------------------------------------------

/// Register `aPaths` as the set of .a/.bc files that represent the folded
/// `soName`, and add them to plan.ltoAPaths.
/// This bookkeeping is required by un-fold to reverse the operation later.
func registerFold(plan: LibFoldPlan, soName: String,
                  aPaths: Array<String>): Unit {
    plan.foldedSoNames.add(soName)
    let lst = ArrayList<String>()
    for (p in aPaths) {
        plan.ltoAPaths.add(p)
        lst.add(p)
    }
    plan.foldedSoToAPaths[soName] = lst
}

// ---------------------------------------------------------------------------
// Phase A: scan app/
// ---------------------------------------------------------------------------

/// Scan `cfg.appDir` and classify each .so as fold (has matching .bc) or keep.
/// Also locates the mandatory entry .bc (ohos_app_cangjie_entry.bc).
///
/// Populates:
///   plan.appBcPaths     — [entry.bc, ...fold bc/FFI.a paths...]
///   plan.appNoBcSoNames — names of .so files with no matching .bc
func scanAppDir(cfg: LibFoldConfig, tools: LibFoldTools,
                targetTriple: String, sanitizedFfiDir: String,
                plan: LibFoldPlan): Bool {
    // Recursively collect all .bc files under appBcDir into a name→path index.
    // Sub-packages may place their .bc files under per-package subdirectories,
    // e.g. appBcDir/<pkgName>/foo.bc, so a flat lookup would miss them.
    let bcIndex = collectAppBcFiles(cfg.appBcDir)

    // The entry .bc is mandatory; everything else is optional.
    let entryBcName = "ohos_app_cangjie_entry.bc"
    let entryBc = bcIndex.get(entryBcName) ?? Path(cfg.appBcDir).join(entryBcName).toString()
    if (!fileExists(entryBc)) {
        eprintln("[lib-fold] ERROR: entry bc not found: ${entryBc}")
        return false
    }
    plan.appBcPaths.add(entryBc)
    logInfo(cfg, "[lib-fold]   fold(entry): ohos_app_cangjie_entry.bc")

    if (!directoryExists(cfg.appDir)) {
        logInfo(cfg, "[lib-fold]   app-dir not found, skipping app scan")
        return true
    }

    try {
        for (info in Directory.readFrom(Path(cfg.appDir))) {
            if (!info.isRegular()) { continue }
            let soName = info.name
            if (!soName.endsWith(".so")) { continue }  // app libs have no version suffix

            if (let Some(bcPath) <- findBcForAppSo(bcIndex, soName)) {
                let lst = ArrayList<String>()
                lst.add(bcPath)
                plan.appBcPaths.add(bcPath)
                var logSuffix = ""
                // Check for a companion FFI archive
                if (let Some(rawFfi) <- findFfiForAppBc(cfg.appBcDir, bcPath)) {
                    let sanitized = sanitizeFfiArchive(
                        tools, rawFfi, targetTriple, sanitizedFfiDir) ?? rawFfi
                    plan.appBcPaths.add(sanitized)
                    lst.add(sanitized)
                    logSuffix = " + ${Path(sanitized).fileName}"
                }
                plan.appFoldedSoNames.add(soName)
                plan.appFoldedSoToPaths[soName] = lst
                logInfo(cfg, "[lib-fold]   fold: ${soName} → ${Path(bcPath).fileName}${logSuffix}")
            } else {
                plan.appNoBcSoNames.add(soName)
                logInfo(cfg, "[lib-fold]   keep: ${soName}")
            }
        }
    } catch (_: Exception) {
        eprintln("[lib-fold] ERROR: cannot read app-dir: ${cfg.appDir}")
        return false
    }

    logInfo(cfg, "[lib-fold]   fold=${plan.appBcPaths.size}  keep=${plan.appNoBcSoNames.size}")
    return true
}

// ---------------------------------------------------------------------------
// BFS helpers
// ---------------------------------------------------------------------------

/// Return the full path to `soName` if it exists in appDir or libDir.
func findSoPath(cfg: LibFoldConfig, soName: String): Option<String> {
    let appPath = Path(cfg.appDir).join(soName).toString()
    if (fileExists(appPath)) { return Some(appPath) }
    let libPath = Path(cfg.libDir).join(soName).toString()
    if (fileExists(libPath)) { return Some(libPath) }
    return None
}

// ---------------------------------------------------------------------------
// Phase A-2: app no-bc BFS + un-fold
// ---------------------------------------------------------------------------

/// Remove a .bc app lib from the fold set and restore it as a standalone .so.
///
/// Mirrors unfoldSo for the app side: a .bc app lib scheduled for LTO-merging
/// into entry.so must remain as a standalone file when a no-.bc app lib holds
/// a hardcoded DT_NEEDED entry pointing to it.  The runtime dynamic linker
/// resolves NEEDED entries by filename — a symbol merged into entry.so does
/// not satisfy a DT_NEEDED: libA.so demand.
///
/// Steps:
///   1. Remove soName from appFoldedSoNames.
///   2. Remove its registered .bc/.a paths from appBcPaths.
///   3. Add soName to appNoBcSoNames so it is linked (-l:) in Pass 1 and
///      copied to output_app/ alongside entry.so.
func unfoldAppSo(cfg: LibFoldConfig, soName: String, plan: LibFoldPlan): Unit {
    if (!plan.appFoldedSoNames.contains(soName)) { return }

    // Step 1: remove from fold set
    let newFolded = HashSet<String>()
    for (n in plan.appFoldedSoNames) {
        if (n != soName) { newFolded.add(n) }
    }
    plan.appFoldedSoNames = newFolded

    // Step 2: remove associated .bc/.a paths from appBcPaths
    if (plan.appFoldedSoToPaths.contains(soName)) {
        let toRemove = HashSet<String>()
        for (p in plan.appFoldedSoToPaths[soName]) { toRemove.add(p) }
        let newBcPaths = ArrayList<String>()
        for (p in plan.appBcPaths) {
            if (!toRemove.contains(p)) { newBcPaths.add(p) }
        }
        plan.appBcPaths = newBcPaths
    }

    // Step 3: keep as standalone — linked in Pass 1, copied to output
    plan.appNoBcSoNames.add(soName)

    logInfo(cfg, "[lib-fold]   un-fold(app): ${soName}  ← required as standalone by no-bc chain")
}

/// Phase A-2: starting from every no-.bc app lib, trace transitive DT_NEEDED
/// dependencies within app/ and un-fold any .bc app libs that are reachable.
///
/// When a .bc app lib (libA.so) is transitively needed by a no-.bc app lib
/// (libB.so), libA.so must remain as a standalone .so file in the output.
/// Without this, libA.bc would be merged into entry.so and libA.so would
/// disappear — causing a runtime DT_NEEDED resolution failure for libB.so.
///
/// After un-folding, libA.so ends up in appNoBcSoNames, so:
///   - Pass 1 links against it (-l:libA.so), giving entry.so DT_NEEDED: libA.so
///   - copyNoBcAppLibs copies libA.so to output_app/
///   - buildVersionScript collects libA.so's undefined symbols so the new
///     ark_interop.so exports everything libA.so needs at runtime
///
/// Shares pinnedVisited with pinnedBfsRound1 / pinnedBfsRound2.
func appNoBcBfs(cfg: LibFoldConfig, tools: LibFoldTools, plan: LibFoldPlan): Unit {
    let queue = ArrayList<String>()
    for (name in plan.appNoBcSoNames) {
        queue.add(name)
    }

    var head = 0
    while (head < queue.size) {
        let cur = queue[head]
        head += 1
        if (plan.pinnedVisited.contains(cur)) { continue }
        plan.pinnedVisited.add(cur)

        let curPath = Path(cfg.appDir).join(cur).toString()
        if (!fileExists(curPath)) { continue }

        for (dep in getNeeded(tools, curPath)) {
            if (plan.pinnedVisited.contains(dep)) { continue }
            // Only care about deps that live in app/ (system libs are on-device)
            let appDep = Path(cfg.appDir).join(dep).toString()
            if (!fileExists(appDep)) { continue }

            if (plan.appFoldedSoNames.contains(dep)) {
                unfoldAppSo(cfg, dep, plan)
            }
            // Continue BFS: the un-folded lib may itself depend on further .bc app libs
            queue.add(dep)
            logInfo(cfg, "[lib-fold]   app-chain: ${dep}  ← needed by ${cur}")
        }
    }
}

// ---------------------------------------------------------------------------
// Phase B: Round 1 PINNED BFS
// ---------------------------------------------------------------------------

/// Starting from every no-.bc app .so, trace all transitive DT_NEEDED
/// dependencies that live in lib/ and mark them as PINNED.
///
/// A PINNED lib must remain a standalone .so file because a no-bc app .so
/// has a hardcoded ELF NEEDED entry that the runtime dynamic linker must
/// satisfy by finding an actual file — it cannot be satisfied by a symbol
/// that was merged into ark_interop.so.
///
/// Extends:
///   plan.excludeSet    — PINNED names are added here (prevents later folding)
///   plan.pinnedVisited — shared with Round 2
func pinnedBfsRound1(cfg: LibFoldConfig, tools: LibFoldTools,
                     plan: LibFoldPlan): Unit {
    // Seed the queue with all no-.bc app .so names.
    let queue = ArrayList<String>()
    for (name in plan.appNoBcSoNames) {
        queue.add(name)
    }

    var head = 0
    while (head < queue.size) {
        let cur = queue[head]
        head += 1
        if (plan.pinnedVisited.contains(cur)) { continue }
        plan.pinnedVisited.add(cur)

        let curPath = findSoPath(cfg, cur) ?? continue

        for (dep in getNeeded(tools, curPath)) {
            if (plan.pinnedVisited.contains(dep)) { continue }
            // Only pin deps that exist in lib/ (ohos/ libs are on-device).
            let libDep = Path(cfg.libDir).join(dep).toString()
            if (!fileExists(libDep)) { continue }

            plan.excludeSet.add(dep)
            queue.add(dep)
            logInfo(cfg, "[lib-fold]   pinned: ${dep}  ← needed by ${cur} (no-bc chain)")
        }
    }
}

// ---------------------------------------------------------------------------
// Phase C + D: scan lib/
// ---------------------------------------------------------------------------

/// Scan modulesDir for `libohos.*.a` core archives and fold them first,
/// then scan libDir for all .so/.so.X files and classify each as fold or keep.
///
/// Populates:
///   plan.ltoAPaths     — .bc/.a paths for the cjc LTO pass
///   plan.noASoPaths    — full paths of .so files to copy as standalone outputs
///   plan.foldedSoNames — names of .so files that have been folded
func scanLibDir(cfg: LibFoldConfig, tools: LibFoldTools,
                targetTriple: String, sanitizedFfiDir: String,
                plan: LibFoldPlan): Bool {
    // --- Phase C: pre-fold libohos.*.a (pure-bitcode core archives) ---
    //
    // These archives ARE bitcode files (not ar archives), so `ar t` fails on
    // them.  We detect them by filename pattern and fold them directly.
    // They live in cfg.ohosADir = api/lib/linux_ohos_aarch64_cjnative/ohos/
    if (directoryExists(cfg.ohosADir)) {
        try {
            for (info in Directory.readFrom(Path(cfg.ohosADir))) {
                if (!info.isRegular()) { continue }
                let aName = info.name
                if (!aName.startsWith("libohos.") || !aName.endsWith(".a")) { continue }
                let aPath = Path(cfg.ohosADir).join(aName).toString()
                let soName = aName.removeSuffix(".a") + ".so"
                registerFold(plan, soName, [aPath])
                logInfo(cfg, "[lib-fold]   fold(core): ${aName}")
            }
        } catch (_: Exception) {
            eprintln("[lib-fold] ERROR: cannot read ohos-a-dir: ${cfg.ohosADir}")
            return false
        }
    }

    // --- Phase D: scan lib/ ---
    if (!directoryExists(cfg.libDir)) {
        eprintln("[lib-fold] ERROR: lib-dir not found: ${cfg.libDir}")
        return false
    }

    try {
        for (info in Directory.readFrom(Path(cfg.libDir))) {
            if (!info.isRegular()) { continue }
            let soName = info.name
            if (!isSoFile(soName)) { continue }
            let soPath = Path(cfg.libDir).join(soName).toString()

            // Already excluded (strict-keep or PINNED from Round 1)
            if (plan.excludeSet.contains(soName)) {
                plan.noASoPaths.add(soPath)
                logInfo(cfg, "[lib-fold]   skip(exclude): ${soName}")
                continue
            }

            // Already folded by the core pre-scan (libohos.*.a)
            if (plan.foldedSoNames.contains(soName)) {
                continue
            }

            let aName = soNameToAName(soName)
            // Rule A: look up .bc in stdBcDir
            let bcOpt = findBcForA(cfg.stdBcDir, aName)
            // Rule A companion FFI: look up FFI .a in stdBcDir
            let ffiOpt = findFfiForA(cfg.stdBcDir, aName)

            if (let Some(bc) <- bcOpt) {
                // fold(bc): Rule A — libcangjie-std-X.a → libstd.X.bc
                var logSuffix = ""
                var foldPaths = [bc]
                if (let Some(rawFfi) <- ffiOpt) {
                    let sanitized = sanitizeFfiArchive(
                        tools, rawFfi, targetTriple, sanitizedFfiDir) ?? rawFfi
                    foldPaths = [bc, sanitized]
                    logSuffix = " + ${Path(sanitized).fileName}"
                }
                registerFold(plan, soName, foldPaths)
                logInfo(cfg, "[lib-fold]   fold(bc): ${soName} → ${Path(bc).fileName}${logSuffix}")
            } else {
                // No Rule-A match; try a direct .a lookup.
                // Check stdBcDir first (non-std .a that happen to live there),
                // then ohosADir (other SDK ohos archives).
                var directA = Path(cfg.stdBcDir).join(aName).toString()
                if (!fileExists(directA)) {
                    directA = Path(cfg.ohosADir).join(aName).toString()
                }
                if (fileExists(directA)) {
                    // If the .a contains C++ LLVM bitcode, Cangjie LTO will
                    // reject it — degrade to keep.
                    if (archiveHasCppBitcode(tools, directA)) {
                        plan.noASoPaths.add(soPath)
                        logInfo(cfg, "[lib-fold]   keep(cpp-bc): ${soName} (C++ bitcode in ${aName})")
                    } else {
                        registerFold(plan, soName, [directA])
                        logInfo(cfg, "[lib-fold]   fold(a):  ${soName} → ${aName}")
                    }
                } else {
                    // No .bc and no .a — must remain as a standalone .so.
                    plan.noASoPaths.add(soPath)
                    logInfo(cfg, "[lib-fold]   keep:     ${soName}")
                }
            }
        }
    } catch (_: Exception) {
        eprintln("[lib-fold] ERROR: cannot read lib-dir: ${cfg.libDir}")
        return false
    }

    return true
}

// ---------------------------------------------------------------------------
// Phase E: strict-keep safety check
// ---------------------------------------------------------------------------

/// Verify that none of the strictKeep libs were accidentally folded.
/// These libs (Cangjie runtime, boundscheck, pcre2) must always exist as
/// standalone .so files; folding them would cause a runtime crash.
func checkStrictKeep(cfg: LibFoldConfig, plan: LibFoldPlan): Bool {
    for (name in cfg.strictKeep) {
        if (plan.foldedSoNames.contains(name)) {
            eprintln("[lib-fold] ERROR: runtime lib incorrectly folded: ${name}")
            return false
        }
    }
    return true
}

// ---------------------------------------------------------------------------
// Phase F: Round 2 PINNED BFS + un-fold
// ---------------------------------------------------------------------------

/// Remove `soName` from the fold set and restore it as a standalone .so.
///
/// This is the "un-fold" operation: a lib that was originally classified as
/// fold(bc/a) must be reverted when a no-bc lib is discovered to NEED it via
/// a hardcoded ELF NEEDED entry.  The dynamic linker must find the .so as a
/// real file — a symbol merged into ark_interop.so is not sufficient.
///
/// Steps:
///   1. Remove soName from foldedSoNames.
///   2. Remove its registered .a/.bc paths from ltoAPaths.
///   3. Add lib/<soName> to noASoPaths.
///   4. Add to excludeSet to prevent future re-folding.
func unfoldSo(cfg: LibFoldConfig, soName: String,
              plan: LibFoldPlan): Unit {
    if (!plan.foldedSoNames.contains(soName)) { return }

    // Step 1: remove from fold set (rebuild to avoid mutation issues)
    let newFolded = HashSet<String>()
    for (n in plan.foldedSoNames) {
        if (n != soName) {
            newFolded.add(n)
        }
    }
    plan.foldedSoNames = newFolded

    // Step 2: remove associated .a/.bc paths from ltoAPaths
    if (plan.foldedSoToAPaths.contains(soName)) {
        let toRemove = HashSet<String>()
        for (p in plan.foldedSoToAPaths[soName]) {
            toRemove.add(p)
        }
        let newLtoA = ArrayList<String>()
        for (p in plan.ltoAPaths) {
            if (!toRemove.contains(p)) {
                newLtoA.add(p)
            }
        }
        plan.ltoAPaths = newLtoA
    }

    // Step 3: add to the standalone .so list
    let soPath = Path(cfg.libDir).join(soName).toString()
    if (fileExists(soPath)) {
        plan.noASoPaths.add(soPath)
    }

    // Step 4: exclude from future folds
    plan.excludeSet.add(soName)

    logInfo(cfg, "[lib-fold]   un-fold: ${soName}  ← required as standalone by no-bc chain")
}

/// Round 2 BFS: starting from the no-bc lib/ .so files (those with no .a),
/// trace transitive DT_NEEDED dependencies.
///
/// If a transitive dependency was already folded into ark_interop.so, un-fold
/// it — because the no-bc .so has a hardcoded ELF NEEDED entry that cannot be
/// resolved by a merged symbol at runtime.
///
/// Shares plan.pinnedVisited with Round 1 to avoid reprocessing nodes.
func pinnedBfsRound2(cfg: LibFoldConfig, tools: LibFoldTools,
                     plan: LibFoldPlan): Unit {
    // Seed with all lib/ .so files that are currently in noASoPaths
    // and have not yet been visited.
    let queue = ArrayList<String>()
    for (soPath in plan.noASoPaths) {
        let name = Path(soPath).fileName
        if (!plan.pinnedVisited.contains(name)) {
            queue.add(name)
        }
    }

    var head = 0
    while (head < queue.size) {
        let cur = queue[head]
        head += 1
        if (plan.pinnedVisited.contains(cur)) { continue }
        plan.pinnedVisited.add(cur)

        let curPath = findSoPath(cfg, cur) ?? continue

        for (dep in getNeeded(tools, curPath)) {
            if (plan.pinnedVisited.contains(dep)) { continue }
            let libDep = Path(cfg.libDir).join(dep).toString()
            if (!fileExists(libDep)) { continue }  // on-device lib, not our concern

            if (plan.foldedSoNames.contains(dep)) {
                // This dep was going to be merged — but a no-bc .so's NEEDED
                // entry demands it be a real standalone file at runtime.
                unfoldSo(cfg, dep, plan)
            }
            // Whether we un-folded or not, follow this dep's own deps.
            queue.add(dep)
            logInfo(cfg, "[lib-fold]   pinned(lib-chain): ${dep}  ← needed by ${cur}")
        }
    }
}

// ---------------------------------------------------------------------------
// Orchestrator
// ---------------------------------------------------------------------------

/// Run all four analysis phases and return the complete build plan.
///
/// Returns None on any hard error (missing entry .bc, unreadable directories,
/// strict-keep violation).
public func buildLibFoldPlan(cfg: LibFoldConfig, tools: LibFoldTools,
                          targetTriple: String,
                          sanitizedFfiDir: String): Option<LibFoldPlan> {
    let plan = LibFoldPlan()

    // Pre-populate excludeSet with strict-keep lib names so that Phase D
    // never attempts to fold them.
    for (name in cfg.strictKeep) {
        plan.excludeSet.add(name)
    }

    logInfo(cfg, "[lib-fold] [scan] Phase A: scanning app/ ...")
    if (!scanAppDir(cfg, tools, targetTriple, sanitizedFfiDir, plan)) {
        return None
    }

    logInfo(cfg, "[lib-fold] [scan] Phase A-2: app no-bc dependency BFS ...")
    appNoBcBfs(cfg, tools, plan)

    logInfo(cfg, "[lib-fold] [scan] Phase B: Round-1 PINNED BFS ...")
    pinnedBfsRound1(cfg, tools, plan)

    logInfo(cfg, "[lib-fold] [scan] Phase C+D: scanning lib/ ...")
    if (!scanLibDir(cfg, tools, targetTriple, sanitizedFfiDir, plan)) {
        return None
    }

    logInfo(cfg, "[lib-fold] [scan] Phase E: strict-keep safety check ...")
    if (!checkStrictKeep(cfg, plan)) {
        return None
    }

    logInfo(cfg, "[lib-fold] [scan] Phase F: Round-2 PINNED BFS + un-fold ...")
    pinnedBfsRound2(cfg, tools, plan)

    logInfo(cfg,
        "[lib-fold] [scan] done:" +
        "  fold_app=${plan.appBcPaths.size}" +
        "  keep_app=${plan.appNoBcSoNames.size}" +
        "  fold_lib=${plan.ltoAPaths.size}" +
        "  keep_lib=${plan.noASoPaths.size}")
    return Some(plan)
}
