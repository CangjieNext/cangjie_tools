// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package unusedDefaultParams

public func localLambda(id: String, name!: String = "aaa") {
    let joined = id + name
    let append = {value: String => joined + value}
    println(append("!"))
    let ignored = 1
}
