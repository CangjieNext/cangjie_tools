// Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.command

import cjpm.config.*
import cjpm.implement.*

let OHOS_BUNDLE_USAGE: String = """
Build and LTO-merge OHOS app/system shared libraries from bitcode artifacts.

This command implements a two-pass LTO compilation strategy:
  Pass 1  cjc --compile-as-exe --output-type=dylib --lto=full
          Merge all app .bc files → entry.so (using placeholder interop.so)
  Pass 2  cjc --output-type=dylib --lto=full  + version script
          LTO-merge system .bc/.a files → new ark_interop.so

Usage:
  cjpm ohos-bundle [options]

Available options:
  -h, --help                    help for ohos-bundle
  -V, --verbose                 enable verbose output
  --lib-dir <path>              system-side .so directory            (default: ./lib)
  --app-dir <path>              app-side .so directory               (default: ./app)
  --app-bc-dir <path>           app-side .bc files from cjpm --lto   (default: ./modules/app)
  --std-bc-dir <path>           SDK std .bc dir (build-tools/.../std) (default: ./modules/std)
  --ohos-a-dir <path>           SDK ohos .a dir (api/lib/.../ohos)   (default: ./modules/ohos)
  --ohos-dir <path>             on-device OHOS system libs            (default: ./ohos)
  --sysroot <path>              cross-compile sysroot path
  --target <triple>             target triple (auto-detected from interop.so if omitted)
  --interop-so <name>           interop library name                  (default: libohos.ark_interop.so)
  --output-app <path>           output for app artifacts              (default: ./output_app)
  --output-system <path>        output for system artifacts           (default: ./output_system)"""

// Options that take a value argument must carry a trailing '=' so that ArgOpt
// knows to consume the next token as the option's value.
let OHOS_BUNDLE_LONG_OPTIONS: Array<String> = [
    "help", "verbose",
    "lib-dir=", "app-dir=", "app-bc-dir=", "std-bc-dir=", "ohos-a-dir=", "ohos-dir=",
    "sysroot=", "target=", "interop-so=",
    "output-app=", "output-system="
]

class OhosBundleCommand <: Handle {
    init(args: Array<String>) {
        this.cmdName = args[0]
        this.arguments = args[1..]
    }

    public override func printHelp(): Unit {
        println(OHOS_BUNDLE_USAGE)
    }

    public override func handleCommand(): Bool {
        if (hasHelpCommand(this)) {
            return true
        }

        let (argMap, unparsedArgs) = getParseOption(this.arguments, "V", OHOS_BUNDLE_LONG_OPTIONS)

        if (unparsedArgs.size != 0) {
            eprintln("Error: unknown argument '${unparsedArgs[0]}' for 'cjpm ohos-bundle'")
            eprintln("       run 'cjpm ohos-bundle --help' for usage")
            return false
        }

        let cfg = OhosBundleConfig()
        cfg.isVerbose = getBoolOption(argMap, "-V", "--verbose")

        // Apply any user-supplied paths; keep the defaults for anything omitted.
        let (libDir, hasLibDir)           = getArgValue(argMap, "--lib-dir")
        let (appDir, hasAppDir)           = getArgValue(argMap, "--app-dir")
        let (appBcDir, hasAppBcDir)       = getArgValue(argMap, "--app-bc-dir")
        let (stdBcDir, hasStdBcDir)       = getArgValue(argMap, "--std-bc-dir")
        let (ohosADir, hasOhosADir)       = getArgValue(argMap, "--ohos-a-dir")
        let (ohosDir, hasOhosDir)         = getArgValue(argMap, "--ohos-dir")
        let (sysroot, hasSysroot)         = getArgValue(argMap, "--sysroot")
        let (target, hasTarget)           = getArgValue(argMap, "--target")
        let (interopSo, hasInteropSo)     = getArgValue(argMap, "--interop-so")
        let (outApp, hasOutApp)           = getArgValue(argMap, "--output-app")
        let (outSys, hasOutSys)           = getArgValue(argMap, "--output-system")

        if (hasLibDir)     { cfg.libDir      = libDir }
        if (hasAppDir)     { cfg.appDir      = appDir }
        if (hasAppBcDir)   { cfg.appBcDir    = appBcDir }
        if (hasStdBcDir)   { cfg.stdBcDir    = stdBcDir }
        if (hasOhosADir)   { cfg.ohosADir    = ohosADir }
        if (hasOhosDir)    { cfg.ohosDir     = ohosDir }
        if (hasSysroot)    { cfg.sysroot     = sysroot }
        if (hasTarget)     { cfg.target      = target }
        if (hasInteropSo)  { cfg.interopSo   = interopSo }
        if (hasOutApp)     { cfg.outputApp   = outApp }
        if (hasOutSys)     { cfg.outputSystem = outSys }

        // Basic sanity: sysroot is required when target is specified explicitly.
        if (hasTarget && !hasSysroot && cfg.sysroot.isEmpty()) {
            eprintln("Warning: --target was specified but --sysroot was not; the linker may fail to locate C runtime objects")
        }

        return doOhosBundle(cfg)
    }
}
