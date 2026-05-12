// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.util

import std.collection.*
import std.fs.*
import std.io.*

public func ninjaEscape(string: String): String {
    string.replace("$", "$$")
}

public func escapePath(word: String): String {
    word.replace("$ ", "$$ ")
       .replace(" ", "$ ")
       .replace(":", "$:")
}

public class NinjaWriter {
    static const DEFAULT_BUFFER_SIZE: Int64 = 4096
    static const FLUSH_THRESHOLD: Int64 = 3500

    var out: OutputStream
    var buffer = Array<UInt8>(DEFAULT_BUFFER_SIZE, repeat: 0)
    var curPos = 0

    public init(out!: OutputStream) {
        this.out = out
    }

    public func newline(): Unit {
        writeChar(b'\n')
    }

    public func comment(text: String): Unit {
        writeString("# ")
        writeString(text)
        writeChar(b'\n')
    }

    public func variable(key: String, value: String, indent!: Int64 = 0): Unit {
        if (value.isEmpty()) {
            return
        }
        let spacing = "  " * indent
        writeString(spacing)
        writeString(key)
        writeString(" = ")
        writeString(value)
        writeChar(b'\n')
    }

    public func rule(
        name: String,
        command: String,
        description!: Option<String> = None,
        restat!: Bool = false,
    ): Unit {
        writeString("rule ")
        writeString(name)
        writeChar(b'\n')
        variable("command", command, indent: 1)
        variable("description", description.getOrDefault { "" }, indent: 1)
        if (restat) {
            variable("restat", "1", indent: 1)
        }
    }

    public func build(
        outputs: Array<String>,
        rule: String,
        inputs!: Option<Array<String>> = None,
        implicit!: Option<Array<String>> = None,
        variables!: Option<HashMap<String, String>> = None,
    ): Unit {
        let escapedOutputs = outputs.map { x => escapePath(x) }
        var allInputs = ArrayList<String>()
        
        if (let Some(inp) <- inputs) {
            for (item in inp) {
                allInputs.add(escapePath(item))
            }
        }

        if (let Some(impl) <- implicit && !impl.isEmpty()) {
            allInputs.add("|")
            for (item in impl) {
                allInputs.add(escapePath(item))
            }
        }

        let inputsStr = if (allInputs.isEmpty()) {
            ""
        } else {
            " ${allInputs |> collectString(delimiter: " ")}"
        }

        writeString("build ")
        writeString(escapedOutputs |> collectString(delimiter: " "))
        writeString(": ")
        writeString(rule)
        writeString(inputsStr)
        writeChar(b'\n')

        if (let Some(vars) <- variables) {
            for ((k, v) in vars) {
                variable(k, v, indent: 1)
            }
        }
    }

    public func defaultTargets(paths: Array<String>): Unit {
        writeString("default ")
        writeString(paths |> collectString(delimiter: " "))
        writeChar(b'\n')
    }

    public func phony(name: String, dependencies: Array<String>): Unit {
        writeString("build ")
        writeString(escapePath(name))
        writeString(": phony ")
        writeString(dependencies.map { it => escapePath(it) } |> collectString(delimiter: " "))
        writeChar(b'\n')
    }

    public func flush(): Unit {
        if (curPos > 0) {
            out.write(buffer[0..curPos])
            curPos = 0
        }
    }

    private func writeString(s: String): Unit {
        unsafe {
            let bytes = s.rawData()
            let size = bytes.size
            if (size > FLUSH_THRESHOLD) {
                flush()
                out.write(bytes)
            } else {
                if (curPos + size > DEFAULT_BUFFER_SIZE) {
                    flush()
                }
                bytes.copyTo(buffer, 0, curPos, size)
                curPos += size
            }
        }
    }

    private func writeChar(c: UInt8): Unit {
        if (curPos + 1 > DEFAULT_BUFFER_SIZE) {
            flush()
        }
        buffer[curPos] = c
        curPos += 1
    }
}
