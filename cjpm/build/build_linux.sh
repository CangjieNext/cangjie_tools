# Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
# This source file is part of the Cangjie project, licensed under Apache-2.0
# with Runtime Library Exception.
#
# See https://cangjie-lang.cn/pages/LICENSE for license information.

set -e

# Check CANGJIE_HOME
if [ -z "${CANGJIE_HOME}" ]; then
    echo "error: cannot find CANGJIE_HOME, please make sure cangjie sdk is configured" >&2
    exit 1
fi

# Check CANGJIE_STDX_PATH
if [ -z "$CANGJIE_STDX_PATH" ]; then
    echo "error: cannot find CANGJIE_STDX_PATH, please make sure stdx lib is configured" >&2
    exit 1
fi

CURRENT_DIR=$(cd $(dirname $0); pwd)
COMMON_OPTION="-j1 --trimpath $CURRENT_DIR --import-path $CURRENT_DIR/bin"

mkdir -p $CURRENT_DIR/bin/cjpm
mkdir -p $CURRENT_DIR/../dist

# Set rpath
RPATH_SET_OPTION=""
while [ -n "$1" ]; do
    var="$1"
    OPTIONS=$(echo ${var%%=*})
    case "$OPTIONS" in
    --set-rpath)
        if [ -n "$2" ]; then
            RPATH_SET_OPTION="--link-options=--disable-new-dtags -rpath=$2"
            shift
        else
            echo "error: --set-rpath needs a value"
            exit 1
        fi
        ;;
    *)
        echo "error: unrecognized build option ${var}"
        exit 0;;
    esac
    shift
done

# build cjpm
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/toml --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/bin/cjpm -o libcjpm.toml.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/config --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/bin/cjpm -o libcjpm.config.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/implement --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/bin/cjpm -o libcjpm.implement.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/command --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/bin/cjpm -o libcjpm.command.a
cjc $COMMON_OPTION "$RPATH_SET_OPTION" "--link-options=-z noexecstack -z relro -z now -s" --import-path $CANGJIE_STDX_PATH -L $CURRENT_DIR/bin/cjpm -lcjpm.command -lcjpm.implement -lcjpm.config -lcjpm.toml -L $CANGJIE_STDX_PATH -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -p $CURRENT_DIR/../src -O2 --output-dir $CURRENT_DIR/bin/cjpm -o cjpm

cp $CURRENT_DIR/bin/cjpm/cjpm $CURRENT_DIR/../dist/
