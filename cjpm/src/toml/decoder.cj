// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.toml

import std.io.*

public class Decoder {
    let data: Array<Byte>

    public init(r: InputStream) {
        // Preserve the stream-based API while keeping the parser's input representation as bytes.
        let buffer = ByteBuffer()
        copy(r, to: buffer)
        this.data = buffer.bytes()
    }

    public init(data: Array<Byte>) {
        // Callers that already have bytes can avoid StringReader and String.toArray conversions.
        this.data = data
    }

    public func decode(): TomlObject {
        let p = parse(data)
        return p.mapping
    }
}
