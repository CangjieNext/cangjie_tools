// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

/// Configuration for `cjpm ohos-bundle`.
///
/// M1 design: all paths are specified explicitly via CLI flags.
/// M2 will add auto-discovery from .cjpm-history.
public class OhosBundleConfig {
    // ---------- input directories ----------

    /// System-side .so files (placeholder interop.so lives here too).
    /// Corresponds to lib/ in execute_so.sh.
    public var libDir: String = "./lib"

    /// App-side .so files (may or may not have matching .bc).
    /// Corresponds to app/ in execute_so.sh.
    public var appDir: String = "./app"

    /// App-side LLVM bitcode files (.bc) produced by `cjpm build --lto=full`.
    /// The entry bitcode (ohos_app_cangjie_entry.bc) must be present here.
    /// In the hvigorw integration this maps to:
    ///   intermediates/cj/build/<target>/<triple>/release/
    public var appBcDir: String = "./modules/app"

    /// SDK standard-library bitcode directory.
    /// Ships as: build-tools/modules/linux_ohos_aarch64_cjnative/std/
    /// Contains files like libstd.collection.bc, libstd.io.bc …
    public var stdBcDir: String = "./modules/std"

    /// SDK ohos archive directory.
    /// Ships as: api/lib/linux_ohos_aarch64_cjnative/ohos/
    /// Contains raw LLVM-bitcode archives with a .a extension:
    ///   libohos.ark_interop.a, libohos.abc.a, …
    public var ohosADir: String = "./modules/ohos"

    /// OHOS system libraries available on-device (used as -l references only).
    /// Corresponds to ohos/ in execute_so.sh.
    public var ohosDir: String = "./ohos"

    /// Cross-compile sysroot path.
    public var sysroot: String = ""

    // ---------- target ----------

    /// LLVM target triple, e.g. "aarch64-linux-ohos".
    /// If empty, auto-detected from the ELF Machine field of the placeholder interop .so.
    public var target: String = ""

    /// Name of the placeholder / output interop shared library.
    /// Default matches the name used in execute_so.sh.
    public var interopSo: String = "libohos.ark_interop.so"

    // ---------- output directories ----------

    /// Output directory for app artifacts (entry.so + no-bc app libs).
    public var outputApp: String = "./output_app"

    /// Output directory for system artifacts (new interop.so + non-folded libs).
    public var outputSystem: String = "./output_system"

    // ---------- options ----------

    public var isVerbose: Bool = false

    /// Libraries that must remain as independent .so files and must never be
    /// LTO-merged into ark_interop.so.
    public var strictKeep: Array<String> = [
        "libcangjie-runtime.so",
        "libboundscheck.so",
        "libpcre2-8.so.0"
    ]

    public init() {}
}
