// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package ohos_app_cangjie_entry.IntroduceParameter

func compoundAssignSampleCase011(a: Int64): Unit {
    a += 2
    a -= 2
    a <<= 2
    a ^= 2
}

func callCompoundAssignSampleCase011(): Unit {
    var value: Int64 = 10
    compoundAssignSampleCase011(value)
}
