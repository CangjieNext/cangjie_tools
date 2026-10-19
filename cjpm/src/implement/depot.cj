// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.net.*
import std.io.*
import std.fs.*
import std.collection.*
import std.regex.*
import stdx.net.http.*
import stdx.net.tls.common.*
import stdx.net.tls.*
import stdx.crypto.digest.*
import stdx.encoding.hex.*
import stdx.serialization.serialization.*
import stdx.encoding.json.*
import cjpm.config.*
import stdx.crypto.common.CryptoException

const CONNECT_TIMEOUT = 30
const READ_TIMEOUT = 60

let tcpSocketConnector = {
    sa: SocketAddress =>
        let socket = TcpSocket(sa)
        socket.connect(timeout: Duration.second * CONNECT_TIMEOUT)
        return socket
}

public class Depot {
    public var baseUrl: String = ""
    public var token: String = ""
    public var localRepo: String = ""
    public var global: GlobalRepoConfig = GlobalRepoConfig()

    public func loadConfiguration(isError!: Bool = true): Bool {
        let alertLevel =  if (isError) { "Error" } else { "Warning" }
        if (!FINAL_CJPM_CONFIG_INFO.repository.contains(REPO_HOME_NAME)) {
            eprintln("${alertLevel}: get data of [repository.home] failed, please check if cangjie-repo.toml is set up correctly")
            return false
        }

        this.baseUrl = FINAL_CJPM_CONFIG_INFO.repository[REPO_HOME_NAME].registry
        this.token = FINAL_CJPM_CONFIG_INFO.repository[REPO_HOME_NAME].token
        this.localRepo = Path(FINAL_CJPM_CONFIG_INFO.repository[REPO_CACHE_NAME].path).join("repository").toString()

        if (baseUrl.isEmpty()) {
            eprintln("${alertLevel}: get registry of [repository.home] failed, please check if cangjie-repo.toml is set up correctly")
            return false
        }

        try {
            normalizeLockSourceUrl(baseUrl, registry: true)
        } catch (_: Exception) {
            eprintln("${alertLevel}: registry source cannot be safely recorded; use the separate token setting for credentials")
            return false
        }

        this.global = FINAL_CJPM_CONFIG_INFO.global

        return true
    }

    public static func new(): Option<Depot> {
        let instance: Depot = Depot()
        if (instance.loadConfiguration()) {
            return Option<Depot>.Some(instance)
        }
        return Option<Depot>.None
    }

    // publish data to central repository
    public func publish(tomlInfo: TomlInfo, data: Array<Byte>): (Bool, String) {
        let name = tomlInfo.pkg.name
        let version = tomlInfo.pkg.version
        let org = tomlInfo.pkg.organization

        // check name and organization
        var flag = if (org.isEmpty()) { true } else { checkBundleName(org, "organization") }
        flag = checkBundleName(name, "module") && flag
        if (!flag) {
            return (false, "")
        }

        var publishUrl = baseUrl + "/pkg/${name}"
        var orgStr = ""
        if (!org.isEmpty()) {
            publishUrl += "?organization=${org}"
            orgStr = "to organization '${org}' "
        }

        // building client
        var tlsConfig = TlsClientConfig()
        if (!this.global.strictTls) {
            tlsConfig.verifyMode = TrustAll
        }
        let client = ClientBuilder().
            connector(tcpSocketConnector).
            noProxy().
            readTimeout(Duration.second * READ_TIMEOUT).
            tlsConfig(tlsConfig).
            build()

        // building https request
        var body = ByteBuffer()
        body.write(data)
        let request = HttpRequestBuilder().
            method("POST").
            url(publishUrl).
            header("Authorization", token).
            body(body).
            build()
        let resp = client.send(request)
        client.close()

        // verify response code
        match (resp.status) {
            case 200 => return (true, "${name}-${version} ${orgStr}publish success")
            case 400 =>
                let respMsg = extractErrorMessage(resp)
                return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: invalid parameters: ${respMsg}")
            case 401 => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: authentication failed")
            case 403 => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: permission denied or the artifact name already in use")
            case 404 => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: file does not exist")
            case 409 => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: artifact with the same name and verison already exists")
            case 429 => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: rate limit exceeded, retry after one day")
            case s where (s >= 500) => return (false, "${name}-${version} ${orgStr}publish failed with status ${resp.status}: server error")
            case _ => return (false, "${name}-${version} ${orgStr}publish failed, unknown error with status ${resp.status}")
        }
    }

    // download package from central repository
    public func download(name: String, org: String, version: String,
        expectedChecksum!: ?String = None): (Bool, String) {
        var downloadUrl = baseUrl + "/pkg/${name}/${version}"
        var orgStr = ""
        if (!org.isEmpty()) {
            downloadUrl += "?organization=${org}"
            orgStr = "from organization '${org}' "
        }

        // building client
        var tlsConfig = TlsClientConfig()
        if (!this.global.strictTls) {
            tlsConfig.verifyMode = TrustAll
        }
        let client = ClientBuilder().
            connector(tcpSocketConnector).
            noProxy().
            readTimeout(Duration.second * READ_TIMEOUT).
            tlsConfig(tlsConfig).
            build()

        // building https request
        let request = HttpRequestBuilder().method("GET").url(downloadUrl).build()
        let resp = client.send(request)

        // verify response code
        var (flag, errLog) = (true, "")
        match (resp.status) {
            case 200 => ()
            case 400 =>
                let respMsg = extractErrorMessage(resp)
                (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: invalid parameters: ${respMsg}")
            case 401 => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: authentication failed")
            case 403 => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: permission denied")
            case 404 => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: file does not exist")
            case 429 => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: rate limit exceeded, retry after one day")
            case s where (s >= 500) => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed with status ${resp.status}: server error")
            case _ => (flag, errLog) = (false, "${name}-${version} ${orgStr}load failed, unknown error with status ${resp.status}")
        }
        if (!flag) {
            client.close()
            return (flag, errLog)
        }

        let buf = Array<UInt8>(1024, repeat: 0)
        let buffer = ArrayList<UInt8>()
        var i = 0
        do {
            i = resp.body.read(buf)
            buffer.add(all: buf[..i])
        } while (i != 0)
        client.close()

        // check sha256sum
        try {
            let sha256Instance = SHA256()
            sha256Instance.write(buffer.toArray())
            let sha256sum = toHexString(sha256Instance.finish())
            if (let Some(expected) <- expectedChecksum) {
                if ("sha256:${sha256sum}" != expected) {
                    return (false, "downloaded artifact checksum mismatch, please try 'cjpm update'")
                }
                // Retain exactly the verified bytes for later offline validation.
                try {
                    saveVerifiedArtifact(this, name, org, version, buffer.toArray())
                } catch (_: Exception) {
                    return (false, "failed to save verified artifact ${name}-${version} ${orgStr}to cache")
                }
                return uncompressCjp(org, name, version, buffer.toArray(), this.localRepo,
                    replaceSource: true)
            }
            let indexPath = this.getIndex(name, org)
            if (indexPath.isEmpty()) {
                return (false, "index error while downloading ${name}-${version} ${orgStr}")
            }

            var indexRawData = String.fromUtf8(File.readFrom(indexPath))
            var indexIter = indexRawData.lines()

            var checkSumFlag = false
            while (true) {
                match (indexIter.next()) {
                    case Some(indexStr) =>
                        try {
                            let index: ArtifactIndex = ArtifactIndex.deserialize(DataModel.fromJson(JsonValue.fromStr(indexStr)))
                            if (index.version != version) {
                                continue
                            }
                            if (index.sha256sum != sha256sum) {
                                return (false, "package was broken while downloading ${name}-${version} ${orgStr}, please try 'cjpm update'")
                            }
                            checkSumFlag = true
                            break
                        } catch (e: Exception) {
                            return (false, "index was broken while downloading ${name}-${version} ${orgStr}, please try 'cjpm update'")
                        }
                    case None => break
                }
            }

            if (!checkSumFlag) {
                return (false, "cannot find index data while downloading ${name}-${version} ${orgStr}, please try 'cjpm update'")
            }
        } catch (e: CryptoException) {
            eprintln("Error: ${e.message}")
            eprintln("Error: please ensure that openssl has been correctly installed and configured according to the \"Usage Instruction\" section of the documentation")
            return (false, "failed to check sha256 check-sum due to missing OpenSSL")
        } catch (e: Exception) {
            return (false, "failed to check sha256 check-sum while downloading ${name}-${version} ${orgStr}, please try 'cjpm update'")
        }

        // save, uncompress and delete cjp file
        return uncompressCjp(org, name, version, buffer.toArray(), this.localRepo)
    }

    // update local index for one module with output
    public func updateIndexWithOutput(name: String, org: String): (Bool, String) {
        let firstLevel = name[..2]
        let secondLevel = if (name.size > 3) { name[2..4] } else { name[2..3] }
        var indexUrl = baseUrl + "/index/${firstLevel}/${secondLevel}/${name}"
        var orgStr = ""
        if (!org.isEmpty()) {
            indexUrl += "?organization=${org}"
            orgStr = " from organization '${org}'"
        }

        // building client
        var tlsConfig = TlsClientConfig()
        if (!this.global.strictTls) {
            tlsConfig.verifyMode = TrustAll
        }
        let client = ClientBuilder().
            connector(tcpSocketConnector).
            noProxy().
            readTimeout(Duration.second * READ_TIMEOUT).
            tlsConfig(tlsConfig).
            build()

        // building https request
        let request = HttpRequestBuilder().method("GET").url(indexUrl).build()
        let resp = client.send(request)

        // verify response code
        var (flag, errLog) = (true, "")
        match (resp.status) {
            case 200 => ()
            case 400 =>
                let respMsg = extractErrorMessage(resp)
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed with status ${resp.status}: invalid parameters: ${respMsg}")
            case 401 =>
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed with status ${resp.status}: authentication failed")
            case 403 =>
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed with status ${resp.status}: permission denied")
            case 404 =>
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed with status ${resp.status}: file does not exist")
            case s where (s >= 500) =>
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed with status ${resp.status}: server error")
            case _ =>
                (flag, errLog) = (false, "index of module '${name}'${orgStr} load failed, unknown error with status ${resp.status}")
        }
        if (!flag) {
            client.close()
            return (flag, errLog)
        }

        let buf = Array<UInt8>(1024, repeat: 0)
        let buffer = ArrayList<UInt8>()
        var i = 0
        do {
            i = resp.body.read(buf)
            buffer.add(all: buf[..i])
        } while (i != 0)
        client.close()

        // save index file
        let indexDir = Path(this.localRepo).join("index").
            join(if (org.isEmpty()) { "default" } else { org }).
            join(firstLevel).
            join(secondLevel).
            toString()
        let indexPath = Path(indexDir).join(name)
        if (!createDirectory(indexDir)) {
            return (false, "cannot save index file for module '${name}'${orgStr}")
        }
        File.writeTo(indexPath, buffer.toArray())

        return (true, "")
    }

    // update local index for one module
    public func updateIndex(name: String, org: String): Bool {
        let (flag, output) = this.updateIndexWithOutput(name, org)
        if (!flag) {
            eprintln("Error: ${output}")
        }
        return flag
    }

    private static func extractErrorMessage(resp: HttpResponse): String {
        let buffer = ArrayList<UInt8>()
        while (true) {
            let buf = Array<UInt8>(1024, repeat: 0)
            let readSize = resp.body.read(buf)
            if (readSize == 0) {
                break
            }
            buffer.add(all: buf[..readSize])
        }
        let respMsg = String.fromUtf8(buffer.toArray())
        try {
            if (let Some(errorsVal) <- JsonValue.fromStr(respMsg).asObject().get("errors")) {
                let errArray = errorsVal.asArray()
                for (idx in 0..errArray.size()) {
                    if (let Some(msgVal) <- errArray[idx].asObject().get("message")) {
                        return msgVal.asString().getValue()
                    }
                }
            }
        } catch (e: Exception) {
            eprintln("Warning: failed to parse response: ${e.message}")
        }
        return respMsg
    }

    // get index file
    public func getIndex(name: String, org: String): String {
        let firstLevel = name[..2]
        let secondLevel = if (name.size > 3) { name[2..4] } else { name[2..3] }
        let indexPath = Path(this.localRepo).join("index").
            join(if (org.isEmpty()) { "default" } else { org }).
            join(firstLevel).
            join(secondLevel).
            join(name).toString()
        if (fileExists(indexPath)) {
            return indexPath
        }
        return ""
    }

    // Separate registries even when name/version coincide. This cache contains
    // the original archive, not a digest of an extracted (possibly modified) tree.
    public func artifactCachePath(name: String, org: String, version: String): Path {
        let source = normalizeLockSourceUrl(this.baseUrl, registry: true)
        let sourceKey = artifactChecksum(source.toArray()).removePrefix("sha256:")
        return Path(this.localRepo).join("artifacts").join(sourceKey).
            join(if (org.isEmpty()) { "default" } else { org }).join("${name}-${version}.cjp")
    }

    // Used by v1 resolution before consuming source files. Legacy source-only
    // caches are insufficient evidence: fetch the archive if it is absent.
    public func loadVerifiedArtifact(name: String, org: String, version: String,
        indexChecksum: String, locked!: ?LockEntry = None): (Bool, String) {
        try {
            let source = "registry+${normalizeLockSourceUrl(this.baseUrl, registry: true)}"
            let expected = registryLockChecksum(indexChecksum)
            checkRegistryLock(source, version, expected, locked)
            let archive = artifactCachePath(name, org, version)
            if (!fileExists(archive.toString())) {
                return this.download(name, org, version, expectedChecksum: Some(expected))
            }
            // Hash the exact bytes that will be extracted, avoiding a second read
            // between validation and consumption of the cached artifact.
            let bytes = File.readFrom(archive)
            if (artifactChecksum(bytes) != expected) {
                return (false, "cached artifact checksum mismatch, please try 'cjpm update'")
            }
            return uncompressCjp(org, name, version, bytes, this.localRepo, replaceSource: true)
        } catch (_: Exception) {
            return (false, "cannot verify registry artifact or source, please try 'cjpm update'")
        }
    }
}

public func registryLockChecksum(indexChecksum: String): String {
    let checksum = indexChecksum.toAsciiLower()
    if (!Regex("^[0-9a-f]{64}$").matches(checksum)) {
        throw Exception("invalid registry checksum")
    }
    return "sha256:${checksum}"
}

func artifactChecksum(bytes: Array<Byte>): String {
    let digest = SHA256()
    digest.write(bytes)
    return "sha256:${toHexString(digest.finish())}"
}

// Never replace a locked checksum with a new index value. A different selected
// version is a new artifact; command integration decides which lock is retained.
public func checkRegistryLock(source: String, version: String, checksum: String,
    locked: ?LockEntry): Unit {
    if (let Some(previous) <- locked) {
        if (previous.version != Some(version) || previous.source != Some(source) ||
            previous.checksum != Some(checksum)) {
            throw Exception("registry metadata differs from lock file, please try 'cjpm update'")
        }
    }
}

func saveVerifiedArtifact(depot: Depot, name: String, org: String, version: String, bytes: Array<Byte>): Unit {
    let archive = depot.artifactCachePath(name, org, version)
    // Several downloads share this parent. Use the synchronized helper so
    // existing directories and concurrent creation are both handled safely.
    if (!createDirectory(archive.parent.toString())) {
        throw Exception("failed to create artifact cache directory")
    }
    let temp = File.createTemp(archive.parent)
    let tempPath = temp.info.path
    try {
        temp.write(bytes)
        temp.close()
        rename(tempPath, to: archive, overwrite: true)
    } finally {
        temp.close()
        if (fileExists(tempPath.toString())) { remove(tempPath) }
    }
}
