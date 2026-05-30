#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie Project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

import os
import subprocess
import sys
import shutil
import platform
import argparse
import json
import time

CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))
BUILD_TIME_LOG = os.path.join(CURRENT_DIR, "logs", "build_time.log")

def format_duration(seconds):
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = int(seconds % 60)
    if hours > 0:
        return "{}h {}m {}s".format(hours, minutes, secs)
    if minutes > 0:
        return "{}m {}s".format(minutes, secs)
    return "{}s".format(secs)

def log_build_time(command, build_type, version, target, product, elapsed_seconds, time_log_path):
    if not os.path.exists(os.path.dirname(time_log_path)):
        os.makedirs(os.path.dirname(time_log_path), exist_ok=True)
    duration_str = format_duration(elapsed_seconds)
    print("[{}] build_type={} version={} target={} product={} elapsed={}".format(
        command, build_type, version, target, product, duration_str))
    entry = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "command": command,
        "build_type": build_type,
        "version": version,
        "target": target or "native",
        "product": product or "all",
        "elapsed_seconds": round(elapsed_seconds, 2),
        "elapsed_human": duration_str,
    }
    with open(time_log_path, "a") as f:
        f.write(json.dumps(entry) + "\n")

# Check command
def check_call(command):
    try:
        return subprocess.check_call(command, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Command '{e.cmd}' returned non-zero exit status {e.returncode}.")
        return e.returncode

# Build cjcov
def build(build_type, target, rpath=None, time_log=None):
    start_time = time.time()
    if not build_type:
        build_type = ""
    if not target:
        target = "native"

    # Check CANGJIE_HOME
    if not os.environ.get("CANGJIE_HOME"):
        print("error: cannot find CANGJIE_HOME, please make sure cangjie sdk is configured.", file=sys.stderr)
        return 1

    # Check stdx lib
    if not os.environ.get("CANGJIE_STDX_PATH"):
        print("error: cannot find CANGJIE_STDX_PATH, please make sure stdx lib is configured.", file=sys.stderr)
        return 1

    # Check if cross compile is supported
    IS_WINDOWS = platform.system() == "Windows"
    IS_LINUX = platform.system() == "Linux"
    IS_MACOS = platform.system() == "Darwin"
    IS_CROSS_WINDOWS = False

    if target != "native" and not IS_LINUX:
        print("error: cross compile is only supported from Linux to windows-x86_64.", file=sys.stderr)
        return 1
    if target == "windows-x86_64" and IS_LINUX:
        IS_CROSS_WINDOWS = True
        IS_LINUX = False

    # Set rpath
    RPATH_SET_OPTION = ""#= "\$ORIGIN/../../runtime/lib/linux_$(arch)_cjnative"
    if rpath:
        if IS_MACOS:
            RPATH_SET_OPTION = f"--link-options=\"-rpath {rpath}\""
        elif IS_LINUX:
            RPATH_SET_OPTION = f"--link-options=\"--disable-new-dtags -rpath={rpath}\""

    # Set common compile option
    DEBUG_MODE = ""
    if build_type == "debug":
        DEBUG_MODE = "-g"
    elif build_type != "release":
        print("error: cjcov only support 'release' and 'debug' mode of compiling.")
        return 1
    # Get cjc executable file
    if IS_WINDOWS:
        CJC = "cjc.exe"
    else:
        CJC = "cjc"

    # Create output directories
    os.makedirs(os.path.join(CURRENT_DIR, 'bin', 'cjcov'), exist_ok=True)
    os.makedirs(os.path.join(CURRENT_DIR, '..', 'dist'), exist_ok=True)

    # Compile static libs of sub-packages
    if IS_WINDOWS:
        COMMON_OPTION = f"-j1 --trimpath={CURRENT_DIR} {DEBUG_MODE} --import-path {os.path.join(CURRENT_DIR,'bin')} --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} --output-type=staticlib"
    else:
        COMMON_OPTION = f"-j1 --trimpath={CURRENT_DIR} {DEBUG_MODE} --import-path {os.path.join(CURRENT_DIR,'bin')} --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} --output-type=staticlib"

    if IS_LINUX or IS_MACOS:
        print(f"{CJC} {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/util')} -o libcjcov.util.a")
        returncode1 = check_call(f"{CJC} {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/util')} -o libcjcov.util.a")
        returncode2 = check_call(f"{CJC} {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/core')} --import-path {os.environ['CANGJIE_STDX_PATH']} -o libcjcov.core.a")
    if IS_WINDOWS:
        returncode1 = check_call(f"{CJC} {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/util')} -o libcjcov.util.a")
        returncode2 = check_call(f"{CJC} {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/core')} --import-path {os.environ['CANGJIE_STDX_PATH']} -o libcjcov.core.a")
    if IS_CROSS_WINDOWS:
        returncode1 = check_call(f"{CJC} --target=x86_64-windows-gnu {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/util')} -o libcjcov.util.a")
        returncode2 = check_call(f"{CJC} --target=x86_64-windows-gnu {COMMON_OPTION} -p {os.path.join(CURRENT_DIR, '..', 'src/core')} --import-path {os.environ['CANGJIE_STDX_PATH']} -o libcjcov.core.a")
    if returncode1 != 0:
        return returncode1
    if returncode2 != 0:
        return returncode2

    # Compile cjcov executable file
    COMMON_OPTION2 = f"-j1 --trimpath={CURRENT_DIR} {DEBUG_MODE} --import-path {os.path.join(CURRENT_DIR,'bin')} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjcov')}"
    if IS_WINDOWS:
        STDX_LINKS=f"-L %CANGJIE_STDX_PATH% -l:libstdx.logger.a -l:libstdx.log.a -l:libstdx.encoding.json.stream.a -l:libstdx.serialization.serialization.a -l:libstdx.encoding.json.a"
    if IS_MACOS:
        STDX_LINKS=f"$CANGJIE_STDX_PATH/libstdx.logger.a $CANGJIE_STDX_PATH/libstdx.log.a $CANGJIE_STDX_PATH/libstdx.encoding.json.stream.a $CANGJIE_STDX_PATH/libstdx.serialization.serialization.a $CANGJIE_STDX_PATH/libstdx.encoding.json.a"
    else:
        STDX_LINKS=f"-L $CANGJIE_STDX_PATH -l:libstdx.logger.a -l:libstdx.log.a -l:libstdx.encoding.json.stream.a -l:libstdx.serialization.serialization.a -l:libstdx.encoding.json.a"
    
    if IS_LINUX:
        returncode = check_call(f"{CJC} {COMMON_OPTION2} {RPATH_SET_OPTION} \"--link-options=-z noexecstack -z relro -z now -s\" -L {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} -L bin/cjcov -lcjcov.util -lcjcov.core {STDX_LINKS} -p {os.path.join(CURRENT_DIR, '..', 'src')}  -o cjcov")
    if IS_MACOS:
        returncode = check_call(f"{CJC} {COMMON_OPTION2} {RPATH_SET_OPTION} --import-path {os.environ['CANGJIE_STDX_PATH']} -L {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} -L bin/cjcov -lcjcov.util -lcjcov.core  {STDX_LINKS} -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 -o cjcov")
    if IS_CROSS_WINDOWS:
        returncode = check_call(f"{CJC} --target=x86_64-windows-gnu {COMMON_OPTION2} --link-options=--no-insert-timestamp -L {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} -L bin/cjcov -lcjcov.util -lcjcov.core {STDX_LINKS} -p {os.path.join(CURRENT_DIR, '..', 'src')} -o cjcov.exe")
    if IS_WINDOWS:
        returncode = check_call(f"{CJC} {COMMON_OPTION2}  --link-options=--no-insert-timestamp -L {os.path.join(CURRENT_DIR, 'bin', 'cjcov')} -lcjcov.util -lcjcov.core {STDX_LINKS} -p {os.path.join(CURRENT_DIR, '..', 'src')} -o cjcov.exe")

    if returncode != 0:
        return returncode

    if IS_WINDOWS or IS_CROSS_WINDOWS:
        shutil.copy(os.path.join(CURRENT_DIR, 'bin', 'cjcov', 'cjcov.exe'), os.path.join(CURRENT_DIR, '..', 'dist'))
    else:
        shutil.copy(os.path.join(CURRENT_DIR, 'bin', 'cjcov', 'cjcov'), os.path.join(CURRENT_DIR, '..', 'dist'))

    elapsed = time.time() - start_time
    log_build_time("build", build_type, "", target, "", elapsed, time_log or BUILD_TIME_LOG)
    print("Successfully build cjcov (elapsed {})!".format(format_duration(elapsed)))
    return 0

# Install cjcov
def install(prefix, time_log=None):
    start_time = time.time()
    if not os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov')) \
        and not os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov.exe')):
        print("error: no cjcov output, please run command 'build' first.")
        return 1

    if prefix:
        os.makedirs(os.path.abspath(prefix), exist_ok=True)
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov'), os.path.abspath(prefix))
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov.exe')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'dist', 'cjcov.exe'), os.path.abspath(prefix))

    elapsed = time.time() - start_time
    log_build_time("install", "", "", "native", "", elapsed, time_log or BUILD_TIME_LOG)
    print("Successfully install cjcov (elapsed {})!".format(format_duration(elapsed)))
    return 0

# Clean output of build
def clean():
    if os.path.exists(os.path.join(CURRENT_DIR, 'bin')):
        shutil.rmtree(os.path.join(CURRENT_DIR, 'bin'))

    if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist')):
        shutil.rmtree(os.path.join(CURRENT_DIR, '..', 'dist'))

    print("Successfully clean cjcov!")
    return 0

def main():
    parser = argparse.ArgumentParser(description='Build system')
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Build command
    build_parser = subparsers.add_parser('build', help='Build cjcov')
    build_parser.add_argument('-t', '--build-type', type=str, dest='build_type', help='Specify build type', required=True)
    build_parser.add_argument('--target', type=str, dest='target', help='Specify build target')
    build_parser.add_argument('--set-rpath', type=str, dest='rpath', help='Set rpath value')
    build_parser.add_argument(
        "--time-log", dest="time_log", type=str, default=BUILD_TIME_LOG,
        help="Path to build time log file (default: build/logs/build_time.log)"
    )

    # Install command
    install_parser = subparsers.add_parser('install', help='Install cjcov')
    install_parser.add_argument('--prefix', help='Specify installation prefix')
    install_parser.add_argument(
        "--time-log", dest="time_log", type=str, default=BUILD_TIME_LOG,
        help="Path to build time log file (default: build/logs/build_time.log)"
    )

    # Clean command
    subparsers.add_parser('clean', help='Clean build files')

    args = parser.parse_args()

    if args.command == 'build':
        return build(build_type=args.build_type, target=args.target, rpath=args.rpath, time_log=args.time_log)
    elif args.command == 'install':
        return install(prefix=args.prefix, time_log=args.time_log)
    elif args.command == 'clean':
        return clean()
    else:
        parser.print_help()
        return 0

if __name__ == '__main__':
    main()