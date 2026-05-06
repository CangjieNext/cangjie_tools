// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie Project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import std.io.*
import std.fs.*
import std.env.*

import cjpm.config.*

import stdx.encoding.json.stream.*

private const JSON_PLAN_VERSION = 1

class JsonPlanSerializer <: PlanSerializer {
    private let writer: JsonWriter
    private let targetDir: String
    private let testTargetDir: String
    private let modulePath: String
    private let ldPath: HashSet<String>
    private let taskIdToIndex: HashMap<TaskId, Int64>
    private var currentIndex: Int64 = 0
    private var isFirstTask: Bool = true

    init(buildConfig: BuildConfig, out: OutputStream) {
        this.writer = JsonWriter(out)
        this.targetDir = buildConfig.globalConfig.targetDir
        this.testTargetDir =  Path(buildConfig.globalConfig.targetDir).join(UNITTEST_BIN).toString()
        this.modulePath = getCanonicalPath(buildConfig.globalConfig.modulePath)
        this.ldPath = buildConfig.globalConfig.ldPath
        this.taskIdToIndex = HashMap<TaskId, Int64>()
        this.currentIndex = 0
        this.isFirstTask = true
    }

    public func writeHeader(): Unit {
        writer.startObject()
        writeVersion()
        writeGlobal()
        writer.writeName("tasks").startArray()
    }

    public func writeCompileTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTask(invocation, task, "compile")
    }

    public func writeLinkTask(invocation: CompilerInvocation, task: CompileTask): Unit {
        writeTask(invocation, task, "link")
    }

    private func writeTask(invocation: CompilerInvocation, task: CompileTask, taskType: String): Unit {
        let taskIndex = currentIndex
        taskIdToIndex[task.id] = taskIndex
        currentIndex += 1

        writer.startObject()
        writer.writeName("id").writeValue(taskIndex)
        writer.writeName("type").writeValue(taskType)
        writeMetadata(invocation.metadata)
        writeCommand(invocation)
        writeDependencies(task.requireTasks)
        writer.writeName("artifacts").writeValue(invocation.metadata.artifacts)
        writer.endObject() // task
    }

    private func writeVersion(): Unit {
        writer.writeName("version").writeValue(JSON_PLAN_VERSION)
    }

    private func writeGlobal(): Unit {
        writer.writeName("global").startObject()
        writer.writeName("targetDir").writeValue(targetDir)
        writer.writeName("testTargetDir").writeValue(testTargetDir)
        writer.writeName("modulePath").writeValue(modulePath)
        writeEnv()
        writer.endObject() // global
    }

    private func writeEnv(): Unit {
        writer.writeName("env").startObject()
        if (!ldPath.isEmpty()) {
            writer.writeName(LD_PATH).writeValue(ldPath.toArray())
        }
        writer.endObject() // env
    }

    private func writeMetadata(metadata: CompilerInvocationMetadata): Unit {
        writer.writeName("metadata").startObject()
        writer.writeName("fullName").writeValue(metadata.fullName)
        writer.writeName("target").writeValue(metadata.target)
        writer.writeName("debug").writeValue(metadata.debug)
        writer.writeName("mock").writeValue(metadata.mock)
        writer.writeName("outputType").writeValue(metadata.outputType.toString())
        writer.writeName("sourceSetDir").writeValue(metadata.sourceSetDir)
        writer.writeName("features").writeValue(metadata.features)
        writer.endObject() // metadata
    }

    private func writeCommand(invocation: CompilerInvocation): Unit {
        writer.writeName("command").startObject()
        writer.writeName("pkgPath").writeValue(invocation.pkgPath)
        writer.writeName("importPaths").writeValue(invocation.importPaths)
        writer.writeName("output").writeValue(invocation.output)

        writer.writeName("env").startObject()
        for ((k, v) in invocation.envBuilder.asCliStrings(getVariables())) {
            writer.writeName(k).writeValue(v)
        }
        writer.endObject() // env

        writer.writeName("compiler").writeValue("cjc")
        writeArgs(invocation)
        writer.endObject() // command
    }

    private func writeArgs(invocation: CompilerInvocation): Unit {
        writer.writeName("args").startArray()

        if (let Some(pkg) <- invocation.pkgPath) {
            writer.writeValue("-p")
            writer.writeValue(pkg)
        }
        if (let Some(obj) <- invocation.objPath) {
            writer.writeValue(obj)
        }
        for (p in invocation.importPaths) {
            writer.writeValue("--import-path=${p}")
        }
        for (f in invocation.cjcFlags) {
            writer.writeValue(f)
        }
        for (f in invocation.linkArgs) {
            writer.writeValue(f)
        }
        for (f in invocation.customOptions) {
            writer.writeValue(f)
        }
        for (f in invocation.linkOptions) {
            writer.writeValue(f)
        }
        for (f in invocation.outputConfigFlags) {
            writer.writeValue(f)
        }
        if (let Some(out) <- invocation.output) {
            writer.writeValue("-o=${out}")
        }

        writer.endArray()
    }

    private func writeDependencies(deps: HashSet<TaskId>): Unit {
        writer.writeName("dependencies").writeValue(resolveDependencies(deps))
    }

    private func resolveDependencies(deps: HashSet<TaskId>): Array<Int64> {
        let result = ArrayList<Int64>()
        for (depId in deps) {
            if (let Some(idx) <- taskIdToIndex.get(depId)) {
                result.add(idx)
            }
        }
        result.toArray()
    }

    public func writeFooter(): Unit {
        writer.endArray() // tasks
        writer.endObject() // root
        writer.flush()
    }
}
