// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.fs.*
import std.env.*
import cjpm.config.*

// ---------------------------------------------------------------------------
// Public entry point called by LibFoldCommand.handleCommand()
// ---------------------------------------------------------------------------

/// Run the two-pass OHOS LTO bundle:
///
///   Pass 1  cjc --compile-as-exe --output-type=dylib --lto=full
///           → output_app/libohos_app_cangjie_entry.so  (final; uses old placeholder interop)
///
///           DCE root set = cjstart.o references + @llvm.used, independent of
///           which interop.so is linked.  Pass 1 therefore produces the final
///           entry.so in a single run.
///
///   Pass 2  cjc --output-type=dylib --lto=full  (NO --compile-as-exe)
///           Version script derived from Pass-1's undefined symbols drives DCE.
///           → output_system/libohos.ark_interop.so  (LTO-merged system libs)
///
/// Each TODO below corresponds to one subsequent implementation step.
public func doLibFold(cfg: LibFoldConfig): Bool {
    logInfo(cfg, "[lib-fold] === starting lib-fold ===")
    logInfo(cfg, "[lib-fold]   lib-dir      = ${cfg.libDir}")
    logInfo(cfg, "[lib-fold]   app-dir      = ${cfg.appDir}")
    logInfo(cfg, "[lib-fold]   app-bc-dir   = ${cfg.appBcDir}")
    logInfo(cfg, "[lib-fold]   std-bc-dir   = ${cfg.stdBcDir}")
    logInfo(cfg, "[lib-fold]   ohos-a-dir   = ${cfg.ohosADir}")
    logInfo(cfg, "[lib-fold]   ohos-dir     = ${cfg.ohosDir}")
    logInfo(cfg, "[lib-fold]   sysroot      = ${cfg.sysroot}")
    logInfo(cfg, "[lib-fold]   interop-so   = ${cfg.interopSo}")
    // DIAG: always print key paths so we can correlate with what's on disk
    eprintln("[lib-fold] DIAG: app-bc-dir = ${cfg.appBcDir}")
    eprintln("[lib-fold] DIAG: app-bc-dir exists = ${directoryExists(cfg.appBcDir)}")
    logInfo(cfg, "[lib-fold]   output-app   = ${cfg.outputApp}")
    logInfo(cfg, "[lib-fold]   output-sys   = ${cfg.outputSystem}")

    // ------------------------------------------------------------------
    // Step 2: Resolve tool paths + detect target triple
    // ------------------------------------------------------------------
    // 2-a. Find readelf / llvm-nm / llvm-ar / llc / cjc.
    //      All subsequent steps receive `tools` instead of constructing
    //      paths themselves.
    let tools = resolveTools(cfg) ?? return false

    // 2-b. If the user did not supply --target, auto-detect from the ELF
    //      Machine field of the placeholder interop .so.
    //      The interop .so is the only ELF whose architecture we know in
    //      advance and whose machine field reliably reflects the build target.
    var targetTriple = cfg.target
    if (targetTriple.isEmpty()) {
        let interopPath = Path(cfg.libDir).join(cfg.interopSo).toString()
        if (!fileExists(interopPath)) {
            eprintln("[lib-fold] ERROR: placeholder interop not found: ${interopPath}")
            eprintln("[lib-fold]        Either supply --target <triple> or ensure --lib-dir contains ${cfg.interopSo}")
            return false
        }
        targetTriple = detectTargetTriple(tools, interopPath) ?? return false
    }
    logInfo(cfg, "[lib-fold] [target] ${targetTriple}")

    // ------------------------------------------------------------------
    // Step 3: Prepare output directories + sanitize FFI archives.
    //
    // Output directories must exist before any compilation step writes
    // into them.  sanitize_ffi_archive() also writes its cleaned .a files
    // under <outputSystem>/.sanitized_ffi/ which is a sub-directory of
    // outputSystem, so creating outputSystem first is sufficient.
    // ------------------------------------------------------------------
    logInfo(cfg, "[lib-fold] cleaning output directories ...")
    if (directoryExists(cfg.outputApp)) {
        deleteDirectory(cfg.outputApp)
    }
    if (directoryExists(cfg.outputSystem)) {
        deleteDirectory(cfg.outputSystem)
    }
    logInfo(cfg, "[lib-fold] creating output directories ...")
    if (!createDirectory(cfg.outputApp)) {
        eprintln("[lib-fold] ERROR: cannot create output-app dir: ${cfg.outputApp}")
        return false
    }
    if (!createDirectory(cfg.outputSystem)) {
        eprintln("[lib-fold] ERROR: cannot create output-system dir: ${cfg.outputSystem}")
        return false
    }

    // The sanitized FFI archive cache lives under output-system so that it
    // is cleaned along with everything else when the user wipes the output.
    let sanitizedFfiDir = Path(cfg.outputSystem).join(".sanitized_ffi").toString()

    // ------------------------------------------------------------------
    // Step 3 + 4: Library analysis — scan app/ and lib/, classify every
    //             .so as fold or keep, run two-round PINNED BFS.
    // ------------------------------------------------------------------
    logInfo(cfg, "[lib-fold] running library analysis ...")
    let plan = buildLibFoldPlan(cfg, tools, targetTriple, sanitizedFfiDir) ?? return false

    // ------------------------------------------------------------------
    // Steps 5 + 6: Symbol collection, version-script generation, and
    //              two-pass cjc compilation.
    // ------------------------------------------------------------------
    return runLibFoldCompile(tools, cfg, targetTriple, plan)
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Print a message when verbose mode is on.
func logInfo(cfg: LibFoldConfig, msg: String): Unit {
    if (cfg.isVerbose) {
        println(msg)
    }
}

/// Print a message unconditionally (used for errors / important warnings).
func logAlways(_: LibFoldConfig, msg: String): Unit {
    println(msg)
}
