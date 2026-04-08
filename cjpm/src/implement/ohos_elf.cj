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
// Tool paths
// ---------------------------------------------------------------------------

/// Resolved absolute paths for every external binary needed by ohos-bundle.
/// Created once at the start of doOhosBundle() by resolveTools().
public struct OhosTools {
    public let readelf: String  // system readelf (binutils)
    public let llvmNm:  String  // llvm-nm  — extract dynamic symbols
    public let llvmAr:  String  // llvm-ar  — repack FFI archives
    public let llc:     String  // llc      — compile bitcode → ELF
    public let cjc:     String  // Cangjie compiler (Pass 1 + Pass 2)

    public init(readelf: String, llvmNm: String, llvmAr: String,
                llc: String, cjc: String) {
        this.readelf = readelf
        this.llvmNm  = llvmNm
        this.llvmAr  = llvmAr
        this.llc     = llc
        this.cjc     = cjc
    }
}

/// Locate all required tools and return an OhosTools, or None on failure.
///
/// Search strategy (mirrors execute_so.sh):
///   • llvm-nm / llvm-ar / llc
///       → $CANGJIE_HOME/third_party/llvm/bin/<name>  (preferred)
///       → /usr/bin/<name>                             (fallback)
///   • cjc
///       → $CANGJIE_HOME/bin/cjc                       (preferred)
///       → bare "cjc" (rely on PATH)                   (fallback)
///   • readelf
///       → bare "readelf" (system binutils, always on PATH)
public func resolveTools(cfg: OhosBundleConfig): Option<OhosTools> {
    let cjHome  = getVariable("CANGJIE_HOME") ?? ""
    let llvmBin = if (!cjHome.isEmpty()) {
        Path(cjHome).join("third_party").join("llvm").join("bin").toString()
    } else { "" }
    let cjBin = if (!cjHome.isEmpty()) {
        Path(cjHome).join("bin").toString()
    } else { "" }

    // readelf: prefer bundled llvm-readobj (llvm-readelf is a symlink to it on
    // Windows, and symlinks often fail without admin privileges).  We pass
    // --elf-output-style=GNU at each call site so the output matches readelf.
    let readelf = pickTool(makeExeName("llvm-readobj"), llvmBin, "readelf")

    // LLVM tools: prefer the bundled LLVM, fall back to system installs
    // Use makeExeName so that on Windows the .exe suffix is appended correctly.
    let llvmNm = pickTool(makeExeName("llvm-nm"), llvmBin, "/usr/bin/llvm-nm")
    let llvmAr = pickTool(makeExeName("llvm-ar"), llvmBin, "/usr/bin/llvm-ar")
    let llc    = pickTool(makeExeName("llc"),     llvmBin, "/usr/bin/llc")

    // cjc must exist; without it Passes 1 and 2 cannot run
    let cjcExe = makeExeName("cjc")
    let cjc = pickTool(cjcExe, cjBin, cjcExe)
    if (!cjBin.isEmpty()) {
        let cjcFull = Path(cjBin).join(cjcExe).toString()
        if (!fileExists(cjcFull)) {
            eprintln("[ohos-bundle] ERROR: cjc not found at ${cjcFull}; set CANGJIE_HOME correctly")
            return None
        }
    }

    let tools = OhosTools(readelf, llvmNm, llvmAr, llc, cjc)
    if (cfg.isVerbose) {
        println("[ohos-bundle]   readobj  = ${tools.readelf}")
        println("[ohos-bundle]   llvm-nm  = ${tools.llvmNm}")
        println("[ohos-bundle]   llvm-ar  = ${tools.llvmAr}")
        println("[ohos-bundle]   llc      = ${tools.llc}")
        println("[ohos-bundle]   cjc      = ${tools.cjc}")
    }
    return Some(tools)
}

/// Return the path to `name` by checking `preferredDir` first, then `fallback`.
/// If neither exists as a file, return the bare `name` (rely on PATH).
func pickTool(name: String, preferredDir: String, fallback: String): String {
    if (!preferredDir.isEmpty()) {
        let preferred = Path(preferredDir).join(name).toString()
        if (fileExists(preferred)) {
            return preferred
        }
    }
    if (!fallback.isEmpty() && fileExists(fallback)) {
        return fallback
    }
    return name
}

// ---------------------------------------------------------------------------
// ELF header parsing — detectTargetTriple
// ---------------------------------------------------------------------------

/// Read the ELF Machine field from `soPath` (via `readelf -h`) and map it to
/// an LLVM target triple.
///
/// Mapping table (matches execute_so.sh's detect_target_triple_from_so):
///   AArch64                          → aarch64-linux-ohos
///   ARM                              → armv7a-linux-ohos
///   Advanced Micro Devices X86-64    → x86_64-linux-gnu
///   X86-64                           → x86_64-linux-gnu
///   Intel 80386                      → i686-linux-gnu
///   RISC-V                           → riscv64-linux-gnu
///
/// Returns None if the .so cannot be read or the machine is unrecognised.
public func detectTargetTriple(tools: OhosTools, soPath: String): Option<String> {
    // Step A: run readelf -h <soPath> and capture stdout
    //   When the resolved tool is llvm-readobj (not readelf), we must request
    //   GNU-style output so the parsing below works unchanged.
    let args = ArrayList<String>()
    if (tools.readelf.contains("llvm-readobj")) {
        args.add("--elf-output-style=GNU")
    }
    args.add("-h")
    args.add(soPath)
    let (ok, stdout, stderr) = execWithOutput(tools.readelf, args)
    if (!ok) {
        eprintln("[ohos-bundle] ERROR: readelf -h failed on ${soPath}: ${stderr}")
        return None
    }

    // Step B: scan lines for "Machine:" and extract the value
    //
    // Example line from readelf -h output:
    //   "  Machine:                           AArch64"
    // After split("\n") and trimAscii():
    //   "Machine:                           AArch64"
    // After removePrefix("Machine:") and trimAscii():
    //   "AArch64"
    var machineVal = ""
    for (rawLine in stdout.split("\n")) {
        let line = rawLine.trimAscii()
        if (line.startsWith("Machine:")) {
            machineVal = line.removePrefix("Machine:").trimAscii()
            break
        }
    }

    if (machineVal.isEmpty()) {
        eprintln("[ohos-bundle] ERROR: could not find Machine field in readelf -h output for ${soPath}")
        return None
    }

    // Step C: map machine name → LLVM target triple
    return machineToTriple(machineVal, soPath)
}

/// Map an ELF Machine field value to an LLVM triple string.
/// Returns None and prints an error for unrecognised machines.
func machineToTriple(machine: String, soPath: String): Option<String> {
    // Use a lookup table instead of match-with-| to stay compatible with
    // Cangjie's string pattern matching rules.
    let table = HashMap<String, String>()
    table["AArch64"]                         = "aarch64-linux-ohos"
    table["ARM"]                             = "armv7a-linux-ohos"
    table["Advanced Micro Devices X86-64"]   = "x86_64-linux-gnu"
    table["X86-64"]                          = "x86_64-linux-gnu"
    table["Intel 80386"]                     = "i686-linux-gnu"
    table["RISC-V"]                          = "riscv64-linux-gnu"

    if (table.contains(machine)) {
        return Some(table[machine])
    }
    eprintln("[ohos-bundle] ERROR: unrecognised ELF Machine '${machine}' in ${soPath}")
    return None
}

// ---------------------------------------------------------------------------
// Dynamic section parsing — getNeeded
// ---------------------------------------------------------------------------

/// Return all DT_NEEDED library names from the dynamic section of `soPath`.
///
/// Internally runs `readelf -d <soPath>` and extracts the library names that
/// appear inside square brackets on lines tagged "(NEEDED)".
///
/// Example readelf -d output line:
///   " 0x0000000000000001 (NEEDED)             Shared library: [libfoo.so]"
/// Extracted value: "libfoo.so"
///
/// Returns an empty array if the file cannot be read or has no NEEDED entries.
public func getNeeded(tools: OhosTools, soPath: String): Array<String> {
    if (!fileExists(soPath)) {
        return Array<String>()
    }

    // Step A: run readelf -d <soPath>
    let args = ArrayList<String>()
    if (tools.readelf.contains("llvm-readobj")) {
        args.add("--elf-output-style=GNU")
    }
    args.add("-d")
    args.add(soPath)
    let (ok, stdout, _) = execWithOutput(tools.readelf, args)
    if (!ok) {
        return Array<String>()
    }

    // Step B: scan for "(NEEDED)" lines and extract the library name from [...]
    //
    // The format is always:
    //   <hex-tag> (NEEDED)   Shared library: [<libname>]
    //
    // We locate the '[' and ']' delimiters by byte position.
    let needed = ArrayList<String>()
    let byteOpen  = UInt8(UInt32(r'['))
    let byteClose = UInt8(UInt32(r']'))

    for (rawLine in stdout.split("\n")) {
        let line = rawLine.trimAscii()
        if (!line.contains("(NEEDED)")) {
            continue
        }
        // Find '[' then ']'
        if (let Some(iOpen) <- line.indexOf(byteOpen, 0)) {
            if (let Some(iClose) <- line.indexOf(byteClose, iOpen + 1)) {
                // Extract the library name between the brackets
                let libName = line[(iOpen + 1)..iClose].trimAscii()
                if (!libName.isEmpty()) {
                    needed.add(libName)
                }
            }
        }
    }

    return needed.toArray()
}
