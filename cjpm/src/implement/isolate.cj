// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement

public func supportLtoForTarget(compileTarget: String): Bool {
    compileTarget.contains("linux") || compileTarget.contains("apple-ios")
}

func supportLto(): Bool {
    return supportLtoForTarget(getCompileTarget())
}

func isSupportCombine(): Bool {
    let compileTarget = getCompileTarget()
    let isTargetLinux = compileTarget.contains("linux")
    let isTargetAndroid = compileTarget.contains("android")
    let isTargetWindows = compileTarget.contains("windows") || compileTarget.contains("w64")
    return (isTargetLinux || isTargetWindows)
}