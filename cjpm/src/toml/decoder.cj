// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.toml

import std.io.*

public class Decoder {
    let data: Array<Byte>

    // Keep the stream-based API for compatibility with existing callers.
    public init(r: InputStream) {
        let buffer = ByteBuffer()
        copy(r, to: buffer)
        this.data = buffer.bytes()
    }

    // File-based callers should use this overload with the exact bytes returned by File.readFrom.
    public init(data: Array<Byte>) {
        this.data = data
    }

    public func decode(): TomlObject {
        let p = parse(data)
        return p.mapping
    }
}
