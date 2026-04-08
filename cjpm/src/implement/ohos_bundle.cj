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
// Public entry point called by OhosBundleCommand.handleCommand()
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
public func doOhosBundle(cfg: OhosBundleConfig): Bool {
    logInfo(cfg, "[ohos-bundle] === starting ohos-bundle ===")
    logInfo(cfg, "[ohos-bundle]   lib-dir      = ${cfg.libDir}")
    logInfo(cfg, "[ohos-bundle]   app-dir      = ${cfg.appDir}")
    logInfo(cfg, "[ohos-bundle]   app-bc-dir   = ${cfg.appBcDir}")
    logInfo(cfg, "[ohos-bundle]   std-bc-dir   = ${cfg.stdBcDir}")
    logInfo(cfg, "[ohos-bundle]   ohos-a-dir   = ${cfg.ohosADir}")
    logInfo(cfg, "[ohos-bundle]   ohos-dir     = ${cfg.ohosDir}")
    logInfo(cfg, "[ohos-bundle]   sysroot      = ${cfg.sysroot}")
    logInfo(cfg, "[ohos-bundle]   interop-so   = ${cfg.interopSo}")
    logInfo(cfg, "[ohos-bundle]   output-app   = ${cfg.outputApp}")
    logInfo(cfg, "[ohos-bundle]   output-sys   = ${cfg.outputSystem}")

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
            eprintln("[ohos-bundle] ERROR: placeholder interop not found: ${interopPath}")
            eprintln("[ohos-bundle]        Either supply --target <triple> or ensure --lib-dir contains ${cfg.interopSo}")
            return false
        }
        targetTriple = detectTargetTriple(tools, interopPath) ?? return false
    }
    logInfo(cfg, "[ohos-bundle] [target] ${targetTriple}")

    // ------------------------------------------------------------------
    // Step 3: Prepare output directories + sanitize FFI archives.
    //
    // Output directories must exist before any compilation step writes
    // into them.  sanitize_ffi_archive() also writes its cleaned .a files
    // under <outputSystem>/.sanitized_ffi/ which is a sub-directory of
    // outputSystem, so creating outputSystem first is sufficient.
    // ------------------------------------------------------------------
    logInfo(cfg, "[ohos-bundle] cleaning output directories ...")
    if (directoryExists(cfg.outputApp)) {
        deleteDirectory(cfg.outputApp)
    }
    if (directoryExists(cfg.outputSystem)) {
        deleteDirectory(cfg.outputSystem)
    }
    logInfo(cfg, "[ohos-bundle] creating output directories ...")
    if (!createDirectory(cfg.outputApp)) {
        eprintln("[ohos-bundle] ERROR: cannot create output-app dir: ${cfg.outputApp}")
        return false
    }
    if (!createDirectory(cfg.outputSystem)) {
        eprintln("[ohos-bundle] ERROR: cannot create output-system dir: ${cfg.outputSystem}")
        return false
    }

    // The sanitized FFI archive cache lives under output-system so that it
    // is cleaned along with everything else when the user wipes the output.
    let sanitizedFfiDir = Path(cfg.outputSystem).join(".sanitized_ffi").toString()

    // ------------------------------------------------------------------
    // Step 3 + 4: Library analysis — scan app/ and lib/, classify every
    //             .so as fold or keep, run two-round PINNED BFS.
    // ------------------------------------------------------------------
    logInfo(cfg, "[ohos-bundle] running library analysis ...")
    let plan = buildOhosPlan(cfg, tools, targetTriple, sanitizedFfiDir) ?? return false

    // ------------------------------------------------------------------
    // Steps 5 + 6: Symbol collection, version-script generation, and
    //              two-pass cjc compilation.
    // ------------------------------------------------------------------
    return runOhosCompile(tools, cfg, targetTriple, plan)
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Print a message when verbose mode is on.
func logInfo(cfg: OhosBundleConfig, msg: String): Unit {
    if (cfg.isVerbose) {
        println(msg)
    }
}

/// Print a message unconditionally (used for errors / important warnings).
func logAlways(_: OhosBundleConfig, msg: String): Unit {
    println(msg)
}
