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

CURRENT_DIR = os.path.dirname(os.path.realpath(__file__))
CPP_DIR = os.path.join(CURRENT_DIR, "..", "cpp")
THIRD_PARTY_LIBUV_DIR = os.path.join(CPP_DIR, "third_party", "libuv")
CPP_BUILD_DIR = os.path.join(CPP_DIR, "build")
CPP_OUT_DIR = os.path.join(CPP_DIR, "out")
LIBFSWATCHER_A_PATH = os.path.join(CPP_BUILD_DIR, "lib", "libfswatcher.a")
LIBUV_A_PATH = os.path.join(CPP_BUILD_DIR, "lib", "libuv.a")
CJPM_SRC_DIRS = ["toml", "util", "config", "implement", "command"]
MACOS_CJPM_DYLIB_DEPS = {
    "toml": [],
    "util": [],
    "config": ["toml", "util"],
    "implement": ["config", "util", "toml"],
    "command": ["implement", "config", "util", "toml"],
}


def get_stdx_link_args(stdx_path=None):
    if not stdx_path:
        stdx_path = os.environ["CANGJIE_STDX_PATH"]
    return (
        f"-L {stdx_path} -lstdx.encoding.json -lstdx.serialization.serialization -lstdx.net.tls "
        f"-lstdx.net.http -lstdx.net.tls.common -lstdx.logger -lstdx.log -lstdx.encoding.url "
        f"-lstdx.encoding.json.stream -lstdx.crypto.x509 -lstdx.crypto.keys -lstdx.encoding.hex "
        f"-lstdx.crypto.crypto -lstdx.crypto.digest -lstdx.crypto.common -lstdx.encoding.base64 "
        f"-lstdx.compress -lstdx.compress.zlib -lstdx.compress.tar"
    )


def get_macos_cpp_link_args():
    return f"-L {CPP_OUT_DIR} -lfswatcher -luv -lc++ -ldl -lpthread"


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


def get_macos_stdlib_rpath(runtime_rpath=None):
    if runtime_rpath and "runtime/lib/" in runtime_rpath:
        return runtime_rpath.replace("runtime/lib/", "lib/", 1)
    arch = normalize_macos_arch(platform.machine())
    return f"@loader_path/../../lib/darwin_{arch}_cjnative"


def get_macos_openssl_rpath():
    openssl_path = os.environ.get("OPENSSL_PATH")
    if openssl_path and os.path.isdir(openssl_path):
        return os.path.abspath(openssl_path)

    openssl_root_dir = os.environ.get("OPENSSL_ROOT_DIR")
    if openssl_root_dir:
        lib_dir = os.path.join(os.path.abspath(openssl_root_dir), "lib")
        if os.path.isdir(lib_dir):
            return lib_dir
    return ""


def build_link_options(*options):
    valid_options = [option for option in options if option]
    if not valid_options:
        return ""
    return f"\"--link-options={' '.join(valid_options)}\""


def join_command_parts(*parts):
    return " ".join(part for part in parts if part)


def get_macos_rpath_options(runtime_rpath):
    rpath_options = ["-rpath @loader_path", f"-rpath {get_macos_stdlib_rpath(runtime_rpath)}"]
    if runtime_rpath:
        rpath_options.append(f"-rpath {runtime_rpath}")
    openssl_rpath = get_macos_openssl_rpath()
    if openssl_rpath:
        rpath_options.append(f"-rpath {openssl_rpath}")
    return rpath_options


def get_macos_dylib_link_option(output_name, runtime_rpath):
    return build_link_options(
        f"-install_name @rpath/{output_name}",
        *get_macos_rpath_options(runtime_rpath),
    )


def get_macos_executable_link_option(runtime_rpath):
    return build_link_options(*get_macos_rpath_options(runtime_rpath))


def get_cjpm_link_args(bin_dir, packages):
    if not packages:
        return ""
    libs = " ".join(f"-lcjpm.{package}" for package in packages)
    return f"-L {bin_dir} {libs}"


def copy_cjpm_dylibs(src_dir, dst_dir):
    for src in CJPM_SRC_DIRS:
        dylib = os.path.join(src_dir, f"libcjpm.{src}.dylib")
        if os.path.exists(dylib):
            shutil.copy(dylib, dst_dir)


def copy_macos_stdx_dylibs(src_dir, dst_dir):
    if not os.path.isdir(src_dir):
        return
    for filename in os.listdir(src_dir):
        if filename.endswith(".dylib"):
            shutil.copy(os.path.join(src_dir, filename), dst_dir)

# Check command
def check_call(command, cwd=None):
    try:
        env = os.environ.copy()
        env["ZERO_AR_DATE"] = "1"
        return subprocess.check_call(command, shell=True, env=env, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Command '{e.cmd}' returned non-zero exit status {e.returncode}.")
        return e.returncode

# Build libfswatcher
def build_fswatcher(cmake_option="", make_prefix=""):
    if os.path.exists(CPP_OUT_DIR):
        shutil.rmtree(CPP_OUT_DIR)
    if not os.path.exists(THIRD_PARTY_LIBUV_DIR):
        returncode = check_call(f"git clone https://gitcode.com/libuv/libuv.git {THIRD_PARTY_LIBUV_DIR}")
        if returncode != 0:
            return returncode
        returncode = check_call(f"git checkout v1.48.0", cwd=THIRD_PARTY_LIBUV_DIR)
        if returncode != 0:
            return returncode
    os.makedirs(CPP_BUILD_DIR, exist_ok=True)
    returncode = check_call(f"cmake .. {cmake_option} -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release", cwd=CPP_BUILD_DIR)
    if returncode != 0:
        return returncode
    returncode = check_call(f"{make_prefix}make -j4", cwd=CPP_BUILD_DIR)
    if returncode != 0:
        return returncode
    os.makedirs(CPP_OUT_DIR, exist_ok=True)
    if not os.path.exists(LIBFSWATCHER_A_PATH):
        print(f"error: cannot find {LIBFSWATCHER_A_PATH}")
        return 1
    shutil.move(LIBFSWATCHER_A_PATH, CPP_OUT_DIR)
    if not os.path.exists(LIBUV_A_PATH):
        print(f"error: cannot find {LIBUV_A_PATH}")
        return 1
    shutil.move(LIBUV_A_PATH, CPP_OUT_DIR)
    print("Successfully build libfswatcher!")
    return 0

# Build cjpm
def build(build_type, target, rpath=None, native_only=False):
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
    is_windows = platform.system() == "Windows"
    is_linux = platform.system() == "Linux"
    is_macos = platform.system() == "Darwin"
    is_cross_windows = False
    is_cross_ohos = False
    toolchain = ""

    if target != "native" and not is_linux:
        print("error: cross compile is only supported from Linux to windows-x86_64/ohos-x86_64/ohos-aarch64.", file=sys.stderr)
        return 1
    if target == "windows-x86_64" and is_linux:
        is_cross_windows = True
        is_linux = False
    if is_linux:
        if target == "ohos-x86_64":
            target = "x86_64-linux-ohos"
            is_cross_ohos = True
            is_linux = False
            toolchain = f"--toolchain={os.environ['CANGJIE_HOME']}/lib/linux_ohos_x86_64_cjnative"
        elif target == "ohos-aarch64":
            target = "aarch64-linux-ohos"
            is_cross_ohos = True
            is_linux = False
            toolchain = f"--toolchain={os.environ['CANGJIE_HOME']}/lib/linux_ohos_aarch64_cjnative"

    # Set rpath
    rpath_set_option = ""
    if rpath:
        if is_macos:
            rpath_set_option = f"--link-options=\"-rpath {rpath}\""
        elif is_linux or is_cross_ohos:
            rpath_set_option = f"--link-options=\"--disable-new-dtags -rpath={rpath}\""

    # Set common compile option
    debug_mode = ""
    if build_type == "debug":
        debug_mode = "-g"
    elif build_type != "release":
        print("error: cjpm only support 'release' and 'debug' mode of compiling.")
        return 1

    if is_windows:
        common_option = f"--trimpath={CURRENT_DIR} {debug_mode} --import-path {os.path.join(CURRENT_DIR,'bin')}"
    else:
        common_option = f"-j1 --trimpath={CURRENT_DIR} {debug_mode} --import-path {os.path.join(CURRENT_DIR,'bin')}"

    # Get cjc executable file
    if is_windows:
        cjc = "cjc.exe"
    else:
        cjc = "cjc"

    # Check MINGW_PATH
    if is_windows or is_cross_windows:
        if not os.environ.get("MINGW_PATH"):
            print("error: cannot find MINGW_PATH, please make sure llvm-mingw toolchain path is configured.")
            return 1

    # Create output directories
    os.makedirs(os.path.join(CURRENT_DIR, 'bin', 'cjpm'), exist_ok=True)
    os.makedirs(os.path.join(CURRENT_DIR, '..', 'dist'), exist_ok=True)

    returncode = 0
    if is_linux or is_macos:
        returncode = build_fswatcher("")
    if is_windows:
        root_include = os.path.join(os.environ.get("MINGW_PATH"), "include")
        target_include = os.path.join(os.environ.get("MINGW_PATH"), "x86_64-w64-mingw32", "include")
        root_include_exists = os.path.isdir(root_include)
        target_include_exists = os.path.isdir(target_include)
        if root_include_exists and not target_include_exists:
            try:
                shutil.copytree(root_include, target_include)
            except Exception as e:
                print(f"Error: failed to copy include - {str(e)}")
                return 1
        returncode = build_fswatcher('-DCMAKE_TOOLCHAIN_FILE=../llvm-mingw-toolchain.cmake -DHOST_ARCH=x86_64 -DUV_BUILD_TESTS=OFF -G "MinGW Makefiles"', "mingw32-")
    if is_cross_windows:
        returncode = build_fswatcher("-DCMAKE_TOOLCHAIN_FILE=../cross-windows-toolchain.cmake -DHOST_ARCH=x86_64 -DUV_BUILD_TESTS=OFF")
    if returncode != 0 or native_only:
        return returncode

    bin_dir = os.path.join(CURRENT_DIR, 'bin', 'cjpm')
    macos_stdx_path = get_macos_stdx_path() if is_macos else ""

    # Compile sub-packages
    for src in CJPM_SRC_DIRS:
        if is_linux or is_macos:
            if is_macos:
                output_name = f"libcjpm.{src}.dylib"
                dylib_deps = get_cjpm_link_args(bin_dir, MACOS_CJPM_DYLIB_DEPS[src])
                extra_link_args = get_stdx_link_args(macos_stdx_path)
                if src == "implement":
                    extra_link_args = join_command_parts(extra_link_args, get_macos_cpp_link_args())
                returncode = check_call(join_command_parts(
                    cjc,
                    common_option,
                    get_macos_dylib_link_option(output_name, rpath),
                    f"-p {os.path.join(CURRENT_DIR, '..', 'src', src)}",
                    f"--import-path {macos_stdx_path}",
                    dylib_deps,
                    extra_link_args,
                    "--output-type=dylib",
                    f"--output-dir {bin_dir}",
                    f"-o {output_name}",
                ))
            else:
                returncode = check_call(f"{cjc} {common_option} -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o libcjpm.{src}.a")
        if is_windows:
            returncode = check_call(f"{cjc} {common_option} -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.path.join(CURRENT_DIR, 'bin')} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o libcjpm.{src}.a")
        if is_cross_windows:
            returncode = check_call(f"{cjc} --target=x86_64-windows-gnu {common_option} -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o libcjpm.{src}.a")
        if is_cross_ohos:
            returncode = check_call(f"{cjc} --target={target} {toolchain} {common_option} -p {os.path.join(CURRENT_DIR, '..', 'src', src)} --import-path {os.environ['CANGJIE_STDX_PATH']} --output-type=staticlib --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o libcjpm.{src}.a")
        if returncode != 0:
            return returncode

    # Compile cjpm executable file
    if is_linux:
        returncode = check_call(f"{cjc} {common_option} {rpath_set_option} \"--link-options=-z noexecstack -z relro -z now -s\" --import-path {os.environ['CANGJIE_STDX_PATH']} -L {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.util -lcjpm.toml -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.encoding.json -lstdx.serialization.serialization -lstdx.net.tls -lstdx.net.http -lstdx.net.tls.common -lstdx.logger -lstdx.log -lstdx.encoding.url -lstdx.encoding.json.stream -lstdx.crypto.x509 -lstdx.crypto.keys -lstdx.encoding.hex -lstdx.crypto.crypto -lstdx.crypto.digest -lstdx.crypto.common -lstdx.encoding.base64 -lstdx.compress -lstdx.compress.zlib -lstdx.compress.tar -L {os.path.join(CURRENT_DIR, '../cpp/out')} -lfswatcher -luv -lstdc++ -lgcc_s -ldl -lpthread -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o cjpm")
    if is_macos:
        returncode = check_call(join_command_parts(
            cjc,
            common_option,
            get_macos_executable_link_option(rpath),
            f"--import-path {macos_stdx_path}",
            get_cjpm_link_args(bin_dir, ["command", "implement", "config", "util", "toml"]),
            os.path.join(CURRENT_DIR, '..', 'src', 'main.cj'),
            "-O2",
            f"--output-dir {bin_dir}",
            "-o cjpm",
        ))
    if is_cross_windows:
        stdx_path = os.environ['CANGJIE_STDX_PATH']
        static_lib = os.path.join(stdx_path, "libboundscheck-static.a")
        target_lib = os.path.join(stdx_path, "libboundscheck.a")
        static_exists = os.path.isfile(static_lib)
        target_exists = os.path.isfile(target_lib)
        if static_exists and not target_exists:
            try:
                shutil.copy2(static_lib, target_lib)
            except Exception as e:
                print(f"Error: failed to copy libboundscheck-static.a - {str(e)}")
                return 1
        returncode = check_call(f"{cjc} --target=x86_64-windows-gnu {common_option} --import-path {os.path.join(CURRENT_DIR, 'bin')} --import-path {os.environ['CANGJIE_STDX_PATH']} \"--link-options=--no-insert-timestamp -static\" -L {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.util -lcjpm.toml -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.encoding.json -lstdx.serialization.serialization -lstdx.net.tls -lstdx.net.http -lstdx.net.tls.common -lstdx.logger -lstdx.log -lstdx.encoding.url -lstdx.encoding.json.stream -lstdx.crypto.x509 -lstdx.crypto.keys -lstdx.encoding.hex -lstdx.crypto.crypto -lstdx.crypto.digest -lstdx.crypto.common -lstdx.encoding.base64 -lstdx.compress -lstdx.compress.zlib -lstdx.compress.tar -lcrypt32 -L {os.path.join(CURRENT_DIR, '../cpp/out')} -lfswatcher -luv -L /opt/buildtools/llvm-mingw-w64/x86_64-w64-mingw32/lib -lc++ -lunwind -liphlpapi -ldbghelp -luserenv -lole32 -lpthread -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o cjpm.exe")
    if is_windows:
        returncode = check_call(f"{cjc} {common_option} --import-path {os.path.join(CURRENT_DIR, 'bin')} --import-path {os.environ['CANGJIE_STDX_PATH']} --link-options=--no-insert-timestamp -L {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.util -lcjpm.toml -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.encoding.json -lstdx.serialization.serialization -lstdx.net.tls -lstdx.net.http -lstdx.net.tls.common -lstdx.logger -lstdx.log -lstdx.encoding.url -lstdx.encoding.json.stream -lstdx.crypto.x509 -lstdx.crypto.keys -lstdx.encoding.hex -lstdx.crypto.crypto -lstdx.crypto.digest -lstdx.crypto.common -lstdx.encoding.base64 -lstdx.compress -lstdx.compress.zlib -lstdx.compress.tar -lcrypt32 -L {os.path.join(CURRENT_DIR, '../cpp/out')} -lfswatcher -luv -L {os.path.join(os.environ['MINGW_PATH'], 'x86_64-w64-mingw32', 'lib')} -lc++ -lunwind -liphlpapi -ldbghelp -luserenv -lole32 -lpthread -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o cjpm.exe")
    if is_cross_ohos:
        returncode = check_call(f"{cjc} --target={target} {toolchain} {common_option} {rpath_set_option} \"--link-options=-z noexecstack -z relro -z now -s\" --import-path {os.environ['CANGJIE_STDX_PATH']} -L {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.util -lcjpm.toml -L {os.environ['CANGJIE_STDX_PATH']} -lstdx.encoding.json -lstdx.serialization.serialization -lstdx.net.tls -lstdx.net.http -lstdx.net.tls.common -lstdx.logger -lstdx.log -lstdx.encoding.url -lstdx.encoding.json.stream -lstdx.crypto.x509 -lstdx.crypto.keys -lstdx.encoding.hex -lstdx.crypto.crypto -lstdx.crypto.digest -lstdx.crypto.common -lstdx.encoding.base64 -lstdx.compress -lstdx.compress.zlib -lstdx.compress.tar -p {os.path.join(CURRENT_DIR, '..', 'src')} -O2 --output-dir {os.path.join(CURRENT_DIR, 'bin', 'cjpm')} -o cjpm")

    if returncode != 0:
        return returncode

    if is_windows or is_cross_windows:
        shutil.copy(os.path.join(CURRENT_DIR, 'bin', 'cjpm', 'cjpm.exe'), os.path.join(CURRENT_DIR, '..', 'dist'))
    else:
        shutil.copy(os.path.join(CURRENT_DIR, 'bin', 'cjpm', 'cjpm'), os.path.join(CURRENT_DIR, '..', 'dist'))
    if is_macos:
        copy_macos_stdx_dylibs(macos_stdx_path, bin_dir)
        copy_cjpm_dylibs(bin_dir, os.path.join(CURRENT_DIR, '..', 'dist'))
        copy_macos_stdx_dylibs(macos_stdx_path, os.path.join(CURRENT_DIR, '..', 'dist'))
    shutil.copy(os.path.join(CURRENT_DIR, '..', 'config', 'cangjie-repo.toml'), os.path.join(CURRENT_DIR, '..', 'dist'))

    print("Successfully build cjpm!")
    return 0

# Install cjpm
def install(prefix):
    if not os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm')) \
        and not os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm.exe')):
        print("error: no cjpm output, please run command 'build' first.")
        return 1

    if prefix:
        os.makedirs(os.path.abspath(prefix), exist_ok=True)
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm'), os.path.abspath(prefix))
        if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm.exe')):
            shutil.copy(os.path.join(CURRENT_DIR, '..', 'dist', 'cjpm.exe'), os.path.abspath(prefix))
        copy_cjpm_dylibs(os.path.join(CURRENT_DIR, '..', 'dist'), os.path.abspath(prefix))
        if platform.system() == "Darwin":
            copy_macos_stdx_dylibs(get_macos_stdx_path(), os.path.abspath(prefix))
        shutil.copy(os.path.join(CURRENT_DIR, '..', 'dist', 'cangjie-repo.toml'), os.path.abspath(prefix))

    print("Successfully install cjpm!")
    return 0

# Clean output of build
def clean():
    if os.path.exists(os.path.join(CURRENT_DIR, 'bin')):
        shutil.rmtree(os.path.join(CURRENT_DIR, 'bin'))

    if os.path.exists(os.path.join(CURRENT_DIR, '..', 'dist')):
        shutil.rmtree(os.path.join(CURRENT_DIR, '..', 'dist'))
    
    if os.path.exists(CPP_BUILD_DIR):
        shutil.rmtree(CPP_BUILD_DIR)

    if os.path.exists(CPP_OUT_DIR):
        shutil.rmtree(CPP_OUT_DIR)

    if os.path.exists(THIRD_PARTY_LIBUV_DIR):
        shutil.rmtree(THIRD_PARTY_LIBUV_DIR)

    print("Successfully clean cjpm!")
    return 0

def main():
    parser = argparse.ArgumentParser(description='Build system')
    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # Build command
    build_parser = subparsers.add_parser('build', help='Build cjpm')
    build_parser.add_argument('-t', '--build-type', type=str, dest='build_type', help='Specify build type', required=True)
    build_parser.add_argument('--target', type=str, dest='target', help='Specify build target')
    build_parser.add_argument('--set-rpath', type=str, dest='rpath', help='Set rpath value')
    build_parser.add_argument('--native-only',
                             action='store_true',
                             dest='native_only',
                             help='Only build native code')

    # Install command
    install_parser = subparsers.add_parser('install', help='Install cjpm')
    install_parser.add_argument('--prefix', help='Specify installation prefix')

    # Clean command
    subparsers.add_parser('clean', help='Clean build files')

    args = parser.parse_args()

    if args.command == 'build':
        return build(build_type=args.build_type, target=args.target, rpath=args.rpath, native_only=args.native_only)
    elif args.command == 'install':
        return install(prefix=args.prefix)
    elif args.command == 'clean':
        return clean()
    else:
        parser.print_help()
        return 0

if __name__ == '__main__':
    main()
