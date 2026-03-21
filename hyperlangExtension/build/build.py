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

# Check CANGJIE_HOME
if not os.environ.get("CANGJIE_HOME"):
    print("error: cannot find CANGJIE_HOME, please make sure cangjie sdk is configured", file=sys.stderr)
    sys.exit(1)

if not os.environ.get("CANGJIE_STDX_PATH"):
    print("error: cannot find CANGJIE_STDX_PATH, please make sure stdx lib is configured", file=sys.stderr)
    sys.exit(1)

CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))

IS_WINDOWS = platform.system() == "Windows"
IS_LINUX = platform.system() == "Linux"
IS_MACOS = platform.system() == "Darwin"
IS_CROSS_WINDOWS = False
SUPPORTED_BUILD_TYPE = ["debug", "release"]
SUPPORTED_TARGET = ["native", "windows-x86_64"]
HLE_SRC_DIRS = ["dtsparser", "tool", "entry", "app"]
HLE_EXEC_LINK_DEPS = ["app", "entry", "tool", "dtsparser"]
MACOS_HLE_DYLIB_DEPS = {
    "dtsparser": [],
    "tool": [],
    "entry": ["tool"],
    "app": ["entry", "tool"],
}
MACOS_HLE_EXEC_DEPS = ["app"]


def get_stdx_link_args(stdx_path=None):
    if not stdx_path:
        stdx_path = os.environ["CANGJIE_STDX_PATH"]
    return [
        '-L', stdx_path,
        '-lstdx.logger',
        '-lstdx.log',
        '-lstdx.encoding.json.stream',
        '-lstdx.serialization.serialization',
        '-lstdx.encoding.json',
        '-lstdx.encoding.url',
    ]


def get_macos_stdx_path():
    stdx_path = os.environ["CANGJIE_STDX_PATH"]
    static_suffix = f"{os.sep}static{os.sep}stdx"
    dynamic_suffix = f"{os.sep}dynamic{os.sep}stdx"
    if stdx_path.endswith(static_suffix):
        dynamic_path = stdx_path[:-len(static_suffix)] + dynamic_suffix
        if os.path.isdir(dynamic_path):
            return dynamic_path
    return stdx_path


def normalize_macos_arch(machine):
    machine = machine.lower()
    if machine in ("arm64", "aarch64"):
        return "aarch64"
    if machine in ("x86_64", "x86-64", "amd64"):
        return "x86_64"
    return machine


def build_link_options(*options):
    valid_options = [option for option in options if option]
    if not valid_options:
        return []
    return [f"--link-options={' '.join(valid_options)}"]


def get_macos_rpath_options():
    arch = normalize_macos_arch(platform.machine())
    return [
        "-rpath @loader_path",
        f"-rpath @loader_path/../../lib/darwin_{arch}_cjnative",
        f"-rpath @loader_path/../../runtime/lib/darwin_{arch}_cjnative",
    ]


def get_macos_dylib_link_options(output_name):
    return build_link_options(
        f"-install_name @rpath/{output_name}",
        *get_macos_rpath_options(),
    )


def get_macos_executable_link_options():
    return build_link_options(*get_macos_rpath_options())


def get_hle_link_args(output_dir, packages):
    if not packages:
        return []
    args = ['-L', output_dir]
    for package in packages:
        args.append(f'-lhle.{package}')
    return args


def copy_dylibs(src_dir, dst_dir, prefix=None):
    if not os.path.isdir(src_dir):
        return
    os.makedirs(dst_dir, exist_ok=True)
    for filename in os.listdir(src_dir):
        if not filename.endswith('.dylib'):
            continue
        if prefix and not filename.startswith(prefix):
            continue
        shutil.copy(os.path.join(src_dir, filename), dst_dir)

# Check command
def check_call(command):
    try:
        env = os.environ.copy()
        env["ZERO_AR_DATE"] = "1"
        return subprocess.check_call(command, env=env)
    except subprocess.CalledProcessError as e:
        print(f"Command '{e.cmd}' returned non-zero exit status {e.returncode}.")
        sys.exit(e.returncode) 

# Build HLE
def build(args):
    target = args.target
    build_type = args.build_type

    global IS_LINUX
    global IS_CROSS_WINDOWS
    if target != "native" and not IS_LINUX:
        print("error: cross compile is only supported from Linux to windows-x86_64.", file=sys.stderr)
        return 1
    if target == "windows-x86_64" and IS_LINUX:
        IS_CROSS_WINDOWS = True
        IS_LINUX = False

    # Set common compile option
    DEBUG_MODE = ""
    if build_type == "debug":
        DEBUG_MODE = "-g"

    if IS_WINDOWS: 
        COMMON_OPTION = ['cjc.exe', f"--trimpath={CURRENT_DIR} {DEBUG_MODE}", '--import-path', f"{os.path.join(CURRENT_DIR, '..', 'target')}"]
    else:
        COMMON_OPTION = ['cjc', '-j1', f"--trimpath={CURRENT_DIR} {DEBUG_MODE}", '--import-path', f"{os.path.join(CURRENT_DIR, '..', 'target')}"]
    
    # Create output directories
    os.makedirs(os.path.join(CURRENT_DIR, '..', 'target', 'bin'), exist_ok=True)
    os.makedirs(os.path.join(CURRENT_DIR, '..', 'target', 'hle'), exist_ok=True)
    target_bin_dir = os.path.join(CURRENT_DIR, '..', 'target', 'bin')
    target_hle_dir = os.path.join(CURRENT_DIR, '..', 'target', 'hle')
    main_source = os.path.join(CURRENT_DIR, '..', 'src', 'main.cj')
    macos_stdx_path = get_macos_stdx_path() if IS_MACOS else ""

    for src in HLE_SRC_DIRS:
        if IS_LINUX or IS_MACOS:
            if IS_MACOS:
                output_name = f"libhle.{src}.dylib"
                returncode = check_call(
                    COMMON_OPTION
                    + get_macos_dylib_link_options(output_name)
                    + ['-O2', '--strip-all', '--no-sub-pkg', '-p', f"{os.path.join(CURRENT_DIR, '..', 'src', src)}"]
                    + ['--import-path', macos_stdx_path]
                    + get_hle_link_args(target_hle_dir, MACOS_HLE_DYLIB_DEPS[src])
                    + get_stdx_link_args(macos_stdx_path)
                    + ['--output-type=dylib', '--output-dir', target_hle_dir, '-o', output_name]
                )
            else:
                returncode = check_call(COMMON_OPTION + ['-O2', '--strip-all', '--no-sub-pkg', '-p', f"{os.path.join(CURRENT_DIR, '..', 'src', src)}", '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}", '--output-type=staticlib', '--output-dir', target_hle_dir, '-o', f"libhle.{src}.a"])
        if IS_WINDOWS:
            returncode = check_call(COMMON_OPTION + ['-O2', '--strip-all', '--no-sub-pkg', '-p', f"{os.path.join(CURRENT_DIR, '..', 'src', src)}", '--import-path', target_hle_dir, '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}", '--output-type=staticlib', '--output-dir', target_hle_dir, '-o', f"libhle.{src}.a"])
        if IS_CROSS_WINDOWS:
            returncode = check_call(COMMON_OPTION + ['--target=x86_64-windows-gnu', '-O2', '--strip-all', '--no-sub-pkg', '-p', f"{os.path.join(CURRENT_DIR, '..', 'src', src)}", '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}", '--output-type=staticlib', '--output-dir', target_hle_dir, '-o', f"libhle.{src}.a"])
        if returncode != 0: 
            print(f"build {src} failed")
            return returncode
        
    if IS_LINUX:
        returncode = check_call(
            COMMON_OPTION
            + ['--link-options=-z noexecstack -z relro -z now -s', '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}"]
            + get_hle_link_args(target_hle_dir, HLE_EXEC_LINK_DEPS)
            + get_stdx_link_args()
            + [main_source, '-O2', '--output-dir', target_bin_dir, '-o', 'main']
        )
    if IS_MACOS:
        returncode = check_call(
            COMMON_OPTION
            + get_macos_executable_link_options()
            + get_hle_link_args(target_hle_dir, MACOS_HLE_EXEC_DEPS)
            + [main_source, '-O2', '--output-dir', target_bin_dir, '-o', 'main']
        )
    if IS_CROSS_WINDOWS:
        returncode = check_call(
            COMMON_OPTION
            + ['--target=x86_64-windows-gnu', '--import-path', target_hle_dir, '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}", '--link-options=--no-insert-timestamp']
            + get_hle_link_args(target_hle_dir, HLE_EXEC_LINK_DEPS)
            + get_stdx_link_args()
            + [main_source, '-O2', '--output-dir', target_bin_dir, '-o', 'main.exe']
        )
    if IS_WINDOWS:
        returncode = check_call(
            COMMON_OPTION
            + ['--import-path', target_hle_dir, '--import-path', f"{os.environ['CANGJIE_STDX_PATH']}", '--link-options=--no-insert-timestamp']
            + get_hle_link_args(target_hle_dir, HLE_EXEC_LINK_DEPS)
            + get_stdx_link_args()
            + [main_source, '-O2', '--output-dir', target_bin_dir, '-o', 'main.exe']
        )
    if returncode != 0: 
        return returncode
    if IS_MACOS:
        copy_dylibs(target_hle_dir, target_bin_dir, prefix='libhle.')
        copy_dylibs(macos_stdx_path, target_bin_dir)

    print("Successfully build HLE!")

# Install HLE
def install(args):
    prefix = args.prefix
    if not os.path.exists(os.path.join(CURRENT_DIR, '..', 'target')):
        print("error: no HLE output, please run command 'build' first.")
        return 1

    if prefix:
        os.makedirs(os.path.abspath(prefix), exist_ok=True)
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'target', 'bin', 'main')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'target', 'bin', 'main'), os.path.join(os.path.abspath(prefix), 'hle'))
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'target', 'bin', 'main.exe')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'target', 'bin', 'main.exe'), os.path.join(os.path.abspath(prefix), 'hle.exe'))
        if IS_MACOS:
            copy_dylibs(os.path.join(CURRENT_DIR, '..', 'target', 'bin'), os.path.abspath(prefix))
    print("Successfully install HLE!")

# Clean output of build
def clean():
    if os.path.exists(os.path.join(CURRENT_DIR, '..', 'target')):
        shutil.rmtree(os.path.join(CURRENT_DIR, '..', 'target'))
    print("Successfully clean HLE!")

def main():
    # set options
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Build command
    build_parser = subparsers.add_parser('build', help='Build HLE')
    build_parser.add_argument('-t', '--build-type', default='release', choices=list(SUPPORTED_BUILD_TYPE), help='Select target build type')
    build_parser.add_argument('--target', default='native', choices=list(SUPPORTED_TARGET), help='Specify the target platform for cross compilation')

    # Install command
    install_parser = subparsers.add_parser('install', help='Install HLE')
    install_parser.add_argument('--prefix', help='Specify installation prefix')

    # Clean command
    subparsers.add_parser('clean', help='Clean build files')

    args = parser.parse_args()
    if args.command == 'build':
        return build(args)
    elif args.command == 'install':
        return install(args)
    elif args.command == 'clean':
        return clean()
    else:
        parser.print_help()
        return 0

if __name__ == '__main__':
    main()
