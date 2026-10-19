// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.fs.*
import std.regex.*
import std.env.*
import cjpm.config.*

const WINDOWS_LOCAL_FILE_REGEX = "^[A-Za-z]:.+$"
const URL_WITH_SPACE = "^.*\\s+.*$"

public class GitCli {
    func getRemoteRef(url: String, ref: String): ?String {
        setVariable("GIT_TERMINAL_PROMPT", "0")
        setVariable("GIT_SSH_COMMAND", "ssh -o ConnectTimeout=5 -o BatchMode=yes -o StrictHostKeyChecking=ask")

        let (success, outInfo, errInfo) = execWithOutput("git", ArrayList<String>(["ls-remote", url, ref]),
            timeout: Duration.second * 30)
        if (!success) {
            eprintln("Error invoking git:")
            eprintln(errInfo + outInfo)
            return None
        }

        // A bit weird regex used here because Cangjie regex engine currently handles match group indexes incorrectly
        let gitRefsParser = Regex("^([0-9a-f]+)\\s+(?:refs\\/(?:heads|tags)\\/)?([^\\s]+)$", MultiLine)
        let matchDataArray = gitRefsParser.findAll(outInfo, group: true)

        for (next in matchDataArray) {
            if (next.matchString(2) == ref) {
                return next.matchString(1)
            }
        }

        return None
    }

    public func clone(url: String, targetDir: String, hash: String): Bool {
        setVariable("GIT_TERMINAL_PROMPT", "0")
        setVariable("GIT_DIR", Path(targetDir).join(".git").toString())
        setVariable("GIT_WORK_TREE", targetDir)

        // same as `git clone` but allows to fetch by commit hash
        let error = execAndGetError("init") ?? execAndGetError("remote add --no-tags origin ${url}") ??
            execAndGetError("fetch --depth=1 origin ${hash}") ?? execAndGetError("checkout --force ${hash}") ??
            return true

        eprintln("Error invoking git:")
        eprintln(error)
        return false
    }

    // returns Some if unsuccessfull
    func execAndGetError(command: String): ?String {
        let arguments = extractOptionByString(command)
        let (success, outInfo, errInfo) = execWithOutput("git", arguments)
        if (success) {
            return Option.None
        } else {
            return errInfo + outInfo
        }
    }

    func getCommitHashFromName(url: String, name: ?String): ?String {
        let target = name ?? "HEAD"
        let hash = getRemoteRef(url, target)
        if (hash.isNone()) {
            eprintln("Error: reference '${target}' not found in git repository with url: ${url}")
        }
        return hash
    }

    public func isAvailable(): Bool {
        let (success, _, _) = execWithOutput(WHICH, ArrayList<String>(["git"]))
        return success
    }
}

func getGitStoragePath(): Path {
    return Path(FINAL_CJPM_CONFIG_INFO.repository[REPO_CACHE_NAME].path).join("git")
}

public func downloadGitDep(name: String, v: DepInfo): ?(String, String, Bool) {
    let gitStoragePath = getGitStoragePath()
    let git = GitCli()

    if (!git.isAvailable()) {
        eprintln("Error: git is not available. Please install it or make it available through $PATH.")
        return None
    }

    let url = v.git ?? return None
    try {
        normalizeLockSourceUrl(url)
    } catch (_: Exception) {
        eprintln("Error: Git source cannot be safely recorded; remove credentials and query parameters from its URL")
        return None
    }
    let targetCommit = v.commitId ?? git.getCommitHashFromName(url, v.branch ?? v.tag) ?? return None
    let dependencyPath = gitStoragePath.join(swapOrgName(name)).join(targetCommit).toString()
    if (directoryExists(dependencyPath)) {
        let commit = resolvedGitCommit(dependencyPath) ?? return None
        if (v.commitId.isSome() && Regex("^([0-9a-f]{40}|[0-9a-f]{64})$").matches(targetCommit) &&
            commit != targetCommit) {
            eprintln("Error: cached Git commit differs from locked revision, please try 'cjpm update'")
            return None
        }
        return (dependencyPath, commit, false)
    } else {
        try {
            Directory.create(dependencyPath, recursive: true)
        } catch (e: FSException) {
            eprintln("Error while working with cjpm shared directory(`${gitStoragePath}`): ${e.message}")
        }

        let result = git.clone(url, dependencyPath, targetCommit)
        if (result) {
            let commit = resolvedGitCommit(dependencyPath) ?? return None
            return checkGitWorkspace(url, dependencyPath, commit, gitStoragePath)
        }

        deleteDirectory(dependencyPath)
        return None
    }
}

// Resolve abbreviated revisions and annotated tags to the checked-out commit.
// Explicit paths avoid inheriting GIT_DIR from another dependency's clone.
func resolvedGitCommit(path: String): ?String {
    let (ok, output, _) = execWithOutput("git", ArrayList<String>([
        "--git-dir", Path(path).join(".git").toString(), "--work-tree", path,
        "rev-parse", "--verify", "HEAD^{commit}"
    ]))
    let commit = output.trimAscii()
    if (!ok || !Regex("^([0-9a-f]{40}|[0-9a-f]{64})$").matches(commit)) {
        eprintln("Error: cannot resolve full Git commit for dependency")
        return None
    }
    return Some(commit)
}
