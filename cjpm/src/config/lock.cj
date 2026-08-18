// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*
import std.fs.*
import stdx.serialization.serialization.*
import cjpm.toml.*
import cjpm.toml.Decoder as DecoderToml
import cjpm.toml.Encoder as EncoderToml

let MODULE_LOCK_VERSION: UInt64 = 0
// Bound input before allocating or retaining structures proportional to the lock file.
const MAX_LOCK_FILE_SIZE: Int64 = 64 * 1024 * 1024
const MAX_LOCK_ENTRY_COUNT: Int64 = 100000
// Non-canonical TOML is supported only for small legacy or hand-written files.
const GENERIC_LOCK_FALLBACK_SIZE: Int64 = 1024 * 1024
// Canonical lock entries are single-line inline tables.
const MAX_LOCK_LINE_SIZE: Int64 = 1024 * 1024
// Keep temporary read buffers small while filling the exact-size destination array.
const LOCK_READ_BUFFER_SIZE: Int64 = 8 * 1024

// lock file model
public class ModuleLock <: Serializable<ModuleLock> {
    public var requires: HashMap<String, DepInfo> = HashMap<String, DepInfo>()
    public var scripts: HashMap<String, DepInfo> = HashMap<String, DepInfo>()
    //Used so that we can change format of this file without breaking change
    var version: UInt64 = MODULE_LOCK_VERSION

    public init() {}

    public init(requires: HashMap<String, DepInfo>) {
        for ((name, depInfo) in requires) {
            if (depInfo.isGitDep() && depInfo.pathDepInGit) {
                continue
            }
            this.requires.add(name, depInfo)
        }
    }

    public init(requires: HashMap<String, DepInfo>, scripts: HashMap<String, DepInfo>) {
        for ((name, depInfo) in requires) {
            if (depInfo.isGitDep() && depInfo.pathDepInGit) {
                continue
            }
            this.requires.add(name, depInfo)
        }
        this.scripts = scripts
    }

    public func serialize(): DataModel {
        let res = DataModelStruct()
        res.add(field<UInt64>(VERSION, this.version))
        res.add(field<HashMap<String, DepInfo>>("requires", this.requires))
        if (!this.scripts.isEmpty()) {
            res.add(field<HashMap<String, DepInfo>>("scripts", this.scripts))
        }
        return res
    }

    public static func deserialize(dm: DataModel): ModuleLock {
        var dms = match (dm) {
            case data: DataModelStruct => data
            case _ => throw Exception("this data is not DataModelStruct")
        }
        let version = UInt64.deserialize(dms.get(VERSION));
        if (version != MODULE_LOCK_VERSION) {
            throw Exception("cjpm.lock file has incompatible version")
        }
        var result = ModuleLock(HashMap<String, DepInfo>.deserialize(dms.get("requires")))
        result.version = version
        result.scripts = HashMap<String, DepInfo>.deserialize(
            (dms.get("scripts") as DataModelStruct) ?? DataModelStruct())
        return result
    }
}

// Reads one physical line without retaining the complete lock file as strings.
class LockLineReader {
    let data: Array<Byte>
    var pos: Int64 = 0

    init(data: Array<Byte>) {
        this.data = data
        if (data.size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            this.pos = 3
        }
    }

    func nextLine(): Option<String> {
        if (pos >= data.size) {
            return Option.None
        }
        let start = pos
        while (pos < data.size && data[pos] != b'\n') {
            pos++
        }
        var end = pos
        if (end > start && data[end - 1] == b'\r') {
            end--
        }
        // A canonical dependency occupies one line; cap its temporary String and parser allocations.
        if (end - start > MAX_LOCK_LINE_SIZE) {
            throw Exception("a line in '${LOCK_FILE_NAME}' exceeds ${MAX_LOCK_LINE_SIZE} bytes")
        }
        if (pos < data.size) {
            pos++
        }
        return String.fromUtf8(data[start..end])
    }
}

// Decodes the canonical format emitted by Encoder without building a TOML AST for the whole file.
// Each dependency line uses the normal TOML parser, then its temporary model is discarded.
func decodeCanonicalLock(data: Array<Byte>): ModuleLock {
    let reader = LockLineReader(data)
    let requires = HashMap<String, DepInfo>()
    let scripts = HashMap<String, DepInfo>()
    var section = ""
    var hasVersion = false
    var hasRequires = false
    var hasScripts = false
    var entryCount: Int64 = 0

    while (let Some(line) <- reader.nextLine()) {
        var trimmed = line.trimAscii()
        if (trimmed.isEmpty() || trimmed.startsWith("#")) {
            continue
        }
        if (trimmed == "[requires]" || trimmed == "[scripts]") {
            if (!hasVersion) {
                throw Exception("missing 'version = 0' before '${trimmed}' in ${LOCK_FILE_NAME}")
            }
            if (trimmed == "[requires]") {
                if (hasRequires) {
                    throw Exception("duplicate '[requires]' table in ${LOCK_FILE_NAME}")
                }
                hasRequires = true
            } else {
                if (hasScripts) {
                    throw Exception("duplicate '[scripts]' table in ${LOCK_FILE_NAME}")
                }
                hasScripts = true
            }
            section = trimmed
            continue
        }

        if (trimmed.startsWith("[[")) {
            throw Exception("unsupported array table in ${LOCK_FILE_NAME}; expected [requires] or [scripts]")
        }
        if (trimmed.startsWith("[")) {
            throw Exception("unsupported table '${trimmed}' in ${LOCK_FILE_NAME}")
        }

        if (section.isEmpty()) {
            let parts = trimmed.split("=")
            if (parts.size != 2 || parts[0].trimAscii() != "version" || parts[1].trimAscii() != "0") {
                throw Exception("invalid top-level field in ${LOCK_FILE_NAME}; expected 'version = 0'")
            }
            if (hasVersion) {
                throw Exception("duplicate 'version' field in ${LOCK_FILE_NAME}")
            }
            hasVersion = true
            continue
        }

        if (entryCount >= MAX_LOCK_ENTRY_COUNT) {
            throw Exception("${LOCK_FILE_NAME} contains too many dependency entries")
        }
        let entry = DecoderToml(line.toArray()).decode()
        var found = false
        for ((name, value) in entry.value) {
            if (found) {
                throw Exception("a lock entry must contain exactly one dependency")
            }
            let depInfo = DepInfo.deserialize(DataModel.fromToml(value))
            if (section == "[requires]") {
                if (requires.contains(name)) {
                    throw Exception("duplicate dependency '${name}' in '[requires]'")
                }
                requires.add(name, depInfo)
            } else {
                if (scripts.contains(name)) {
                    throw Exception("duplicate dependency '${name}' in '[scripts]'")
                }
                scripts.add(name, depInfo)
            }
            found = true
        }
        if (!found) {
            throw Exception("empty dependency entry in ${LOCK_FILE_NAME}")
        }
        entryCount++
    }

    if (!hasVersion) {
        throw Exception("missing 'version = 0' in ${LOCK_FILE_NAME}")
    }
    if (!hasRequires) {
        throw Exception("missing '[requires]' table in ${LOCK_FILE_NAME}")
    }
    let result = ModuleLock()
    result.requires = requires
    result.scripts = scripts
    return result
}

// The file size is checked before allocation; bounded chunks also handle streams that return short reads.
func readLockFile(file: File, fileSize: Int64): Array<Byte> {
    let data = Array<Byte>(fileSize, repeat: 0)
    let buffer = Array<Byte>(LOCK_READ_BUFFER_SIZE, repeat: 0)
    var offset: Int64 = 0
    while (offset < fileSize) {
        let readSize = file.read(buffer)
        if (readSize <= 0) {
            throw Exception("unexpected end of '${LOCK_FILE_NAME}'")
        }
        let remaining = fileSize - offset
        if (readSize > remaining) {
            throw Exception("'${LOCK_FILE_NAME}' changed while being read")
        }
        for (i in 0..readSize) {
            data[offset + i] = buffer[i]
        }
        offset += readSize
    }
    return data
}

public func loadLockFile(rootPath: String): (Bool, ModuleLock) {
    var res = ModuleLock()
    var lockFile: String = Path(rootPath).join(LOCK_FILE_NAME).toString()

    if (!fileExists(lockFile)) {
        // This file does not exist by default.
        return (true, res)
    }

    var data = Array<Byte>()
    try (file = File(lockFile, Read)) {
        let fileSize: Int64 = file.info.size
        if (fileSize > MAX_LOCK_FILE_SIZE) {
            throw Exception("'${LOCK_FILE_NAME}' exceeds the maximum supported size of ${MAX_LOCK_FILE_SIZE} bytes")
        }
        data = readLockFile(file, fileSize)
    } catch (e: Exception) {
        eprintln("Error: ${e.message}")
        eprintln("Error: parse the '${LOCK_FILE_NAME}' file failed")
        return (false, res)
    }

    try {
        res = decodeCanonicalLock(data)
    } catch (e: Exception) {
        // Keep compatibility with small hand-written lock files that use equivalent TOML nesting.
        if (data.size <= GENERIC_LOCK_FALLBACK_SIZE) {
            try {
                res = ModuleLock.deserialize(DataModel.fromToml(DecoderToml(data).decode()))
                return (true, res)
            } catch (_) {
                ()
            }
        }
        eprintln("Error: ${e.message}")
        eprintln("Error: parse the '${LOCK_FILE_NAME}' file failed")
        return (false, res)
    }

    return (true, res)
}

public func creatLockFile(rootPath: String, lockFile: ModuleLock): Bool {
    var lockFilePath: String = Path(rootPath).join(LOCK_FILE_NAME).toString()
    if (directoryExists(lockFilePath)) {
        eprintln("Error: failed to create '${LOCK_FILE_NAME}', because a directory with the same name already exists")
        return false
    }

    try {
        var f = File(lockFilePath, Write)
        let enc = EncoderToml(f)
        enc.encode(lockFile.serialize())
        f.close()
    } catch (e: Exception) {
        eprintln("Error: generate the '${LOCK_FILE_NAME}' file failed")
        return false
    }

    if (!modifyFilePermission(lockFilePath)) {
        return false
    }

    return true
}
