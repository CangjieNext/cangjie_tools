// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

import std.collection.*
import std.fs.*
import std.io.*
import std.regex.*
import std.sort.*
import stdx.serialization.serialization.*
import cjpm.toml.*
import cjpm.toml.Decoder as DecoderToml
import cjpm.toml.Encoder as EncoderToml

const LOCK_VERSION_V0: UInt64 = 0
const LOCK_VERSION_V1: UInt64 = 1
// Adding a reader does not change the default writer. Switch this constant
// together with encodeLockFile only when the new output format is ready.
public const CURRENT_LOCK_VERSION: UInt64 = LOCK_VERSION_V1
// Keep each format's accepted fields separate when introducing later versions.
// Ignoring an unknown field here would silently discard it on the next write.
// Separate Git fields remain accepted for earlier v1 files, but are no longer
// emitted: the writer merges their metadata into source after validation.
let V1_LOCK_ENTRY_FIELDS = HashSet<String>([
    "version", "source", "checksum", "dependencies", "output-type", "git", "commitId", "branch", "tag"
])
// Controls only the per-read I/O buffer and does not limit the lock file size.
const LOCK_READ_BUFFER_SIZE: Int64 = 8 * 1024

// Legacy v0 serialization model. New command paths use LockFile below;
// retain this model for the existing v0-only read/write APIs.
public class ModuleLock <: Serializable<ModuleLock> {
    public var requires: HashMap<String, DepInfo> = HashMap<String, DepInfo>()
    public var scripts: HashMap<String, DepInfo> = HashMap<String, DepInfo>()
    //Used so that we can change format of this file without breaking change
    var version: UInt64 = LOCK_VERSION_V0

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
        if (version != LOCK_VERSION_V0) {
            throw Exception("cjpm.lock file has incompatible version")
        }
        var result = ModuleLock(HashMap<String, DepInfo>.deserialize(dms.get("requires")))
        result.version = version
        result.scripts = HashMap<String, DepInfo>.deserialize(
            (dms.get("scripts") as DataModelStruct) ?? DataModelStruct())
        return result
    }
}

// Reads one physical line at a time without retaining the complete lock file.
class LockLineReader {
    let file: File
    let buffer: Array<Byte> = Array<Byte>(LOCK_READ_BUFFER_SIZE, repeat: 0)
    var bufferSize: Int64 = 0
    var bufferPos: Int64 = 0
    var firstLine = true

    init(file: File) {
        this.file = file
    }

    func nextByte(): Option<Byte> {
        if (bufferPos >= bufferSize) {
            let size = file.read(buffer)
            if (size <= 0) {
                return Option.None
            }
            bufferSize = size
            bufferPos = 0
        }
        let result = buffer[bufferPos]
        bufferPos++
        return Option.Some(result)
    }

    func nextLine(): Option<String> {
        let line = ByteBuffer()
        var hasByte = false
        while (let Some(value) <- nextByte()) {
            hasByte = true
            if (value == b'\n') {
                break
            }
            line.writeByte(value)
        }
        if (!hasByte) {
            return Option.None
        }

        let bytes = line.bytes()
        var start: Int64 = 0
        var end: Int64 = bytes.size
        if (firstLine && end >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
            start = 3
        }
        firstLine = false
        if (end > start && bytes[end - 1] == b'\r') {
            end--
        }
        return String.fromUtf8(bytes[start..end])
    }
}

// Decodes the canonical format emitted by Encoder without building a TOML tree for the whole file.
// Each dependency line is converted directly to DepInfo before its temporary TOML objects are released.
func decodeCanonicalLock(file: File): ModuleLock {
    let reader = LockLineReader(file)
    let requires = HashMap<String, DepInfo>()
    let scripts = HashMap<String, DepInfo>()
    var section = ""
    var hasVersion = false
    var hasRequires = false
    var hasScripts = false

    while (let Some(line) <- reader.nextLine()) {
        let trimmed = line.trimAscii()
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
            throw Exception("invalid '${LOCK_FILE_NAME}' format: found array table '${trimmed}'; " +
                "expected 'version = 0' followed by '[requires]' and optional '[scripts]' tables")
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

public func loadLockFile(rootPath: String): (Bool, ModuleLock) {
    var res = ModuleLock()
    var lockFile: String = Path(rootPath).join(LOCK_FILE_NAME).toString()

    if (!fileExists(lockFile)) {
        // This file does not exist by default.
        return (true, res)
    }

    try (file = File(lockFile, Read)) {
        res = decodeCanonicalLock(file)
    } catch (e: Exception) {
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

// Version-neutral internal model shared by the resolver and commands.
// Do not infer a local source from a v0 entry with no source field:
// v0 registry and Git entries lack that field as well.
public class LockEntry {
    public var version: ?String = None
    public var outputType: ?OutputType = None
    // Compatibility metadata from earlier v1 writers; new output folds these
    // fields into source. Selectors are also decoded for command comparisons.
    public var git: ?String = None
    public var commitId: ?String = None
    public var branch: ?String = None
    public var tag: ?String = None
    public var source: ?String = None
    public var checksum: ?String = None
    public var dependencies: ArrayList<String> = ArrayList<String>()
    // Retain the exact v0 resolver input, including branch/tag/output-type.
    // Migration must resolve missing metadata before encoding a v1 entry.
    public var legacy: ?DepInfo = None
    // Graph collection precedes source/checksum collection. A remote graph node
    // must not be serialized as a local module during that intermediate stage.
    public var sourcePending: Bool = false

    public init() {}
}

public class LockFile {
    // Format read from disk, or the current format for a newly resolved graph.
    // Changing this value alone does not complete migration: each entry must
    // also pass the output format's metadata checks.
    public var formatVersion: UInt64 = CURRENT_LOCK_VERSION
    // Each section owns its dependency graph; references cannot cross sections.
    public let requires = HashMap<String, LockEntry>()
    public let scripts = HashMap<String, LockEntry>()

    public init() {}
}

func lockString(table: TomlObject, key: String): String {
    let value = table.get(key) ?? throw Exception("missing '${key}' in lock entry")
    match (value) {
        case text: TomlString => text.value
        case _ => throw Exception("lock field '${key}' must be a string")
    }
}

func checkLockName(name: String): Unit {
    // Module references are names, never 'name version' or filesystem paths.
    if (!Regex("^[_a-zA-Z][_a-zA-Z0-9-]*(::[_a-zA-Z][_a-zA-Z0-9-]*)?$").matches(name)) {
        throw Exception("invalid module name in lock file")
    }
}

func sortedLockDependencies(dependencies: ArrayList<String>): Array<String> {
    let unique = HashSet<String>()
    for (name in dependencies) {
        checkLockName(name)
        unique.add(name)
    }
    let result = unique.toArray()
    sort(result)
    return result
}

func hexDigit(value: UInt8): UInt8 {
    if (value <= UInt8(9)) { return UInt8(UInt32(r'0') + UInt32(value)) }
    return UInt8(UInt32(r'A') + UInt32(value - UInt8(10)))
}

func fromHexDigit(value: UInt8): UInt8 {
    if (value >= b'0' && value <= b'9') { return value - b'0' }
    if (value >= b'a' && value <= b'f') { return value - b'a' + UInt8(10) }
    if (value >= b'A' && value <= b'F') { return value - b'A' + UInt8(10) }
    throw Exception("invalid Git selector escape in lock source")
}

// Encode selector bytes as RFC 3986 query components so refs containing
// reserved characters cannot be confused with source delimiters.
// Encode UTF-8 bytes, including '/' in branch names, with uppercase hex digits
// to give equivalent selectors a stable representation on write.
func encodeGitSelector(selector: String): String {
    if (selector.isEmpty() || Regex("[\\x00-\\x20\\x7f]").findAll(selector).size > 0) {
        throw Exception("invalid Git selector in lock source")
    }
    let result = ByteBuffer()
    for (byte in selector.toArray()) {
        let safe = (byte >= b'a' && byte <= b'z') ||
            (byte >= b'A' && byte <= b'Z') ||
            (byte >= b'0' && byte <= b'9') ||
            byte == b'-' || byte == b'.' || byte == b'_' || byte == b'~'
        if (safe) {
            result.writeByte(byte)
        } else {
            result.writeByte(b'%')
            result.writeByte(hexDigit(byte / UInt8(16)))
            result.writeByte(hexDigit(byte % UInt8(16)))
        }
    }
    return String.fromUtf8(result.bytes())
}

func decodeGitSelector(selector: String): String {
    let input = selector.toArray()
    let result = ByteBuffer()
    var index: Int64 = 0
    while (index < input.size) {
        if (input[index] == b'%') {
            if (index + 2 >= input.size) { throw Exception("invalid Git selector escape in lock source") }
            result.writeByte(fromHexDigit(input[index + 1]) * UInt8(16) + fromHexDigit(input[index + 2]))
            index += 3
        } else {
            // Query delimiters must be escaped; '+' is not a space escape.
            let byte = input[index]
            if (!((byte >= b'a' && byte <= b'z') || (byte >= b'A' && byte <= b'Z') ||
                (byte >= b'0' && byte <= b'9') || byte == b'-' || byte == b'.' || byte == b'_' || byte == b'~')) {
                throw Exception("invalid Git selector in lock source")
            }
            result.writeByte(byte)
            index++
        }
    }
    try {
        let decoded = String.fromUtf8(result.bytes())
        // Validate after unescaping too, so encoded whitespace/control bytes
        // cannot bypass the constraints applied to manifest selectors.
        encodeGitSelector(decoded)
        return decoded
    } catch (_: Exception) {
        throw Exception("invalid Git selector encoding in lock source")
    }
}

public func parseGitLockSource(source: String): (String, ?String, ?String, String) {
    // Keep the selector before the final fragment, which always identifies the
    // resolved commit and is the revision used by the downloader.
    if (!source.startsWith("git+")) { throw Exception("invalid Git lock source") }
    let parts = source[4..].split("#")
    if (parts.size != 2) { throw Exception("git source must contain a URL and a full resolved commit") }
    let location = parts[0].split("?")
    if (location.size > 2) { throw Exception("invalid Git selector in lock source") }
    var branch: ?String = None
    var tag: ?String = None
    if (location.size == 2) {
        let selector = location[1].split("=")
        if (selector.size != 2 || selector[1].isEmpty()) {
            throw Exception("invalid Git selector in lock source")
        }
        let value = Some(decodeGitSelector(selector[1]))
        if (selector[0] == "branch") { branch = value }
        else if (selector[0] == "tag") { tag = value }
        else { throw Exception("invalid Git selector in lock source") }
    }
    if (location[0].isEmpty() || !Regex("^([0-9a-f]{40}|[0-9a-f]{64})$").matches(parts[1])) {
        throw Exception("git source must contain a URL and a full resolved commit")
    }
    return (normalizeLockSourceUrl(location[0]), branch, tag, parts[1])
}

// Callers supply a normalized repository URL and a resolved full commit.
// This query is lock metadata, not part of the URL sent to Git.
public func formatGitLockSource(url: String, branch: ?String, tag: ?String, commit: String): String {
    if (branch.isSome() && tag.isSome()) { throw Exception("lock entry cannot contain both branch and tag") }
    let selector = if (let Some(value) <- branch) { "?branch=${encodeGitSelector(value)}" }
        else if (let Some(value) <- tag) { "?tag=${encodeGitSelector(value)}" }
        else { "" }
    return "git+${url}${selector}#${commit}"
}

// Used both after decoding v1 and before encoding resolved entries, so generated
// locks obey the same contract as files read from disk.
func validateV1Entry(info: LockEntry): Unit {
    let version = info.version ?? throw Exception("missing version in lock entry")
    if (!Regex(REGEXP_VERSION).matches(version)) {
        throw Exception("lock entry version must be an exact module version")
    }
    if (info.legacy.isSome()) {
        throw Exception("v0 lock entry must be resolved before writing v1")
    }
    if (info.sourcePending) {
        throw Exception("lock entry source metadata must be resolved before writing v1")
    }
    if (let Some(outputType) <- info.outputType && !outputType.isValid) {
        throw Exception("invalid output-type in lock entry")
    }
    let isGit = if (let Some(source) <- info.source) { source.startsWith("git+") } else { false }
    if (!isGit && (info.git.isSome() || info.commitId.isSome() || info.branch.isSome() || info.tag.isSome())) {
        throw Exception("Git fields require a git source in lock entry")
    }
    if (info.branch.isSome() && info.tag.isSome()) {
        throw Exception("lock entry cannot contain both branch and tag")
    }
    if (let Some(source) <- info.source) {
        // Detailed URL normalization and credential handling belongs to source
        // collection. These checks validate the lock format without echoing URLs.
        if (source.startsWith("registry+")) {
            if (source.size <= "registry+".size) {
                throw Exception("registry source URL cannot be empty")
            }
            normalizeLockSourceUrl(source[9..], registry: true)
            let checksum = info.checksum ?? throw Exception("registry lock entry requires checksum")
            if (!Regex("^sha256:[0-9a-f]{64}$").matches(checksum)) {
                throw Exception("checksum must be 'sha256:' followed by 64 lowercase hexadecimal digits")
            }
        } else if (source.startsWith("git+")) {
            let (url, sourceBranch, sourceTag, commit) = parseGitLockSource(source)
            if (url.isEmpty() || !Regex("^([0-9a-f]{40}|[0-9a-f]{64})$").matches(commit)) {
                throw Exception("git source must contain a URL and a full resolved commit")
            }
            normalizeLockSourceUrl(url)
            // Accept earlier v1's separate fields, but reject disagreement with
            // the canonical source representation before rewriting the entry.
            if (let Some(git) <- info.git && normalizeLockSourceUrl(git) != normalizeLockSourceUrl(url)) {
                throw Exception("lock git field disagrees with source")
            }
            if (let Some(lockedCommit) <- info.commitId && lockedCommit != commit) {
                throw Exception("lock commitId field disagrees with source")
            }
            if (sourceBranch.isSome() && info.branch.isSome() && sourceBranch != info.branch) {
                throw Exception("lock branch field disagrees with source")
            }
            if (sourceTag.isSome() && info.tag.isSome() && sourceTag != info.tag) {
                throw Exception("lock tag field disagrees with source")
            }
            if ((sourceBranch.isSome() && info.tag.isSome()) || (sourceTag.isSome() && info.branch.isSome())) {
                throw Exception("lock entry cannot contain both branch and tag")
            }
            if (let Some(branch) <- info.branch) { encodeGitSelector(branch) }
            if (let Some(tag) <- info.tag) { encodeGitSelector(tag) }
            if (info.checksum.isSome()) {
                throw Exception("git lock entry cannot contain checksum")
            }
        } else {
            throw Exception("lock source must start with 'registry+' or 'git+'");
        }
    } else if (info.checksum.isSome()) {
        throw Exception("local lock entry cannot contain checksum")
    }
    for (name in info.dependencies) {
        checkLockName(name)
    }
}

// Convert only the v0 fields that remain meaningful to the shared resolved
// model. The original DepInfo stays attached until resolution can complete v1.
// Leave source/checksum/dependency edges unresolved rather than inventing them.
func migrateV0Entry(legacy: DepInfo): LockEntry {
    let result = LockEntry()
    result.legacy = Some(legacy)
    result.version = legacy.version
    result.outputType = legacy.outputType
    result.git = legacy.git
    result.commitId = legacy.commitId
    result.branch = legacy.branch
    result.tag = legacy.tag
    return result
}

func decodeV0Entry(value: TomlValue): LockEntry {
    // Reuse v0 parsing for shorthand versions and Git entries without a version.
    return migrateV0Entry(DepInfo.deserialize(DataModel.fromToml(value)))
}

func decodeV1Entry(value: TomlValue): LockEntry {
    let result = LockEntry()
    let table = match (value) {
        case obj: TomlObject => obj
        case _ => throw Exception("v1 lock entry must be a table")
    }
    for ((key, _) in table.value) {
        if (!V1_LOCK_ENTRY_FIELDS.contains(key)) {
            throw Exception("unknown field in v1 lock entry")
        }
    }
    result.version = Some(lockString(table, "version"))
    if (table.contains("output-type")) {
        result.outputType = Some(OutputType.fromString(lockString(table, "output-type")))
    }
    if (table.contains("git")) { result.git = Some(lockString(table, "git")) }
    if (table.contains("commitId")) { result.commitId = Some(lockString(table, "commitId")) }
    if (table.contains("branch")) { result.branch = Some(lockString(table, "branch")) }
    if (table.contains("tag")) { result.tag = Some(lockString(table, "tag")) }
    if (table.contains("source")) {
        result.source = Some(lockString(table, "source"))
        let source = result.source.getOrThrow()
        if (source.startsWith("git+")) {
            // Fill only missing fields: keeping explicit legacy values lets
            // validateV1Entry detect disagreement instead of hiding it.
            let (_, sourceBranch, sourceTag, _) = parseGitLockSource(source)
            if (result.branch.isNone()) { result.branch = sourceBranch }
            if (result.tag.isNone()) { result.tag = sourceTag }
        }
    }
    if (table.contains("checksum")) {
        result.checksum = Some(lockString(table, "checksum"))
    }
    let dependencies = table.get("dependencies") ?? throw Exception("missing dependencies in lock entry")
    match (dependencies) {
        case array: TomlArray =>
            for (element in array.value) {
                match (element) {
                    case text: TomlString => result.dependencies.add(text.value)
                    case _ => throw Exception("lock dependencies must contain only strings")
                }
            }
        case _ => throw Exception("lock dependencies must be an array")
    }
    validateV1Entry(result)
    result.dependencies = ArrayList<String>(sortedLockDependencies(result.dependencies))
    return result
}

func decodeLockTable(value: TomlValue, entries: HashMap<String, LockEntry>,
    decodeEntry: (TomlValue) -> LockEntry): Unit {
    let table = match (value) {
        case obj: TomlObject => obj
        case _ => throw Exception("lock dependency section must be a table")
    }
    for ((name, entry) in table.value) {
        entries.add(name, decodeEntry(entry))
    }
}

// Share only section traversal; the supplied decoder owns version-specific
// field rules. Keep the input format until resolution produces a new lock.
func decodeLockSections(root: TomlObject, formatVersion: UInt64,
    decodeEntry: (TomlValue) -> LockEntry): LockFile {
    let result = LockFile()
    result.formatVersion = formatVersion
    let requires = root.get("requires") ?? throw Exception("missing requires table in cjpm.lock")
    decodeLockTable(requires, result.requires, decodeEntry)
    if (let Some(scripts) <- root.get("scripts")) {
        decodeLockTable(scripts, result.scripts, decodeEntry)
    }
    return result
}

func decodeV0LockFile(root: TomlObject): LockFile {
    // v0 has no complete dependency graph, so v1 graph validation belongs after
    // resolution, not in this compatibility reader.
    return decodeLockSections(root, LOCK_VERSION_V0, decodeV0Entry)
}

func decodeV1LockFile(root: TomlObject): LockFile {
    let result = decodeLockSections(root, LOCK_VERSION_V1, decodeV1Entry)
    // Validate references after all entries have been read, allowing forward
    // references while still rejecting dependencies outside their own section.
    validateV1Table(result.requires)
    validateV1Table(result.scripts)
    return result
}

func validateV1Table(entries: HashMap<String, LockEntry>): Unit {
    for ((name, entry) in entries) {
        checkLockName(name)
        validateV1Entry(entry)
        for (dependency in entry.dependencies) {
            if (!entries.contains(dependency)) {
                throw Exception("dangling dependency '${dependency}' referenced by '${name}' in lock section")
            }
        }
    }
}

// Parse both versions and preserve unresolved legacy metadata for migration.
public func decodeCompatibleLockFile(bytes: Array<Byte>): LockFile {
    let root = try {
        DecoderToml(bytes).decode()
    } catch (_: Exception) {
        // Parser diagnostics may contain input tokens (including credentials).
        throw Exception("invalid TOML in cjpm.lock (syntax error or duplicate definition)")
    }
    for ((key, _) in root.value) {
        if (key != "version" && key != "requires" && key != "scripts") {
            throw Exception("unknown top-level field in cjpm.lock")
        }
    }
    let versionValue = root.get("version") ?? throw Exception("missing version in cjpm.lock")
    let version = match (versionValue) {
        case number: TomlInteger => number.value
        case _ => throw Exception("cjpm.lock version must be an integer")
    }
    // This is the only input-format dispatch point. Unknown formats must not
    // reach a known decoder and silently lose fields when rewritten.
    if (version == Int64(LOCK_VERSION_V0)) { return decodeV0LockFile(root) }
    if (version == Int64(LOCK_VERSION_V1)) { return decodeV1LockFile(root) }
    throw Exception("cjpm.lock file has incompatible version")
}

func encodeV1Table(buffer: ByteBuffer, name: String, entries: HashMap<String, LockEntry>): Unit {
    if (entries.isEmpty()) {
        buffer.write("\n[${name}]\n".toArray())
        return
    }
    // Stable module, field and dependency ordering avoids changes caused only
    // by hash-map traversal and lets identical output skip the physical write.
    let names = ArrayList<String>(entries.keys()).toArray()
    sort(names)
    for (moduleName in names) {
        let info = entries[moduleName]
        buffer.write("\n[${name}.${TomlString(moduleName)}]\n".toArray())
        let fields = DataModelStruct()
            .add(field<String>("version", info.version.getOrThrow()))
            .addFieldIfSome("output-type", info.outputType)
            // Earlier v1 files may carry selector metadata separately. Fold it
            // into source so every newly written lock has one Git identity.
            .addFieldIfSome("source", if (let Some(source) <- info.source && source.startsWith("git+")) {
                let (url, sourceBranch, sourceTag, commit) = parseGitLockSource(source)
                Some(formatGitLockSource(url,
                    sourceBranch ?? info.branch, sourceTag ?? info.tag, commit))
            } else { info.source })
            .addFieldIfSome("checksum", info.checksum)
            .add(field<Array<String>>("dependencies", sortedLockDependencies(info.dependencies)))
        // Only scalar fields and a string array: no changes to the shared TOML
        // encoder's inline-table rules or cjpm.toml serialization are needed.
        EncoderToml(buffer).encode(fields)
    }
}

// Command callers always write the current format. Update this delegation
// together with CURRENT_LOCK_VERSION when a future format becomes the default.
public func encodeLockFile(lockFile: LockFile): Array<Byte> {
    return encodeV1LockFile(lockFile)
}

func encodeV1LockFile(lockFile: LockFile): Array<Byte> {
    if (lockFile.formatVersion != LOCK_VERSION_V1) {
        throw Exception("resolve v0 metadata before writing a v1 lock file")
    }
    validateV1Table(lockFile.requires)
    validateV1Table(lockFile.scripts)
    let buffer = ByteBuffer()
    buffer.write("version = ${LOCK_VERSION_V1}\n".toArray())
    encodeV1Table(buffer, "requires", lockFile.requires)
    if (!lockFile.scripts.isEmpty()) {
        encodeV1Table(buffer, "scripts", lockFile.scripts)
    }
    return buffer.bytes()
}

// A missing lock starts a new graph; an unreadable lock is an error. Callers
// must check the flag rather than treating the error's empty model as a new lock.
public func loadCompatibleLockFile(rootPath: String): (Bool, LockFile) {
    let path = Path(rootPath).join(LOCK_FILE_NAME).toString()
    // fileExists only recognizes regular files; a directory is a conflict,
    // not a missing lock that can be replaced at the end of the command.
    if (directoryExists(path)) {
        eprintln("Error: failed to create '${LOCK_FILE_NAME}', because a directory with the same name already exists")
        return (false, LockFile())
    }
    if (!fileExists(path)) {
        return (true, LockFile())
    }
    try {
        return (true, decodeCompatibleLockFile(File.readFrom(path)))
    } catch (_: Exception) {
        eprintln("Error: failed to read '${LOCK_FILE_NAME}' (invalid format or file access)")
        return (false, LockFile())
    }
}

// Serialize and set permissions on a sibling temporary file before replacement.
// A failed command or write must never truncate the previous lock.
public func writeLockFile(rootPath: String, lockFile: LockFile): Bool {
    let path = Path(rootPath).join(LOCK_FILE_NAME).toString()
    try {
        // Protect direct callers as well as commands that loaded the lock.
        if (directoryExists(path)) {
            eprintln("Error: failed to create '${LOCK_FILE_NAME}', because a directory with the same name already exists")
            return false
        }
        let bytes = encodeLockFile(lockFile)
        if (fileExists(path) && File.readFrom(path) == bytes) { return true }
        let temp = File.createTemp(Path(path).parent)
        let tempPath = temp.info.path
        try {
            temp.write(bytes)
            temp.close()
            if (!modifyFilePermission(tempPath.toString())) { return false }
            // Recheck after preparing the temporary file: overwrite can remove
            // a directory created since the initial check.
            if (directoryExists(path)) {
                eprintln("Error: failed to create '${LOCK_FILE_NAME}', because a directory with the same name already exists")
                return false
            }
            rename(tempPath, to: Path(path), overwrite: true)
        } finally {
            temp.close()
            if (fileExists(tempPath.toString())) { remove(tempPath) }
        }
        return true
    } catch (_: Exception) {
        eprintln("Error: failed to generate '${LOCK_FILE_NAME}'")
        return false
    }
}

// Do not include the input in errors. Query strings may carry opaque tokens;
// rejecting them is safer than guessing which parameters contain credentials.
// SSH user names identify the remote account, and are not passwords.
public func normalizeLockSourceUrl(url: String, registry!: Bool = false): String {
    if (url.isEmpty() || !checkSafeUrl(url) || url.contains("?") || url.contains("#") ||
        url.contains("\\") || Regex("[\\x00-\\x20\\x7f]").findAll(url).size > 0) {
        throw Exception("dependency source cannot be safely recorded")
    }
    let parts = url.split("://")
    if (parts.size == 1 && !registry) {
        if (url.startsWith("/")) { return url }
        // Git also accepts relative repository paths (repo, ./repo, ../repo).
        // Preserve their spelling and the existing resolution against the
        // command's working directory, including when reading a locked source.
        // A colon can denote an SCP address or scheme, so handle it separately.
        if (!url.contains(":")) { return url }
        // SCP-style Git URL, for example git@example.com:org/repo.git.
        if (Regex("^([a-zA-Z0-9_-]+@)?[a-zA-Z0-9.-]+:[^@]+$").matches(url)) {
            let address = url.split(":")[0]
            let hostParts = address.split("@")
            let host = hostParts[hostParts.size - 1].toAsciiLower()
            let user = if (hostParts.size == 2) { "${hostParts[0]}@" } else { "" }
            return "${user}${host}:${url[address.size + 1..]}"
        }
        throw Exception("dependency source cannot be safely recorded")
    }
    if (parts.size != 2) { throw Exception("dependency source cannot be safely recorded") }
    let scheme = parts[0].toAsciiLower()
    if (scheme != "http" && scheme != "https" &&
        (registry || (scheme != "ssh" && scheme != "git" && scheme != "file"))) {
        throw Exception("unsupported dependency source scheme")
    }
    let authority = parts[1].split("/")[0]
    var host = authority
    var user = ""
    if (authority.contains("@")) {
        let credentials = authority.split("@")
        if (scheme != "ssh" || credentials.size != 2 ||
            !Regex("^[a-zA-Z0-9_-]+$").matches(credentials[0])) {
            throw Exception("dependency source contains credentials")
        }
        user = "${credentials[0]}@"
        host = credentials[1]
    }
    if ((host.isEmpty() && scheme != "file") ||
        (!host.isEmpty() && !Regex("^[a-zA-Z0-9.\\[\\]:-]+$").matches(host))) {
        throw Exception("invalid dependency source authority")
    }
    let path = parts[1][authority.size..]
    if (scheme == "file" && path.isEmpty()) {
        throw Exception("invalid dependency source path")
    }
    let result = "${scheme}://${user}${host.toAsciiLower()}${path}"
    return if (registry) { result.removeSuffix("/") } else { result }
}
