// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package ohos_app_cangjie_entry.IntroduceField

type CompareFunction = (Int64, Int64) -> Int64

func companame(compare: CompareFunction) {
    let num: CompareFunction = {a: Int64, b: Int64 => a - b}
}
