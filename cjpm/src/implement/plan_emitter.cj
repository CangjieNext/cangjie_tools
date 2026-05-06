// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

import std.collection.*
import cjpm.config.*

interface PlanSerializer {
    func writeHeader(): Unit
    func writeCompileTask(invocation: CompilerInvocation, task: CompileTask): Unit
    func writeLinkTask(invocation: CompilerInvocation, task: CompileTask): Unit
    func writeFooter(): Unit
}

class PlanEmitter {
    public static func emit(
        buildConfig: BuildConfig,
        engine: IncrementalEngine,
        serializer: PlanSerializer,
    ): Bool {
        serializer.writeHeader()

        let graph = engine.taskContext.taskGraphBuilder.finish()

        for (taskId in graph.toposort) {
            if (let Some(task) <- engine.taskContext.compileTasks.get(taskId)) {
                let invocation = constructCompilerInvocation(
                    task, buildConfig, buildConfig.globalConfig.ldPath
                )
                match (task.id) {
                    case Compile(_, _, _, _, _, _) =>
                        serializer.writeCompileTask(invocation, task)
                    case Link(_, _, _, _, _, _) =>
                        serializer.writeLinkTask(invocation, task)
                    case _ => ()
                }
            }
        }

        serializer.writeFooter()
        return true
    }
}
