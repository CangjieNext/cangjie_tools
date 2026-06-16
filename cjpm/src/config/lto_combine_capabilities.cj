// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.config

// Platform capability checks and user-facing platform strings for LTO / combine features.
// This is the single source of truth — warning messages should reference constants here.

// Platform descriptions used in user-facing warnings/errors.
public let LTO_SUPPORTED_PLATFORMS: String = "Linux/OpenHarmony/Android/iOS"
public let MODULE_COMBINE_SUPPORTED_PLATFORMS: String = "Linux/OpenHarmony/Android/Windows"

// Whether the compile target supports LTO (profile.build.lto).
public func supportLtoForTarget(compileTarget: String): Bool {
    compileTarget.contains("linux") || compileTarget.contains("apple-ios")
}

// Whether the compile target supports profile.build.combined (module-level merge).
public func supportModuleCombineForTarget(compileTarget: String): Bool {
    let isTargetLinux = compileTarget.contains("linux")
    let isTargetWindows = compileTarget.contains("windows") || compileTarget.contains("w64")
    return isTargetLinux || isTargetWindows
}

// Whether the current host/cross target supports LTO.
public func supportLtoForCurrentTarget(): Bool {
    supportLtoForTarget(getCompileTarget())
}

// Whether the current host/cross target supports module-level combine.
public func supportModuleCombineForCurrentTarget(): Bool {
    supportModuleCombineForTarget(getCompileTarget())
}

public func isIosTarget(compileTarget: String): Bool {
    compileTarget.contains("apple-ios")
}

// iOS project lto-combine uses a dedicated link path; treat module combine as supported
// even when supportModuleCombineForTarget is false for the current target.
public func supportModuleCombine(compileTarget: String, isProjectCombine: Bool): Bool {
    if (isIosTarget(compileTarget) && isProjectCombine) {
        return true
    }
    return supportModuleCombineForTarget(compileTarget)
}

// True when linking the iOS project lto-combine root (bitcode-only, no native fallback).
public func isIosProjectCombineRoot(isProjectCombined: Bool, compileTarget: String): Bool {
    isProjectCombined && isIosTarget(compileTarget)
}
