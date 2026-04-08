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
// File utilities
// ---------------------------------------------------------------------------

/// Binary-copy src to dst.  Overwrites dst if it already exists.
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
// Collect undefined dynamic symbols (for --export-symbols)
// ---------------------------------------------------------------------------

/// Collect undefined dynamic symbol names from `soPath` using
///   llvm-nm -D -u <soPath>
///
/// Strips @VERSION suffixes.  Returns an empty array on failure.
func collectUndefinedSymbols(tools: LibFoldTools, soPath: String): Array<String> {
    if (!fileExists(soPath)) { return Array<String>() }
    let args = ArrayList<String>()
    args.add("-D"); args.add("-u"); args.add(soPath)
    let (ok, stdout, _) = execWithOutput(tools.llvmNm, args)
    if (!ok || stdout.isEmpty()) { return Array<String>() }

    let syms = ArrayList<String>()
    for (rawLine in stdout.split("\n")) {
        let line = rawLine.trimAscii()
        if (line.isEmpty() || line.endsWith(":")) { continue }
        var sym = ""
        for (part in line.split(" ")) {
            let p = part.trimAscii()
            if (!p.isEmpty()) { sym = p }
        }
        if (sym.isEmpty()) { continue }
        // Strip @VERSION suffix
        if (let Some(at) <- sym.indexOf(UInt8(UInt32(r'@')), 0)) {
            sym = sym[0..at]
        }
        if (!sym.isEmpty()) { syms.add(sym) }
    }
    return syms.toArray()
}

// ---------------------------------------------------------------------------
// --export-symbols: write version script from output .so
// ---------------------------------------------------------------------------

/// Write a linker version script to `mapFile` containing the undefined symbols
/// from `soPath` plus any undefined symbols from all kept .so files.
///
/// The resulting file has the form:
///   {
///    global:
///    "sym1";
///    ...
///    local: *;
///   };
///
/// This can be passed as --version-script in the next pass to drive DCE.
func writeExportSymbols(tools: LibFoldTools, cfg: LibFoldConfig,
                        plan: LibFoldPlan, soPath: String,
                        mapFile: String): Bool {
    let symSet = HashSet<String>()

    // Symbols from the compiled output .so
    for (s in collectUndefinedSymbols(tools, soPath)) { symSet.add(s) }

    // Symbols from each kept .so (they also depend on whatever will be in
    // the next pass's output at runtime)
    for (keptPath in plan.keptSoPaths) {
        for (s in collectUndefinedSymbols(tools, keptPath)) { symSet.add(s) }
    }

    var content = "{\n global:\n"
    for (sym in symSet) {
        content += " \"${sym}\";\n"
    }
    content += " local: *;\n};\n"

    try {
        File.writeTo(Path(mapFile), content.toArray())
        logInfo(cfg, "[lib-fold] export-symbols: ${symSet.size} symbols → ${mapFile}")
        return true
    } catch (_: Exception) {
        eprintln("[lib-fold] ERROR: cannot write export-symbols: ${mapFile}")
        return false
    }
}

// ---------------------------------------------------------------------------
// --export-kept: write kept .so names to file
// ---------------------------------------------------------------------------

/// Write the basenames of all kept .so files to `keptFile` (one per line).
/// The file can be passed as --strict-kept in the next pass.
func writeExportKept(cfg: LibFoldConfig, plan: LibFoldPlan,
                     keptFile: String): Bool {
    var content = ""
    for (name in plan.keptSoNames) {
        content += name + "\n"
    }
    try {
        File.writeTo(Path(keptFile), content.toArray())
        logInfo(cfg, "[lib-fold] export-kept: ${plan.keptSoNames.size} names → ${keptFile}")
        return true
    } catch (_: Exception) {
        eprintln("[lib-fold] ERROR: cannot write export-kept: ${keptFile}")
        return false
    }
}

// ---------------------------------------------------------------------------
// Dummy .cj source (cjc requires at least one .cj input file)
// ---------------------------------------------------------------------------

func writeDummyCj(path: String): Bool {
    let content = "package dummy_entry\nmain(): Int64 { return 0 }\n"
    try {
        File.writeTo(Path(path), content.toArray())
        return true
    } catch (_: Exception) {
        eprintln("[lib-fold] ERROR: cannot write dummy entry: ${path}")
        return false
    }
}

// ---------------------------------------------------------------------------
// Common link flags
// ---------------------------------------------------------------------------

/// Build the common --link-option flags for both pass 1 and pass 2:
///   --gc-sections, --strip-debug, -rpath $ORIGIN, --no-dynamic-linker,
///   -L <sysrootLib>  (when sysroot is set)
func buildCommonLinkArgs(cfg: LibFoldConfig, targetTriple: String): Array<String> {
    let sysrootLib = sysrootLibPath(cfg, targetTriple)

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

/// Compute  <sysroot>/usr/lib/<triple>  or "" if sysroot is empty.
func sysrootLibPath(cfg: LibFoldConfig, targetTriple: String): String {
    if (cfg.sysroot.isEmpty()) { return "" }
    return Path(cfg.sysroot).join("usr").join("lib").join(targetTriple).toString()
}

// ---------------------------------------------------------------------------
// Build the cjc argument list
// ---------------------------------------------------------------------------

/// Build the complete cjc argument list for this invocation.
///
/// Inputs:
///   dummyCj       — path to the dummy .cj stub
///   commonLinkArgs — output of buildCommonLinkArgs()
///   soname        — the ELF DT_SONAME (= basename of --output)
func buildCjcArgs(cfg: LibFoldConfig, targetTriple: String,
                  plan: LibFoldPlan, dummyCj: String,
                  commonLinkArgs: Array<String>,
                  soname: String): Array<String> {
    let sysrootLib = sysrootLibPath(cfg, targetTriple)
    let args = ArrayList<String>()

    // Dummy .cj source (satisfies cjc's "need at least one .cj" requirement)
    args.add(dummyCj)

    // All fold .bc/.a inputs
    for (p in plan.foldBcPaths) { args.add(p) }

    // Target / sysroot
    args.add("--target"); args.add(targetTriple)
    if (!cfg.sysroot.isEmpty()) {
        args.add("--sysroot"); args.add(cfg.sysroot)
        args.add("-B"); args.add(sysrootLib)
    }

    // LTO output type
    args.add("--output-type=dylib")
    args.add("--lto=full")

    // Pass 1 DCE mode: DCE root = cjstart.o + @llvm.used
    if (cfg.compileAsExe) {
        args.add("--compile-as-exe")
    }

    // Common link flags
    for (a in commonLinkArgs) { args.add(a) }

    // ELF soname
    args.add("--link-option"); args.add("-soname")
    args.add("--link-option"); args.add(soname)

    // Pass 2 DCE mode: DCE root = version script exports
    if (!cfg.versionScript.isEmpty()) {
        args.add("--link-option"); args.add("--version-script")
        args.add("--link-option"); args.add(cfg.versionScript)
    }

    // --stub-so: split full path into  -L <parent>  -l:<name>
    // This ensures DT_NEEDED records the soname, not the absolute path.
    if (!cfg.stubSo.isEmpty()) {
        let stubDir  = Path(cfg.stubSo).parent.toString()
        let stubName = Path(cfg.stubSo).fileName
        args.add("-L"); args.add(stubDir)
        args.add("-l:${stubName}")
    }

    // Kept .so files: -L <dir> -l:<name> for each
    for (soPath in plan.keptSoPaths) {
        let dir  = Path(soPath).parent.toString()
        let name = Path(soPath).fileName
        args.add("-L"); args.add(dir)
        args.add("-l:${name}")
    }

    // --link-dir: extra search directories (e.g. ohos/ on-device libs)
    for (d in cfg.linkDirs) {
        args.add("-L"); args.add(d)
    }

    // Output path
    args.add("-o"); args.add(cfg.output)

    return args.toArray()
}

// ---------------------------------------------------------------------------
// Cleanup intermediate files
// ---------------------------------------------------------------------------

func cleanupIntermediates(cfg: LibFoldConfig, dummyCj: String): Unit {
    deleteFile(dummyCj)

    // Remove cjc-generated .cjo / .cjo.flag files from the output directory
    let outDir = Path(cfg.output).parent.toString()
    if (directoryExists(outDir)) {
        try {
            for (info in Directory.readFrom(Path(outDir))) {
                if (!info.isRegular()) { continue }
                if (info.name.endsWith(".cjo") || info.name.endsWith(".cjo.flag")) {
                    deleteFile(Path(outDir).join(info.name).toString())
                }
            }
        } catch (_: Exception) { /* ignore */ }
    }

    // Remove the sanitized FFI cache (lives in .sanitized_ffi/ under outDir)
    let sanitizedDir = Path(outDir).join(".sanitized_ffi").toString()
    if (directoryExists(sanitizedDir)) {
        deleteDirectory(sanitizedDir)
    }
}

// ---------------------------------------------------------------------------
// Main orchestrator (Steps 5 + 6)
// ---------------------------------------------------------------------------

/// Execute the compilation step: write dummy .cj, invoke cjc, then run
/// post-compile exports (--export-symbols, --export-kept).
///
/// Call this after buildLibFoldPlan() returns a complete LibFoldPlan.
public func runLibFoldCompile(tools: LibFoldTools, cfg: LibFoldConfig,
                               targetTriple: String,
                               plan: LibFoldPlan): Bool {
    // Ensure output directory exists
    let outDir = Path(cfg.output).parent.toString()
    if (!directoryExists(outDir)) {
        if (!createDirectory(outDir)) {
            eprintln("[lib-fold] ERROR: cannot create output dir: ${outDir}")
            return false
        }
    }

    let dummyCj = Path(outDir).join("dummy_entry.cj").toString()
    if (!writeDummyCj(dummyCj)) { return false }

    let soname         = Path(cfg.output).fileName
    let commonLinkArgs = buildCommonLinkArgs(cfg, targetTriple)

    // DIAG: print fold inputs
    eprintln("[lib-fold] DIAG fold inputs (${plan.foldBcPaths.size}):")
    for (p in plan.foldBcPaths) { eprintln("[lib-fold] DIAG   bc: ${p}") }
    eprintln("[lib-fold] DIAG kept .so (${plan.keptSoPaths.size}):")
    for (p in plan.keptSoPaths) { eprintln("[lib-fold] DIAG   kept: ${p}") }

    let args = buildCjcArgs(cfg, targetTriple, plan, dummyCj, commonLinkArgs, soname)

    logInfo(cfg, "[lib-fold] cjc → ${soname} ...")
    if (!execAndToTerminal(tools.cjc, args)) {
        eprintln("[lib-fold] ERROR: cjc failed")
        cleanupIntermediates(cfg, dummyCj)
        return false
    }

    // --export-symbols: write undefined-symbol version script from output .so
    if (!cfg.exportSymbols.isEmpty()) {
        if (!writeExportSymbols(tools, cfg, plan, cfg.output, cfg.exportSymbols)) {
            cleanupIntermediates(cfg, dummyCj)
            return false
        }
    }

    // --export-kept: write kept .so names for the next pass
    if (!cfg.exportKept.isEmpty()) {
        if (!writeExportKept(cfg, plan, cfg.exportKept)) {
            cleanupIntermediates(cfg, dummyCj)
            return false
        }
    }

    cleanupIntermediates(cfg, dummyCj)

    eprintln("[lib-fold] done: ${cfg.output}")
    eprintln("[lib-fold]   kept .so: ${plan.keptSoNames.size}")
    eprintln("[lib-fold]   folded:   ${plan.foldedSoNames.size}")
    return true
}
