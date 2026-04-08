// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*

/// Configuration for `cjpm lib-fold`.
///
/// A single invocation performs ONE pass:
///   1. Scan every --input-dir for .so files.
///   2. For each .so search all --bc-dir directories for a matching .bc/.a.
///   3. .so with no match → "kept" (standalone); .so with a match → candidate
///      for LTO-merge ("folded").
///   4. Expand the kept set: add --strict-kept entries and trace all transitive
///      DT_NEEDED dependencies.  Any fold candidate that is reachable becomes
///      un-folded.
///   5. Run cjc: merge all fold .bc/.a files into --output, linking against
///      --stub-so (if any) and the kept .so files.
///   6. Post:
///      --export-symbols → write the undefined-symbol version script from the
///        output .so (for use as --version-script in the next pass).
///      --export-kept    → write the list of kept .so basenames to a file
///        (for use as --strict-kept in the next pass).
///
/// Pass 1 (app entry.so):
///   --input-dir $APP_DIR --bc-dir … --stub-so <interop.so path>
///   --compile-as-exe --output $OUT/libohos_app_cangjie_entry.so
///   --export-symbols syms.map --export-kept kept.txt
///
/// Pass 2 (system interop.so):
///   --input-dir $LIB_DIR --bc-dir … --strict-kept kept.txt
///   --version-script syms.map --link-dir $OHOS_DIR
///   --output $OUT/libohos.ark_interop.so
///
/// NOTE: --compile-as-exe and --version-script are mutually exclusive.
///   --compile-as-exe  → DCE root set = cjstart.o references + @llvm.used
///   --version-script  → DCE root set = exported symbols in the version script
public class LibFoldConfig {
    // ---------- input ----------

    /// Directories to scan for .so files (repeatable).
    public var inputDirs: ArrayList<String> = ArrayList<String>()

    /// Directories to search for .bc / .a bitcode files (repeatable).
    /// All directories are searched recursively; the first match wins.
    public var bcDirs: ArrayList<String> = ArrayList<String>()

    // ---------- link stub ----------

    /// Full path to a .so that is used as a link-time placeholder (stub).
    /// Internally split into  -L <parent>  -l:<basename>  so that DT_NEEDED
    /// records only the soname, not the absolute path.
    /// Empty string = no stub.
    public var stubSo: String = ""

    // ---------- DCE mode ----------

    /// If true, pass --compile-as-exe to cjc (DCE by cjstart.o + @llvm.used).
    /// Mutually exclusive with a non-empty versionScript.
    public var compileAsExe: Bool = false

    // ---------- output ----------

    /// Full path for the output .so.  The basename is used as the ELF soname.
    public var output: String = "./output.so"

    // ---------- post-compile exports ----------

    /// If non-empty, write the undefined-symbol version script extracted from
    /// the output .so to this path (used as --version-script in the next pass).
    public var exportSymbols: String = ""

    /// If non-empty, write the list of kept .so basenames (one per line) to
    /// this path (used as --strict-kept in the next pass).
    public var exportKept: String = ""

    // ---------- pre-compile inputs ----------

    /// Path to a file produced by --export-kept in the previous pass.
    /// Each line is a .so basename that must remain as a standalone file
    /// (never LTO-merged into the output).
    public var strictKeptFile: String = ""

    /// Path to a linker version script (produced by --export-symbols in the
    /// previous pass).  When non-empty, passed to cjc via
    ///   --link-option --version-script --link-option <path>
    /// Mutually exclusive with compileAsExe.
    public var versionScript: String = ""

    // ---------- extra link directories ----------

    /// Extra directories added to the linker search path (repeatable).
    /// Use for on-device / SDK directories that are not under --sysroot
    /// (e.g. $OHOS_DIR containing libc.so, libm.so …).
    public var linkDirs: ArrayList<String> = ArrayList<String>()

    // ---------- target ----------

    /// Cross-compile sysroot path.  When set, cjc receives
    ///   --sysroot <sysroot>  -B <sysroot>/usr/lib/<triple>
    public var sysroot: String = ""

    /// LLVM target triple, e.g. "aarch64-linux-ohos".
    /// If empty and --stub-so is provided, auto-detected from the stub ELF.
    public var target: String = ""

    // ---------- misc ----------

    public var isVerbose: Bool = false

    public init() {}
}
