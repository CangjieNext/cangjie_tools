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
// Bitcode magic detection
// ---------------------------------------------------------------------------

/// Return true if the first bytes of `data` match one of the known LLVM
/// bitcode signatures:
///
///   42 43 c0 de  — LLVM bitcode wrapper (canonical form used by cjc/clang)
///   de c0 17 0b  — LLVM bitcode (thin-LTO / raw BC produced by some tools)
///   42 43 **     — short two-byte LLVM BC prefix (matches execute_so.sh's
///                  `[[ "$magic" == 4243* ]]` fallback)
///
/// `data` must contain at least the first bytes of the file (4 bytes suffice
/// for full detection; 2 bytes suffice for the prefix-only case).
public func isBitcodeMagic(data: Array<Byte>): Bool {
    if (data.size < 2) {
        return false
    }
    // Full four-byte match first (avoids false positives on short files)
    if (data.size >= 4) {
        if (data[0] == 0x42u8 && data[1] == 0x43u8 &&
            data[2] == 0xc0u8 && data[3] == 0xdeu8) {
            return true
        }
        if (data[0] == 0xdeu8 && data[1] == 0xc0u8 &&
            data[2] == 0x17u8 && data[3] == 0x0bu8) {
            return true
        }
    }
    // Two-byte prefix fallback: 0x42 0x43
    if (data[0] == 0x42u8 && data[1] == 0x43u8) {
        return true
    }
    return false
}

// ---------------------------------------------------------------------------
// Read the first bytes of a file (binary-safe)
// ---------------------------------------------------------------------------

/// Read at most `limit` bytes from the beginning of the file at `path`.
/// Returns None if the file cannot be read (missing, permission error, etc.).
///
/// Uses `File.readFrom` which returns the full content as `Array<Byte>`;
/// we then slice to `limit` bytes to avoid loading huge files entirely.
func readFirstBytes(path: String, limit: Int64): Option<Array<Byte>> {
    try {
        let all: Array<Byte> = File.readFrom(Path(path))
        if (all.size <= limit) {
            return Some(all)
        }
        return Some(all[0..limit])
    } catch (_: Exception) {
        return None
    }
}

// ---------------------------------------------------------------------------
// Archive member listing
// ---------------------------------------------------------------------------

/// List all member names contained in the ar archive at `archivePath`.
///
/// Runs `llvm-ar t <archivePath>` and splits the output on newlines.
/// Returns an empty array if the archive cannot be read or has no members.
public func listArchiveMembers(llvmAr: String, archivePath: String): Array<String> {
    let args = ArrayList<String>()
    args.add("t")
    args.add(archivePath)
    let (ok, stdout, _) = execWithOutput(llvmAr, args)
    if (!ok || stdout.isEmpty()) {
        return Array<String>()
    }
    let members = ArrayList<String>()
    for (rawLine in stdout.split("\n")) {
        let name = rawLine.trimAscii()
        if (!name.isEmpty()) {
            members.add(name)
        }
    }
    return members.toArray()
}

// ---------------------------------------------------------------------------
// Compile a single bitcode file to ELF
// ---------------------------------------------------------------------------

/// Compile the LLVM bitcode file at `bcPath` to an ELF object file at
/// `elfPath` using:
///
///   llc -filetype=obj -relocation-model=pic --mtriple=<triple> -o <elfPath> <bcPath>
///
/// Position-independent code (-relocation-model=pic) is required because
/// the resulting object will be linked into a shared library (.so).
/// Returns true on success.
func compileBcToElf(tools: LibFoldTools, bcPath: String, elfPath: String,
                    triple: String): Bool {
    let args = ArrayList<String>()
    args.add("-filetype=obj")
    args.add("-relocation-model=pic")
    args.add("--mtriple=${triple}")
    args.add("-o")
    args.add(elfPath)
    args.add(bcPath)
    let (ok, _, _) = execWithOutput(tools.llc, args)
    return ok
}

// ---------------------------------------------------------------------------
// Repack an archive from an explicit list of object files
// ---------------------------------------------------------------------------

/// Create (or overwrite) the ar archive at `outPath` containing exactly the
/// objects listed in `objPaths`.
///
/// Uses `llvm-ar crD` flags:
///   c — create a new archive (suppress "no archive" warning)
///   r — insert/replace members
///   D — deterministic mode: zero out timestamps, UIDs, GIDs, and file modes
///       in the archive header; ensures reproducible builds
///
/// When `objPaths` is empty a valid (empty) archive is still created.
/// Returns true on success.
func repackArchive(llvmAr: String, outPath: String, objPaths: Array<String>): Bool {
    let args = ArrayList<String>()
    args.add("crD")
    args.add(outPath)
    for (obj in objPaths) {
        args.add(obj)
    }
    let (ok, _, _) = execWithOutput(llvmAr, args)
    return ok
}

// ---------------------------------------------------------------------------
// Main sanitization entry point
// ---------------------------------------------------------------------------

/// Inspect `archivePath` for LLVM bitcode members and, if any are found,
/// compile them to ELF and return a new sanitized archive under `sanitizedDir`.
/// If no bitcode members are found the original path is returned unchanged.
///
/// Algorithm (mirrors sanitize_ffi_archive() in execute_so.sh):
///
///   1. List all member names via `llvm-ar t`.
///      If the archive is empty/unreadable, return the original path.
///
///   2. Extract all members to a temp directory:
///        llvm-ar x <archivePath>   (run with workingDirectory=tempDir)
///      This avoids passing binary content through a String (as `ar p` would).
///
///   3. Quick-scan: read the first 4 bytes of each extracted member.
///      If none are bitcode, clean up and return the original path.
///
///   4. For every bitcode member, compile it:
///        llc -filetype=obj -relocation-model=pic --mtriple=<triple> \
///            -o <member_elf.o> <member.o>
///      Members where llc succeeds contribute <member_elf.o> to the object
///      list.  Members where llc fails are dropped with a warning (mirroring
///      execute_so.sh's "dropping member" behaviour).
///      Non-bitcode members contribute their original extracted path.
///
///   5. Repack surviving objects with `llvm-ar crD` into
///        <sanitizedDir>/<basename>
///
///   6. Remove the temp directory.
///
/// Returns None only on a hard failure (directory creation or repacking).
public func sanitizeFfiArchive(tools: LibFoldTools, archivePath: String,
                                targetTriple: String,
                                sanitizedDir: String): Option<String> {
    let baseName = Path(archivePath).fileName

    // ------------------------------------------------------------------
    // Step 1: List members.  Empty archive → return as-is.
    // ------------------------------------------------------------------
    let members = listArchiveMembers(tools.llvmAr, archivePath)
    if (members.isEmpty()) {
        return Some(archivePath)
    }

    // ------------------------------------------------------------------
    // Step 2: Extract all members to a temporary directory.
    // ------------------------------------------------------------------
    let tempDir = Path(sanitizedDir).join(".tmp_${baseName}").toString()
    if (!createDirectory(tempDir)) {
        eprintln("[lib-fold] [sanitize-ffi] ERROR: cannot create temp dir ${tempDir}")
        return None
    }

    // Resolve archivePath to absolute before changing workingDirectory,
    // otherwise the relative path becomes invalid once llvm-ar cds into tempDir.
    let absArchivePath = canonicalize(Path(archivePath)).toString()

    let extractArgs = ArrayList<String>()
    extractArgs.add("x")
    extractArgs.add(absArchivePath)
    let (extracted, _, extractErr) = execWithOutput(
        tools.llvmAr, extractArgs, workingDirectory: Some(Path(tempDir)))
    if (!extracted) {
        eprintln("[lib-fold] [sanitize-ffi] ERROR: llvm-ar x failed on ${archivePath}: ${extractErr}")
        deleteDirectory(tempDir)
        return None
    }

    // ------------------------------------------------------------------
    // Step 3: Quick-scan — check whether any member is bitcode.
    // ------------------------------------------------------------------
    var hasBc = false
    for (member in members) {
        let mpath = Path(tempDir).join(member).toString()
        if (let Some(hdr) <- readFirstBytes(mpath, 4)) {
            if (isBitcodeMagic(hdr)) {
                hasBc = true
                break
            }
        }
    }

    if (!hasBc) {
        // All members are ELF — nothing to do.
        deleteDirectory(tempDir)
        return Some(archivePath)
    }

    eprintln("[lib-fold] [sanitize-ffi] ${baseName}: bitcode members found, compiling to ELF...")

    // ------------------------------------------------------------------
    // Step 4: Compile bitcode members; build the final object list.
    // ------------------------------------------------------------------
    // Design note: bitcode members are compiled to a sibling file
    // `<stem>_elf.o` so no rename is needed.  The obj_list accumulates
    // the path that should actually be packed (either the original .o for
    // ELF members, or the new _elf.o for compiled bitcode members).
    // ------------------------------------------------------------------
    let objList = ArrayList<String>()
    for (member in members) {
        let mpath = Path(tempDir).join(member).toString()
        if (!fileExists(mpath)) {
            // Member was not extracted (duplicate name collision, etc.) — skip.
            continue
        }

        if (let Some(hdr) <- readFirstBytes(mpath, 4)) {
            if (isBitcodeMagic(hdr)) {
                // Derive an ELF output path alongside the bitcode file.
                let stem = if (member.endsWith(".o")) {
                    member[0..(member.size - 2)]
                } else {
                    member
                }
                let elfPath = Path(tempDir).join("${stem}_elf.o").toString()

                if (compileBcToElf(tools, mpath, elfPath, targetTriple)) {
                    objList.add(elfPath)
                    eprintln("[lib-fold] [sanitize-ffi]   → compiled: ${member}")
                } else {
                    // llc failed: drop this member (matches execute_so.sh behaviour)
                    eprintln("[lib-fold] [sanitize-ffi]   [warn] llc failed for ${member} in ${baseName}, dropping")
                }
                continue
            }
        }
        // Member is ELF (or unreadable but present) — keep as-is.
        objList.add(mpath)
    }

    // ------------------------------------------------------------------
    // Step 5: Repack surviving objects into the sanitized output archive.
    // ------------------------------------------------------------------
    if (!createDirectory(sanitizedDir)) {
        eprintln("[lib-fold] [sanitize-ffi] ERROR: cannot create sanitized dir ${sanitizedDir}")
        deleteDirectory(tempDir)
        return None
    }

    let cleanPath = Path(sanitizedDir).join(baseName).toString()
    if (!repackArchive(tools.llvmAr, cleanPath, objList.toArray())) {
        eprintln("[lib-fold] [sanitize-ffi] ERROR: llvm-ar crD failed for ${baseName}")
        deleteDirectory(tempDir)
        return None
    }

    // ------------------------------------------------------------------
    // Step 6: Clean up the temporary directory.
    // ------------------------------------------------------------------
    deleteDirectory(tempDir)
    return Some(cleanPath)
}
