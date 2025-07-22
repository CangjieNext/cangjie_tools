@REM Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
@REM This source file is part of the Cangjie project, licensed under Apache-2.0
@REM with Runtime Library Exception.

@REM See https://cangjie-lang.cn/pages/LICENSE for license information.

@echo off

@REM Check CANGJIE_HOME
if "%CANGJIE_HOME%"=="" (
    echo error: cannot find CANGJIE_HOME, please make sure cangjie sdk is configured >&2
    exit /b 1
)

@REM Check CANGJIE_STDX_PATH
if "%CANGJIE_STDX_PATH%"=="" (
    echo error: cannot find CANGJIE_STDX_PATH, please make sure stdx lib is configured >&2
    exit /b 1
)

set HLE_CURRENT_DIR="%~dp0\.."
echo %HLE_CURRENT_DIR%
set HLE_COMMON_OPTION=--trimpath=%HLE_CURRENT_DIR%

set HLE_BUILD_DIR=%HLE_CURRENT_DIR%\target\release

if not exist %HLE_BUILD_DIR% (
    mkdir %HLE_CURRENT_DIR%\target\release
)

mkdir %HLE_BUILD_DIR%\hle
mkdir %HLE_BUILD_DIR%\bin


@REM build hle
cjc.exe %HLE_COMMON_OPTION% -O2 --strip-all --no-sub-pkg --import-path %HLE_BUILD_DIR%\hle --import-path %CANGJIE_STDX_PATH% -p %HLE_CURRENT_DIR%\src\dtsparser --output-type=staticlib --output-dir %HLE_BUILD_DIR%\hle -o libhle.dtsparser.a
cjc.exe %HLE_COMMON_OPTION% -O2 --strip-all --no-sub-pkg --import-path %HLE_BUILD_DIR%\hle --import-path %CANGJIE_STDX_PATH% -p %HLE_CURRENT_DIR%\src\tool --output-type=staticlib --output-dir %HLE_BUILD_DIR%\hle -o libhle.tool.a
cjc.exe %HLE_COMMON_OPTION% -O2 --strip-all --no-sub-pkg --import-path %HLE_BUILD_DIR%\hle --import-path %CANGJIE_STDX_PATH% -p %HLE_CURRENT_DIR%\src\entry --output-type=staticlib --output-dir %HLE_BUILD_DIR%\hle -o libhle.entry.a
cjc.exe %HLE_COMMON_OPTION% --import-path %HLE_BUILD_DIR%\hle --import-path %CANGJIE_STDX_PATH% --link-options=--no-insert-timestamp -L %HLE_BUILD_DIR%\hle -lhle.dtsparser -lhle.tool -lhle.entry -L %CANGJIE_STDX_PATH% -lstdx.compress.zlib -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p %HLE_CURRENT_DIR%\src -O2 --output-dir %HLE_BUILD_DIR%\bin -o main.exe
