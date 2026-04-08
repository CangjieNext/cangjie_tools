// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import std.collection.*
import cjpm.config.*
import cjpm.implement.*

let LIB_FOLD_USAGE: String = """
LTO-merge shared libraries from bitcode artifacts into a single output .so.

One invocation = one pass.  Call twice (with different options) for a full
two-pass app+system build:

  Pass 1 — produce entry.so from app bitcode:
    cjpm lib-fold \\
      --input-dir $APP_DIR \\
      --bc-dir $APP_BC_DIR --bc-dir $STD_BC_DIR --bc-dir $STDX_BC_DIR --bc-dir $OHOS_BC_DIR \\
      --stub-so  $LIB_DIR/libohos.ark_interop.so \\
      --compile-as-exe \\
      --output   $OUT_APP/libohos_app_cangjie_entry.so \\
      --export-symbols syms.map \\
      --export-kept    kept.txt \\
      --target   aarch64-linux-ohos \\
      --sysroot  $SYSROOT

  Pass 2 — produce new interop.so from system bitcode:
    cjpm lib-fold \\
      --input-dir $LIB_DIR \\
      --bc-dir $STD_BC_DIR --bc-dir $STDX_BC_DIR --bc-dir $OHOS_BC_DIR \\
      --strict-kept    kept.txt \\
      --version-script syms.map \\
      --link-dir $OHOS_DIR \\
      --output   $OUT_SYS/libohos.ark_interop.so \\
      --target   aarch64-linux-ohos \\
      --sysroot  $SYSROOT

Algorithm:
  1. Scan every --input-dir for .so files.
  2. For each .so look in all --bc-dir trees for a matching .bc / .a.
  3. .so with no match → kept (standalone); .so with match → fold candidate.
  4. --strict-kept entries + transitive DT_NEEDED BFS → expand kept set.
  5. cjc: merge fold .bc/.a into --output.
  6. --export-symbols / --export-kept → write outputs for the next pass.

NOTE: --compile-as-exe and --version-script are mutually exclusive.
  --compile-as-exe  DCE root = cjstart.o + @llvm.used   (Pass 1)
  --version-script  DCE root = version script globals    (Pass 2)

Usage:
  cjpm lib-fold [options]

Available options:
  -h, --help                    help for lib-fold
  -V, --verbose                 enable verbose output
  --input-dir   <path>          directory to scan for .so files (repeatable)
  --bc-dir      <path>          directory to search for .bc/.a (repeatable)
  --stub-so     <path>          full path of link-time placeholder .so
  --compile-as-exe              Pass 1 DCE mode (mutually exclusive with --version-script)
  --output      <path>          output .so path (soname = basename)
  --export-symbols <path>       write undefined-symbol version script after compile
  --export-kept    <path>       write kept .so names (one per line) after compile
  --strict-kept    <path>       file of .so names that must stay standalone
  --version-script <path>       linker version script (Pass 2 DCE mode)
  --link-dir    <path>          extra linker search directory (repeatable)
  --sysroot     <path>          cross-compile sysroot
  --target      <triple>        LLVM target triple (auto-detected from --stub-so if omitted)"""

class LibFoldCommand <: Handle {
    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args[1..]
    }

    public override func printHelp(): Unit {
        println(LIB_FOLD_USAGE)
    }

    public override func handleCommand(): Bool {
        if (hasHelpCommand(this)) {
            return true
        }

        let cfg = LibFoldConfig()

        // Manual arg parsing to support repeatable options (--input-dir, --bc-dir, --link-dir).
        // ArgOpt uses HashMap<String,String> and silently drops duplicate keys.
        let args = this.arguments
        // Options that take a value argument
        let valueOpts = HashSet<String>()
        for (o in ["--input-dir", "--bc-dir", "--link-dir", "--stub-so", "--output",
                   "--export-symbols", "--export-kept", "--strict-kept",
                   "--version-script", "--sysroot", "--target"]) {
            valueOpts.add(o)
        }

        var i = 0
        while (i < args.size) {
            let arg = args[i]
            if (arg == "-h" || arg == "--help") {
                println(LIB_FOLD_USAGE)
                return true
            } else if (arg == "-V" || arg == "--verbose") {
                cfg.isVerbose = true
            } else if (arg == "--compile-as-exe") {
                cfg.compileAsExe = true
            } else if (valueOpts.contains(arg)) {
                i += 1
                if (i >= args.size) {
                    eprintln("Error: option '${arg}' requires a value")
                    return false
                }
                let val = args[i]
                if      (arg == "--input-dir")      { cfg.inputDirs.add(val) }
                else if (arg == "--bc-dir")         { cfg.bcDirs.add(val) }
                else if (arg == "--link-dir")       { cfg.linkDirs.add(val) }
                else if (arg == "--stub-so")        { cfg.stubSo         = val }
                else if (arg == "--output")         { cfg.output         = val }
                else if (arg == "--export-symbols") { cfg.exportSymbols  = val }
                else if (arg == "--export-kept")    { cfg.exportKept     = val }
                else if (arg == "--strict-kept")    { cfg.strictKeptFile = val }
                else if (arg == "--version-script") { cfg.versionScript  = val }
                else if (arg == "--sysroot")        { cfg.sysroot        = val }
                else if (arg == "--target")         { cfg.target         = val }
            } else {
                eprintln("Error: unknown argument '${arg}' for 'cjpm lib-fold'")
                eprintln("       run 'cjpm lib-fold --help' for usage")
                return false
            }
            i += 1
        }

        // Validate mutual exclusion
        if (cfg.compileAsExe && !cfg.versionScript.isEmpty()) {
            eprintln("Error: --compile-as-exe and --version-script are mutually exclusive")
            eprintln("       --compile-as-exe  DCE root = cjstart.o + @llvm.used   (Pass 1)")
            eprintln("       --version-script  DCE root = version script globals   (Pass 2)")
            return false
        }

        // Validate required options
        if (cfg.inputDirs.isEmpty()) {
            eprintln("Error: at least one --input-dir is required")
            return false
        }
        if (cfg.output.isEmpty()) {
            eprintln("Error: --output is required")
            return false
        }

        if (!cfg.target.isEmpty() && cfg.sysroot.isEmpty()) {
            eprintln("Warning: --target was specified but --sysroot was not; the linker may fail to locate C runtime objects")
        }

        return doLibFold(cfg)
    }
}
