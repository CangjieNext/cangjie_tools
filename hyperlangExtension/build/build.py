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

# Check command
def check_call(command):
    try:
        subprocess.check_call(command, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Command '{e.cmd}' returned non-zero exit status {e.returncode}.")
        sys.exit(e.returncode) 

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

if IS_WINDOWS:
    COMMON_OPTION = f"--trimpath={CURRENT_DIR} --import-path {os.path.join(CURRENT_DIR, '..', 'target', 'release')}"
else:
    COMMON_OPTION = f"-j4 --trimpath={CURRENT_DIR} --import-path {os.path.join(CURRENT_DIR, '..', 'target', 'release')}"

# set options
parser = argparse.ArgumentParser()
parser.add_argument('--cross-windows', action='store_true', help='Cross-compiling for Windows')
args, unknown = parser.parse_known_args()

IS_CROSS_WINDOWS = args.cross_windows
if IS_CROSS_WINDOWS:
    IS_LINUX= False

if unknown:
    for var in unknown:
        print(f"error: unrecognized build option {var}")
        sys.exit(0)

os.makedirs(os.path.join(CURRENT_DIR, '..', 'target', 'release', 'bin'), exist_ok=True)
os.makedirs(os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle'), exist_ok=True)

if IS_WINDOWS:
    CJC = "cjc.exe"
else:
    CJC = "cjc"

src_dirs = ['dtsparser', 'tool', 'entry']
for src in src_dirs:
    if IS_LINUX or IS_MACOS:
        check_call(f"{CJC} {COMMON_OPTION} -O2 --strip-all --no-sub-pkg -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -o libhle.{src}.a")
    if IS_WINDOWS:
        check_call(f"{CJC} {COMMON_OPTION} -O2 --strip-all --no-sub-pkg -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -o libhle.{src}.a")
    if IS_CROSS_WINDOWS:
        check_call(f"{CJC} --target=x86_64-windows-gnu {COMMON_OPTION} -O2 --strip-all --no-sub-pkg -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -o libhle.{src}.a")

if IS_LINUX:
    check_call(f"{CJC} {COMMON_OPTION} \"--link-options=-z noexecstack -z relro -z now -s\" --import-path {os.environ['CANGJIE_STDX_PATH']} -L {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -lhle.dtsparser -lhle.tool -lhle.entry -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'bin')} -o main")
if IS_MACOS:
    check_call(f"{CJC} {COMMON_OPTION} --import-path {os.environ['CANGJIE_STDX_PATH']} -L {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -lhle.dtsparser -lhle.tool -lhle.entry -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'bin')} -o main")
if IS_CROSS_WINDOWS:
    check_call(f"{CJC} --target=x86_64-windows-gnu {COMMON_OPTION} --import-path {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} --import-path {os.environ['CANGJIE_STDX_PATH']} --link-options=--no-insert-timestamp -L {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -lhle.dtsparser -lhle.tool -lhle.entry -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'bin')} -o main.exe")
if IS_WINDOWS:
    check_call(f"{CJC} {COMMON_OPTION} --import-path {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} --import-path {os.environ['CANGJIE_STDX_PATH']} --link-options=--no-insert-timestamp -L {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'hle')} -lhle.dtsparser -lhle.tool -lhle.entry -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, '..', 'target', 'release', 'bin')} -o main.exe")

print("Successfully build HLE!")