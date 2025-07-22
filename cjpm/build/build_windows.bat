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

set CJPM_CURRENT_DIR="%~dp0\.."
set CJPM_COMMON_OPTION=--trimpath=%CJPM_CURRENT_DIR%

set CJPM_BUILD_DIR=%CJPM_CURRENT_DIR%\build
set CJPM_DIST=%CJPM_CURRENT_DIR%\dist

if not exist %CJPM_DIST% (
    mkdir %CJPM_CURRENT_DIR%\dist
)

if exist %CJPM_BUILD_DIR%\bin\ rmdir /s /q %CJPM_BUILD_DIR%\bin\
mkdir %CJPM_BUILD_DIR%\bin\cjpm

@REM build cjpm
cjc.exe %CJPM_COMMON_OPTION% --import-path %CJPM_BUILD_DIR%\bin --import-path %CANGJIE_STDX_PATH% -p %CJPM_CURRENT_DIR%\src\toml --output-type=staticlib --output-dir %CJPM_BUILD_DIR%\bin\cjpm -o libcjpm.toml.a
cjc.exe %CJPM_COMMON_OPTION% --import-path %CJPM_BUILD_DIR%\bin --import-path %CANGJIE_STDX_PATH% -p %CJPM_CURRENT_DIR%\src\config --output-type=staticlib --output-dir %CJPM_BUILD_DIR%\bin\cjpm -o libcjpm.config.a
cjc.exe %CJPM_COMMON_OPTION% --import-path %CJPM_BUILD_DIR%\bin --import-path %CANGJIE_STDX_PATH% -p %CJPM_CURRENT_DIR%\src\implement --output-type=staticlib --output-dir %CJPM_BUILD_DIR%\bin\cjpm -o libcjpm.implement.a
cjc.exe %CJPM_COMMON_OPTION% --import-path %CJPM_BUILD_DIR%\bin --import-path %CANGJIE_STDX_PATH% -p %CJPM_CURRENT_DIR%\src\command --output-type=staticlib --output-dir %CJPM_BUILD_DIR%\bin\cjpm -o libcjpm.command.a
cjc.exe %CJPM_COMMON_OPTION% --import-path %CJPM_BUILD_DIR%\bin --import-path %CANGJIE_STDX_PATH% --link-options=--no-insert-timestamp -L %CJPM_BUILD_DIR%\bin\cjpm -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.toml -L %CANGJIE_STDX_PATH% -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p %CJPM_CURRENT_DIR%\src -O2 --output-dir %CJPM_BUILD_DIR%\bin\cjpm -o cjpm.exe

copy %CJPM_BUILD_DIR%\bin\cjpm\cjpm.exe %CJPM_DIST%
