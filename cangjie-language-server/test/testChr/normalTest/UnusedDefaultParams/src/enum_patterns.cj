// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package unusedDefaultParams

public enum DefaultVariant {
    | Used
    | Pattern(String)
    | Unused
}

public func describeVariant(variant: DefaultVariant) {
    match (variant) {
        case DefaultVariant.Used => println("used")
        case DefaultVariant.Pattern(value) => println(value)
        case _ => ()
    }
}

public func enumPatterns(id: String, name!: String = "aaa") {
    let direct = DefaultVariant.Used
    describeVariant(direct)
    let payload = DefaultVariant.Pattern(id + name)
    describeVariant(payload)
}
