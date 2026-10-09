// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package unusedDefaultParamsMacro

import unusedDefaultParamsMacro.macroPkg.*

@PassThrough
public func macroDefaultUsed(id: String, name!: String = "aaa") {
    let joined = id + name
    println(joined)
}

main(): Int64 {
    macroDefaultUsed("id")
    return 0
}
