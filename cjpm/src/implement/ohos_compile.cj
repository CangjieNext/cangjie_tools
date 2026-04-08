// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.fs.*
import std.env.*
import cjpm.config.*

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

let ENTRY_SO_NAME: String = "libohos_app_cangjie_entry.so"

// ---------------------------------------------------------------------------
// File utilities
// ---------------------------------------------------------------------------

/// Binary-copy src to dst using File.readFrom / File.writeTo.
/// Overwrites dst if it already exists.
func copyFile(src: String, dst: String): Bool {
    try {
        let data = File.readFrom(Path(src))
        File.writeTo(Path(dst), data)
        return true
    } catch (_: Exception) {
        return false
    }
}

// ---------------------------------------------------------------------------
// Step 5-a: Collect undefined dynamic symbols
// ---------------------------------------------------------------------------

/// Collect undefined dynamic symbol names from `soPath` using
///   llvm-nm -D -u <soPath>
///
/// Output format per line:  "<spaces> U <symbol>[@VERSION]"
/// - Skip lines ending with ":" (object file headers in the nm output).
/// - Take the last whitespace-delimited token.
/// - Strip any @VERSION suffix.
func collectUndefinedSymbols(tools: OhosTools, soPath: String): Array<String> {
    if (!fileExists(soPath)) { return Array<String>() }

    let args = ArrayList<String>()
    args.add("-D")
    args.add("-u")
    args.add(soPath)
    let (ok, stdout, _) = execWithOutput(tools.llvmNm, args)
    if (!ok || stdout.isEmpty()) { return Array<String>() }

    let syms = ArrayList<String>()
    for (rawLine in stdout.split("\n")) {
        let line = rawLine.trimAscii()
        if (line.isEmpty() || line.endsWith(":")) { continue }

        // Take the last whitespace-delimited token
        var sym = ""
        for (part in line.split(" ")) {
            let p = part.trimAscii()
            if (!p.isEmpty()) { sym = p }
        }
        if (sym.isEmpty()) { continue }

        // Strip @VERSION suffix (e.g. _ZN...@GLIBCXX_3.4 → _ZN...)
        if (let Some(atIdx) <- sym.indexOf(UInt8(UInt32(r'@')), 0)) {
            sym = sym[0..atIdx]
        }
        if (!sym.isEmpty()) {
            syms.add(sym)
        }
    }
    return syms.toArray()
}

// ---------------------------------------------------------------------------
// Step 5-b: Build version script
// ---------------------------------------------------------------------------

/// Generate the ld version script that precisely controls which symbols
/// ark_interop.so exports.
///
/// Symbol set = undefined symbols from Pass-1 entry.so (after LTO DCE the
/// dead symbols are gone, so this is the exact set the app will resolve at
/// runtime) UNION undefined symbols from no-.bc app libs (they also rely on
/// interop.so at runtime).
///
/// The resulting version_script.map has the form:
///   {
///    global:
///    "sym1";
///    ...
///    local: *;
///   };
public func buildVersionScript(tools: OhosTools, cfg: OhosBundleConfig,
                                plan: OhosBuildPlan,
                                entrySOPath: String, mapFile: String): Bool {
    // Collect from Pass-1 entry.so
    let symSet = HashSet<String>()
    for (s in collectUndefinedSymbols(tools, entrySOPath)) {
        symSet.add(s)
    }
    // Collect from no-.bc app libs (they run against interop.so at runtime)
    for (soName in plan.appNoBcSoNames) {
        let soPath = Path(cfg.appDir).join(soName).toString()
        for (s in collectUndefinedSymbols(tools, soPath)) {
            symSet.add(s)
        }
    }

    // Build the map content (order of symbols is unimportant for correctness)
    var mapContent = "{\n global:\n"
    for (sym in symSet) {
        mapContent += " \"${sym}\";\n"
    }
    mapContent += " local: *;\n};\n"

    try {
        File.writeTo(Path(mapFile), mapContent.toArray())
        logInfo(cfg, "[ohos-bundle] [step5] version script: ${symSet.size} symbols → ${mapFile}")
        return true
    } catch (_: Exception) {
        eprintln("[ohos-bundle] ERROR: cannot write version script: ${mapFile}")
        return false
    }
}

// ---------------------------------------------------------------------------
// Step 6 helpers: link-arg builders
// ---------------------------------------------------------------------------

/// Build  -L <dir> -l:<name>  args for every entry in `noASoPaths`.
/// Each path contributes its parent directory and filename.
func buildNoASoLinkArgs(noASoPaths: ArrayList<String>): ArrayList<String> {
    let args = ArrayList<String>()
    for (soPath in noASoPaths) {
        let dir  = Path(soPath).parent.toString()
        let name = Path(soPath).fileName
        args.add("-L"); args.add(dir)
        args.add("-l:${name}")
    }
    return args
}

/// Build  -L <appDir> -l:<name>...  for no-.bc app .so files.
/// Returns empty if there are no such files.
func buildNoBcAppLinkArgs(cfg: OhosBundleConfig,
                          plan: OhosBuildPlan): ArrayList<String> {
    let args = ArrayList<String>()
    if (plan.appNoBcSoNames.isEmpty()) { return args }
    args.add("-L"); args.add(cfg.appDir)
    for (name in plan.appNoBcSoNames) {
        args.add("-l:${name}")
    }
    return args
}

/// Collect DT_NEEDED entries from folded system libs (and the original
/// interop.so) that live in ohos/ or the sysroot, not in lib/.
/// Returns a dir → lib-name list map for building -L/-l linker args.
func collectExternalNeeded(tools: OhosTools, cfg: OhosBundleConfig,
                           plan: OhosBuildPlan,
                           targetTriple: String): HashMap<String, ArrayList<String>> {
    let sysrootLib = if (!cfg.sysroot.isEmpty()) {
        Path(cfg.sysroot).join("usr").join("lib").join(targetTriple).toString()
    } else { "" }

    let seen    = HashSet<String>()
    let dirLibs = HashMap<String, ArrayList<String>>()

    // Sources: all folded lib/.so files + the original interop placeholder
    let sources = ArrayList<String>()
    for (soName in plan.foldedSoNames) {
        let p = Path(cfg.libDir).join(soName).toString()
        if (fileExists(p)) { sources.add(p) }
    }
    let interopPath = Path(cfg.libDir).join(cfg.interopSo).toString()
    if (fileExists(interopPath)) { sources.add(interopPath) }

    for (soPath in sources) {
        for (dep in getNeeded(tools, soPath)) {
            if (seen.contains(dep)) { continue }
            seen.add(dep)
            if (plan.foldedSoNames.contains(dep)) { continue }
            // Already handled by NO_A_SO_INPUTS
            if (fileExists(Path(cfg.libDir).join(dep).toString())) { continue }

            // Try ohos/ first, then sysroot
            let ohosPath = Path(cfg.ohosDir).join(dep).toString()
            if (!cfg.ohosDir.isEmpty() && fileExists(ohosPath)) {
                if (!dirLibs.contains(cfg.ohosDir)) {
                    dirLibs[cfg.ohosDir] = ArrayList<String>()
                }
                dirLibs[cfg.ohosDir].add(dep)
            } else if (!sysrootLib.isEmpty() &&
                       fileExists(Path(sysrootLib).join(dep).toString())) {
                if (!dirLibs.contains(sysrootLib)) {
                    dirLibs[sysrootLib] = ArrayList<String>()
                }
                dirLibs[sysrootLib].add(dep)
            }
        }
    }
    return dirLibs
}

/// Build  -L <dir> -l:<name>...  from the dir → lib-name map.
func buildOhosLinkArgs(dirLibs: HashMap<String, ArrayList<String>>): ArrayList<String> {
    let args = ArrayList<String>()
    for ((dir, libs) in dirLibs) {
        args.add("-L"); args.add(dir)
        for (lib in libs) {
            args.add("-l:${lib}")
        }
    }
    return args
}

// ---------------------------------------------------------------------------
// Step 6 helpers: common cjc link flags
// ---------------------------------------------------------------------------

/// Build the --link-option flags shared by both cjc passes:
///   --gc-sections, --strip-debug, -rpath $ORIGIN,
///   --no-dynamic-linker, -L <sysrootLib>
///
/// Omitted (already lld defaults or auto-injected by cjc):
///   -z combreloc / -z text / -z noexecstack / --hash-style / -z now / -z norelro
///   --as-needed / --allow-shlib-undefined / --allow-multiple-definition
///   --lto-O2 / --mllvm flags / -shared / -T cjld.shared.lds / cjstart.o
func buildCommonLinkArgs(cfg: OhosBundleConfig, targetTriple: String): Array<String> {
    let sysrootLib = if (!cfg.sysroot.isEmpty()) {
        Path(cfg.sysroot).join("usr").join("lib").join(targetTriple).toString()
    } else { "" }

    let args = ArrayList<String>()
    let linkOpts: Array<String> = [
        "--gc-sections", "--strip-debug",
        "-rpath", "$ORIGIN",
        "--no-dynamic-linker"
    ]
    for (opt in linkOpts) {
        args.add("--link-option"); args.add(opt)
    }
    if (!sysrootLib.isEmpty()) {
        args.add("--link-option"); args.add("-L")
        args.add("--link-option"); args.add(sysrootLib)
    }
    return args.toArray()
}

// ---------------------------------------------------------------------------
// Step 6-a: cjc Pass 1 (old placeholder interop → entry.so)
// ---------------------------------------------------------------------------

/// Build the cjc argument list for Pass 1.
/// Uses --compile-as-exe so DCE roots are driven by cjstart.o + @llvm.used.
func buildPass1Args(cfg: OhosBundleConfig, targetTriple: String,
                    plan: OhosBuildPlan, dummyCj: String,
                    commonLinkArgs: Array<String>,
                    noASoArgs: Array<String>,
                    noBcAppArgs: Array<String>): Array<String> {
    let sysrootLib = if (!cfg.sysroot.isEmpty()) {
        Path(cfg.sysroot).join("usr").join("lib").join(targetTriple).toString()
    } else { "" }

    let args = ArrayList<String>()

    // Sources: dummy .cj placeholder + all app LTO .bc paths
    args.add(dummyCj)
    for (bc in plan.appBcPaths) { args.add(bc) }

    // Target / sysroot
    args.add("--target"); args.add(targetTriple)
    if (!cfg.sysroot.isEmpty()) {
        args.add("--sysroot"); args.add(cfg.sysroot)
        args.add("-B"); args.add(sysrootLib)
    }

    // LTO mode: --compile-as-exe sets DCE root = cjstart.o refs + @llvm.used
    args.add("--compile-as-exe")
    args.add("--output-type=dylib")
    args.add("--lto=full")

    // Common link flags + pass-specific soname
    for (a in commonLinkArgs) { args.add(a) }
    args.add("--link-option"); args.add("-soname")
    args.add("--link-option"); args.add(ENTRY_SO_NAME)

    // Library search: lib/ dir + placeholder interop
    args.add("-L"); args.add(cfg.libDir)
    args.add("-l:${cfg.interopSo}")

    // No-.a standalone system libs
    for (a in noASoArgs) { args.add(a) }

    // No-.bc app libs
    for (a in noBcAppArgs) { args.add(a) }

    // Output
    args.add("-o")
    args.add(Path(cfg.outputApp).join(ENTRY_SO_NAME).toString())

    return args.toArray()
}

/// Write the dummy Cangjie source file required by cjc.
/// cjc refuses to compile when there are no .cj input files even if .bc
/// files supply all the real code; this stub satisfies that requirement.
func writeDummyCj(path: String): Bool {
    let content = "package dummy_entry\nmain(): Int64 { return 0 }\n"
    try {
        File.writeTo(Path(path), content.toArray())
        return true
    } catch (_: Exception) {
        eprintln("[ohos-bundle] ERROR: cannot write dummy entry: ${path}")
        return false
    }
}

/// cjc Pass 1: LTO-merge all app .bc files into entry.so using the old
/// placeholder interop.so in lib/.  Because the DCE root set is determined
/// by cjstart.o + @llvm.used (independent of which interop is linked), this
/// single cjc run already produces the final entry.so — no second run needed.
func runPass1(tools: OhosTools, cfg: OhosBundleConfig, targetTriple: String,
              plan: OhosBuildPlan, dummyCj: String,
              commonLinkArgs: Array<String>,
              noASoArgs: Array<String>, noBcAppArgs: Array<String>): Bool {
    logInfo(cfg, "[ohos-bundle] [1/2] cjc → ${ENTRY_SO_NAME} ...")
    let args = buildPass1Args(cfg, targetTriple, plan, dummyCj,
                               commonLinkArgs, noASoArgs, noBcAppArgs)
    return execAndToTerminal(tools.cjc, args)
}

// ---------------------------------------------------------------------------
// Step 6-b: cjc Pass 2 (LTO merge system libs → new ark_interop.so)
// ---------------------------------------------------------------------------

/// cjc Pass 2: LTO-link all system .bc/.a files into the new ark_interop.so,
/// using the version script generated from Pass-1 entry.so's undefined symbols.
///
/// Does NOT use --compile-as-exe — DCE roots come from the version script
/// globals.  Using --compile-as-exe here would switch DCE roots to cjstart.o
/// and eliminate symbols that entry.so needs at runtime.
///
/// cjc auto-injects: -shared / -T cjld.shared.lds / cjstart.o /
///   --mllvm --cangjie-full-lto / --mllvm --cj-lto-opt / --mllvm --cangjie-pipeline /
///   --mllvm --relocation-model=pic / --mllvm --frame-pointer=non-leaf /
///   --mllvm -cj-stack-grow=false / --plugin-opt=no-opaque-pointers /
///   --allow-multiple-definition / --lto-O2 / --as-needed
func runPass2(tools: OhosTools, cfg: OhosBundleConfig, targetTriple: String,
              plan: OhosBuildPlan, dummyCj: String,
              commonLinkArgs: Array<String>,
              noASoArgs: Array<String>, ohosArgs: Array<String>,
              mapFile: String): Bool {
    let outSo = Path(cfg.outputSystem).join(cfg.interopSo).toString()
    logInfo(cfg, "[ohos-bundle] [2/2] cjc → ${cfg.interopSo} (LTO merge) ...")

    let sysrootLib = if (!cfg.sysroot.isEmpty()) {
        Path(cfg.sysroot).join("usr").join("lib").join(targetTriple).toString()
    } else { "" }

    let args = ArrayList<String>()

    // Sources: dummy .cj placeholder + all system LTO .a/.bc paths
    args.add(dummyCj)
    for (a in plan.ltoAPaths) { args.add(a) }

    // Target / sysroot
    args.add("--target"); args.add(targetTriple)
    if (!cfg.sysroot.isEmpty()) {
        args.add("--sysroot"); args.add(cfg.sysroot)
        args.add("-B"); args.add(sysrootLib)
    }

    // LTO mode: no --compile-as-exe, DCE driven by version script
    args.add("--output-type=dylib")
    args.add("--lto=full")

    // Common link flags + pass-specific soname + version script
    for (a in commonLinkArgs) { args.add(a) }
    args.add("--link-option"); args.add("-soname")
    args.add("--link-option"); args.add(cfg.interopSo)
    args.add("--link-option"); args.add("--version-script")
    args.add("--link-option"); args.add(mapFile)

    // No-.a standalone system libs
    for (a in noASoArgs) { args.add(a) }

    // ohos/ / sysroot deps: --no-as-needed so they appear in DT_NEEDED
    if (!ohosArgs.isEmpty()) {
        args.add("--link-option"); args.add("--no-as-needed")
        for (a in ohosArgs) { args.add(a) }
        args.add("--link-option"); args.add("--as-needed")
    }

    args.add("-o"); args.add(outSo)

    return execAndToTerminal(tools.cjc, args.toArray())
}

// ---------------------------------------------------------------------------
// Step 6-c: copy non-folded libs + strict-keep validation
// ---------------------------------------------------------------------------

/// Copy every lib/ .so that was NOT folded into ark_interop.so to outputSystem/.
/// Skips files that already exist at the destination (idempotent).
func copyNonFoldedLibs(cfg: OhosBundleConfig, plan: OhosBuildPlan): Bool {
    logInfo(cfg, "[ohos-bundle] copying non-folded libs → ${cfg.outputSystem}/ ...")
    if (!directoryExists(cfg.libDir)) { return true }
    try {
        for (info in Directory.readFrom(Path(cfg.libDir))) {
            if (!info.isRegular()) { continue }
            let soName = info.name
            if (!isSoFile(soName)) { continue }
            if (plan.foldedSoNames.contains(soName)) { continue }
            let dst = Path(cfg.outputSystem).join(soName).toString()
            if (fileExists(dst)) { continue }
            let src = Path(cfg.libDir).join(soName).toString()
            if (!copyFile(src, dst)) {
                eprintln("[ohos-bundle] ERROR: failed to copy ${soName} to ${cfg.outputSystem}/")
                return false
            }
            logInfo(cfg, "[ohos-bundle]   copy(sys): ${soName}")
        }
    } catch (_: Exception) {
        eprintln("[ohos-bundle] ERROR: cannot read lib-dir during copy: ${cfg.libDir}")
        return false
    }
    return true
}

/// Verify that each strictKeep lib was successfully copied to outputSystem/.
func checkStrictKeepCopied(cfg: OhosBundleConfig): Bool {
    for (name in cfg.strictKeep) {
        let src = Path(cfg.libDir).join(name).toString()
        // If the lib is not present in lib-dir it cannot have been folded,
        // so there is nothing to verify — skip it silently.
        if (!fileExists(src)) { continue }
        let dst = Path(cfg.outputSystem).join(name).toString()
        if (!fileExists(dst)) {
            eprintln("[ohos-bundle] ERROR: required lib not in output_system: ${dst}")
            return false
        }
    }
    return true
}

// ---------------------------------------------------------------------------
// Post-compile: copy no-.bc app libs to output_app/
// ---------------------------------------------------------------------------

/// Copy no-.bc app .so files to outputApp/ so they are alongside entry.so.
func copyNoBcAppLibs(cfg: OhosBundleConfig, plan: OhosBuildPlan): Unit {
    for (soName in plan.appNoBcSoNames) {
        let dst = Path(cfg.outputApp).join(soName).toString()
        if (fileExists(dst)) { continue }
        let src = Path(cfg.appDir).join(soName).toString()
        if (fileExists(src) && copyFile(src, dst)) {
            logInfo(cfg, "[ohos-bundle]   copy(app): ${soName}")
        }
    }
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

/// Remove intermediate files created during compilation.
/// Errors are silently ignored (cleanup is best-effort).
func cleanupIntermediates(cfg: OhosBundleConfig, dummyCj: String,
                          appNeedsFile: String, mapFile: String): Unit {
    for (path in [dummyCj, appNeedsFile, mapFile]) {
        if (!path.isEmpty()) {
            deleteFile(path)
        }
    }
    // Remove cjc-generated .cjo / .cjo.flag intermediate objects from outputApp/
    if (directoryExists(cfg.outputApp)) {
        try {
            for (info in Directory.readFrom(Path(cfg.outputApp))) {
                if (!info.isRegular()) { continue }
                if (info.name.endsWith(".cjo") || info.name.endsWith(".cjo.flag")) {
                    deleteFile(Path(cfg.outputApp).join(info.name).toString())
                }
            }
        } catch (_: Exception) { /* ignore */ }
    }
    // Remove the sanitized FFI archive cache created by buildOhosPlan()
    let sanitizedFfiDir = Path(cfg.outputSystem).join(".sanitized_ffi").toString()
    if (directoryExists(sanitizedFfiDir)) {
        deleteDirectory(sanitizedFfiDir)
    }
}

// ---------------------------------------------------------------------------
// Main orchestrator (Steps 5 + 6)
// ---------------------------------------------------------------------------

/// Execute Steps 5 and 6: symbol collection, version script, two-pass
/// compilation, copy and cleanup.
///
/// Call this after buildOhosPlan() returns a complete OhosBuildPlan.
public func runOhosCompile(tools: OhosTools, cfg: OhosBundleConfig,
                            targetTriple: String,
                            plan: OhosBuildPlan): Bool {
    // ------------------------------------------------------------------
    // Intermediate file paths (all under outputSystem/ for easy cleanup)
    // ------------------------------------------------------------------
    let dummyCj      = Path(cfg.outputApp).join("dummy_entry.cj").toString()
    let appNeedsFile = Path(cfg.outputSystem).join("app_needs.txt").toString()
    let mapFile      = Path(cfg.outputSystem).join("version_script.map").toString()
    let entrySOPath  = Path(cfg.outputApp).join(ENTRY_SO_NAME).toString()

    // Write the dummy .cj source (cjc requires at least one .cj input)
    if (!writeDummyCj(dummyCj)) { return false }

    // Pre-build reusable link-arg lists
    let commonLinkArgs = buildCommonLinkArgs(cfg, targetTriple)
    let noASoArgs      = buildNoASoLinkArgs(plan.noASoPaths)
    let noBcAppArgs    = buildNoBcAppLinkArgs(cfg, plan)
    let dirLibs        = collectExternalNeeded(tools, cfg, plan, targetTriple)
    let ohosArgs       = buildOhosLinkArgs(dirLibs)

    // ------------------------------------------------------------------
    // Pass 1: cjc → entry.so (final; uses old placeholder interop)
    // ------------------------------------------------------------------
    if (!runPass1(tools, cfg, targetTriple, plan, dummyCj,
                  commonLinkArgs, noASoArgs.toArray(), noBcAppArgs.toArray())) {
        eprintln("[ohos-bundle] ERROR: cjc Pass 1 failed")
        cleanupIntermediates(cfg, dummyCj, appNeedsFile, mapFile)
        return false
    }

    // ------------------------------------------------------------------
    // Step 5: collect undefined symbols from Pass-1 output → version script
    // ------------------------------------------------------------------
    if (!buildVersionScript(tools, cfg, plan, entrySOPath, mapFile)) {
        cleanupIntermediates(cfg, dummyCj, appNeedsFile, mapFile)
        return false
    }

    // ------------------------------------------------------------------
    // Pass 2: cjc → new ark_interop.so (LTO merge system libs)
    // ------------------------------------------------------------------
    if (!runPass2(tools, cfg, targetTriple, plan, dummyCj,
                  commonLinkArgs, noASoArgs.toArray(), ohosArgs.toArray(), mapFile)) {
        eprintln("[ohos-bundle] ERROR: cjc Pass 2 failed")
        cleanupIntermediates(cfg, dummyCj, appNeedsFile, mapFile)
        return false
    }

    // ------------------------------------------------------------------
    // Copy non-folded lib/ .so files → output_system/
    // ------------------------------------------------------------------
    if (!copyNonFoldedLibs(cfg, plan)) { return false }
    if (!checkStrictKeepCopied(cfg))   { return false }

    // ------------------------------------------------------------------
    // Copy no-.bc app libs to output_app/
    // ------------------------------------------------------------------
    copyNoBcAppLibs(cfg, plan)

    // ------------------------------------------------------------------
    // Cleanup intermediates
    // ------------------------------------------------------------------
    cleanupIntermediates(cfg, dummyCj, appNeedsFile, mapFile)

    eprintln("[ohos-bundle] done:")
    eprintln("[ohos-bundle]   output_app/    → ${ENTRY_SO_NAME} + ${plan.appNoBcSoNames.size} kept app libs")
    eprintln("[ohos-bundle]   output_system/ → ${cfg.interopSo} + ${plan.noASoPaths.size} non-folded libs")
    return true
}
