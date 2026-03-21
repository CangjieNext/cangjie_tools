// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.implement
import cjpm.config.*

func supportLto(): Bool {
    let isCrossCompile = !crossCompileTarget.isEmpty()
    let compileTarget = if (isCrossCompile) { crossCompileTarget } else { targetConfigName }
    let isLinux = compileTarget.contains("linux") && !compileTarget.contains("android")
    let isAndroid = isCrossCompile && compileTarget.contains("android")
    let isDarwin = compileTarget.contains("darwin")
    let isIos = isCrossCompile && compileTarget.contains("ios")
    return isLinux || isAndroid || isDarwin || isIos
}

func isSupportCombine(): Bool {
    let isCrossCompile = !crossCompileTarget.isEmpty()
    let compileTarget = if (isCrossCompile) {
            crossCompileTarget
        } else {
            targetConfigName
        }
    let isTargetLinux = compileTarget.contains("linux")
    let isTargetAndroid = compileTarget.contains("android")
    let isTargetWindows = compileTarget.contains("windows") || compileTarget.contains("w64")
    let isTargetDarwin = compileTarget.contains("darwin")
    let isTargetIos = isCrossCompile && compileTarget.contains("ios")
    // combine-all-deps is supported on Linux, Android cross targets, Windows, macOS, and iOS cross targets.
    return (isTargetLinux || isTargetWindows || isTargetDarwin || isTargetIos || (isCrossCompile && isTargetAndroid))
}
