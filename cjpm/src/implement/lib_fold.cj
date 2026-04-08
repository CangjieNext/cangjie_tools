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

/// Run a single lib-fold pass:
///
///   1. Resolve tool paths.
///   2. Detect target triple (auto from --stub-so ELF if --target not given).
///   3. Prepare the sanitized-FFI cache directory under the output dir.
///   4. Build the library analysis plan (classify .so files as fold or keep).
///   5. Run cjc with all fold .bc/.a inputs.
///   6. Write --export-symbols / --export-kept outputs.
///
/// Two invocations implement the two-pass build:
///
///   Pass 1  --input-dir $APP_DIR --bc-dir … --stub-so <interop.so>
///           --compile-as-exe --output $OUT_APP/libohos_app_cangjie_entry.so
///           --export-symbols syms.map --export-kept kept.txt
///
///   Pass 2  --input-dir $LIB_DIR --bc-dir …
///           --strict-kept kept.txt --version-script syms.map
///           --link-dir $OHOS_DIR
///           --output $OUT_SYS/libohos.ark_interop.so
public func doLibFold(cfg: LibFoldConfig): Bool {
    logInfo(cfg, "[lib-fold] === starting lib-fold ===")
    if (cfg.isVerbose) {
        for (d in cfg.inputDirs) { logInfo(cfg, "[lib-fold]   input-dir  = ${d}") }
        for (d in cfg.bcDirs)    { logInfo(cfg, "[lib-fold]   bc-dir     = ${d}") }
        logInfo(cfg, "[lib-fold]   stub-so    = ${cfg.stubSo}")
        logInfo(cfg, "[lib-fold]   output     = ${cfg.output}")
        logInfo(cfg, "[lib-fold]   compile-as-exe = ${cfg.compileAsExe}")
        logInfo(cfg, "[lib-fold]   version-script = ${cfg.versionScript}")
        logInfo(cfg, "[lib-fold]   sysroot    = ${cfg.sysroot}")
        logInfo(cfg, "[lib-fold]   target     = ${cfg.target}")
    }

    // ------------------------------------------------------------------
    // Step 1: Resolve tool paths
    // ------------------------------------------------------------------
    let tools = resolveTools(cfg) ?? return false

    // ------------------------------------------------------------------
    // Step 2: Detect target triple
    // ------------------------------------------------------------------
    var targetTriple = cfg.target
    if (targetTriple.isEmpty()) {
        if (!cfg.stubSo.isEmpty() && fileExists(cfg.stubSo)) {
            targetTriple = detectTargetTriple(tools, cfg.stubSo) ?? return false
        } else {
            eprintln("[lib-fold] ERROR: --target is required when --stub-so is not provided")
            return false
        }
    }
    logInfo(cfg, "[lib-fold] target: ${targetTriple}")

    // ------------------------------------------------------------------
    // Step 3: Prepare the sanitized-FFI cache directory
    //         (lives under the output directory for automatic cleanup)
    // ------------------------------------------------------------------
    let outDir = Path(cfg.output).parent.toString()
    if (!directoryExists(outDir)) {
        if (!createDirectory(outDir)) {
            eprintln("[lib-fold] ERROR: cannot create output directory: ${outDir}")
            return false
        }
    }
    let sanitizedFfiDir = Path(outDir).join(".sanitized_ffi").toString()

    // ------------------------------------------------------------------
    // Step 4: Library analysis — scan and classify .so files
    // ------------------------------------------------------------------
    logInfo(cfg, "[lib-fold] running library analysis ...")
    let plan = buildLibFoldPlan(cfg, tools) ?? return false

    // ------------------------------------------------------------------
    // Step 5+6: Compilation + post-compile exports
    // ------------------------------------------------------------------
    return runLibFoldCompile(tools, cfg, targetTriple, plan)
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/// Print a message only when verbose mode is on.
func logInfo(cfg: LibFoldConfig, msg: String): Unit {
    if (cfg.isVerbose) {
        println(msg)
    }
}

/// Print a message unconditionally.
func logAlways(_: LibFoldConfig, msg: String): Unit {
    println(msg)
}
