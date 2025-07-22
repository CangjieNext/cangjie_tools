#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

"""cangjie language server build entry"""

import argparse
import os
import platform
import shutil
import stat
import subprocess
from subprocess import PIPE
from pathlib import Path

HOME_DIR = os.path.dirname(Path(__file__).resolve().parent)
BUILD_DIR = os.path.join(HOME_DIR, "build-lsp")
THIRDPARTY_DIR = os.path.join(HOME_DIR, "third_party")
LIB_DIR = os.path.join(HOME_DIR, "src", "lib")
CANGJIE_LIB_DIR = os.path.join(LIB_DIR, "cangjie", "lib", "cjnative")

IS_WINDOWS = platform.system() == "Windows"
IS_MACOS = platform.system() == "Darwin"
IS_LINUX = platform.system() == "Linux"
JSON_GIT = "https://gitee.com/mirrors/nlohmann-json.git"
FLATBUFFER_GIT = "https://gitee.com/mirrors_trending/flatbuffers.git"

def resolve_path(path):
    if os.path.isabs(path):
        return path
    return os.path.abspath(path)
def download_json():
    cmd = ["git", "clone", "-b", "v3.11.3", "--depth=1", JSON_GIT, "json-v3.11.3"]
    output = subprocess.Popen(cmd, cwd=THIRDPARTY_DIR, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())


def download_flatbuffers(args):
    cmd = ["git", "clone", "-b", "v24.3.25", "--depth=1", FLATBUFFER_GIT, "flatbuffers"]
    output = subprocess.Popen(cmd, cwd=THIRDPARTY_DIR, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())


def generate_flat_header():
    index_file = os.path.join(HOME_DIR, "generate", "index.fbs")
    flatbuffers_dir = os.path.join(THIRDPARTY_DIR, "flatbuffers")
    new_index_path = os.path.join(flatbuffers_dir, "include", "index.fbs")
    shutil.copy(index_file, new_index_path)

    flatbuffers_build_dir = os.path.join(flatbuffers_dir, "build")
    if not os.path.exists(flatbuffers_build_dir):
        os.makedirs(flatbuffers_build_dir)
    compile_cmd = ["cmake", flatbuffers_dir, "-G", "Unix Makefiles"]
    output = subprocess.Popen(compile_cmd, cwd=flatbuffers_build_dir, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())
    build_cmd = ["cmake", "--build", flatbuffers_build_dir, "-j8"]
    output = subprocess.Popen(build_cmd, cwd=flatbuffers_build_dir, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())
    flatc_binary = "flatc"
    if IS_WINDOWS:
        flatc_binary = "flatc.exe"
    flac_cmd = os.path.join(flatbuffers_build_dir, flatc_binary)
    generate_cmd = [flac_cmd, "--cpp", index_file]
    output = subprocess.Popen(generate_cmd, cwd=flatbuffers_build_dir, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())
    header_path = os.path.join(flatbuffers_build_dir, "index_generated.h")
    new_header_path = os.path.join(flatbuffers_dir, "include", "index_generated.h")
    shutil.copy(header_path, new_header_path)

def prepare_cangjie(args):
    if args.cangjie_sdk_path is None:
        return
    if not os.path.exists(LIB_DIR):
        os.makedirs(LIB_DIR)
    if not os.path.exists(CANGJIE_LIB_DIR):
        os.makedirs(CANGJIE_LIB_DIR)
    cangjie_sdk_path = resolve_path(args.cangjie_sdk_path)
    if os.path.exists(cangjie_sdk_path):
        # copy header files
        cangjie_header_dir = os.path.join(cangjie_sdk_path, "include")
        new_header_dir = os.path.join(LIB_DIR, "cangjie", "include")
        shutil.copytree(cangjie_header_dir, new_header_dir, dirs_exist_ok=True)
        # copy libraries
        tools_lib_dir = os.path.join(cangjie_sdk_path, "tools", "lib")
        tools_bin_dir = os.path.join(cangjie_sdk_path, "tools", "bin")
        file_extension = ""
        if IS_WINDOWS or args.cross_windows:
            file_extension = "dll"
        if IS_LINUX and not args.cross_windows:
            file_extension = "so"
        if IS_MACOS:
            file_extension = "dylib"
        cangjie_lib_path = os.path.join(tools_lib_dir, "libcangjie-lsp." + file_extension)
        if IS_WINDOWS or args.cross_windows:
            cangjie_lib_path = os.path.join(tools_bin_dir, "libcangjie-lsp." + file_extension)
        new_lib_path = os.path.join(CANGJIE_LIB_DIR, "libcangjie-lsp." + file_extension)
        shutil.copy(cangjie_lib_path, new_lib_path)
        if IS_WINDOWS or args.cross_windows:
            cangjie_lib_path = os.path.join(tools_lib_dir, "libcangjie-lsp.dll.a")
            new_lib_path = os.path.join(CANGJIE_LIB_DIR, "libcangjie-lsp.dll.a")
            shutil.copy(cangjie_lib_path, new_lib_path)

def prepare_build(args):
    prepare_cangjie(args)
    if not os.path.exists(THIRDPARTY_DIR):
        os.makedirs(THIRDPARTY_DIR)
    if not os.path.exists(os.path.join(THIRDPARTY_DIR, "json-v3.11.3")):
        download_json()
    if not os.path.exists(os.path.join(THIRDPARTY_DIR, "flatbuffers")):
        download_flatbuffers(args)
        generate_flat_header()

def get_generator():
    generator = "Unix Makefiles"
    if IS_WINDOWS:
        generator = "MinGW Makefiles"
    return generator

def get_compiler_type():
    compiler_type = "Ninja"
    if IS_WINDOWS:
        compiler_type = "MINGW"
    return compiler_type

def get_build_commands(args):
    result = [
        "cmake",
        "--build",
        BUILD_DIR
    ]
    if args.jobs > 0:
        result.extend(["-j", str(args.jobs)])
    return result

def generate_cmake_commands(args):
    # cmake .. -G "MinGW Makefiles" -DCOMPILER_TYPE=MINGW -DCMAKE_BUILD_TYPE=Release
    result = [
        "cmake", HOME_DIR,
        "-G", get_generator(),
        "-DCOMPILER_TYPE=" + get_compiler_type(),
        "-DCMAKE_BUILD_TYPE=" + args.build_type
    ]
    if args.cross_windows:
        result.extend([
            "-DCROSS_WINDOWS=ON"
        ])
    return result

def build(args):
    print("start build")
    prepare_build(args)
    cmake_command = generate_cmake_commands(args)
    build_command = get_build_commands(args)
    if not os.path.exists(BUILD_DIR):
        os.makedirs(BUILD_DIR)
        output = subprocess.Popen(cmake_command, cwd=BUILD_DIR, stdout=PIPE)
        for line in output.stdout:
            print(line.decode("ascii", "ignore").rstrip())
    print(build_command)
    output = subprocess.Popen(build_command, cwd=BUILD_DIR, stdout=PIPE)
    for line in output.stdout:
        print(line.decode("ascii", "ignore").rstrip())
    print("end build")

def redo_with_write(redo_func, path, err):
    # Is the error an access error?
    if not os.access(path, os.W_OK):
        os.chmod(path, stat.S_IWUSR)
        redo_func(path)
    else:
        raise

def delete_folder(folder_path):
    try:
        shutil.rmtree(folder_path, onerror=redo_with_write)
    except Exception as e:
        print(f"delete {folder_path} failed: {str(e)}")

def clean(args):
    """
    clean target
    output build-lsp
    third_party/json-v3.11.3
    third_party/flatbuffers
    src/lib
    """
    print("start clean")
    delete_folders = ["output", "build-lsp"]
    for folder in delete_folders:
        folder = os.path.join(HOME_DIR, folder)
        if os.path.exists(folder):
            delete_folder(os.path.join(HOME_DIR, folder))
    if os.path.exists(THIRDPARTY_DIR):
        delete_folder(THIRDPARTY_DIR)
    third_party_folders = ["json-v3.11.3", "flatbuffers"]
    for folder in third_party_folders:
        folder = os.path.join(THIRDPARTY_DIR, folder)
        if os.path.exists(folder):
            delete_folder(os.path.join(THIRDPARTY_DIR, folder))
    if os.path.exists(LIB_DIR):
        delete_folder(LIB_DIR)
    print("end clean")

def main():
    """build entry"""
    parser = argparse.ArgumentParser(description="build lspserver project")
    subparsers = parser.add_subparsers(help="sub command help")
    parser_build = subparsers.add_parser("build", help=" build cangjie")
    parser_build.add_argument(
        "-t",
        "--build-type",
        choices=["Release", "Debug"],
        dest="build_type",
        type=str,
        default="Release",
        help="select target build type"
    )
    parser_build.add_argument(
        "-j", "--jobs", dest="jobs", type=int, default=0, help="run N jobs in parallel"
    )
    parser_build.add_argument(
        "--cangjie-sdk-path",
        dest="cangjie_sdk_path",
        type=str,
        help="set cangjie sdk dir to copy header files and libraries"
    )
    parser_build.add_argument(
        "--cross-windows",
        action="store_true",
        help="set cross windows build"
    )
    parser_build.set_defaults(func=build)

    parser_clean = subparsers.add_parser("clean", help="clean build")
    parser_clean.set_defaults(func=clean)

    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    os.environ["LANG"] = "C.UTF-8"
    main()