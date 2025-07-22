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
COMMON_OPTION="-j4 --trimpath $CURRENT_DIR --import-path $CURRENT_DIR/../target/release"

mkdir -p $CURRENT_DIR/../target/release/bin
mkdir -p $CURRENT_DIR/../target/release/hle

# build hle
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/dtsparser -O2 --strip-all --no-sub-pkg --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/../target/release/hle -o libhle.dtsparser.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/tool -O2 --strip-all --no-sub-pkg --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/../target/release/hle -o libhle.tool.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src/entry -O2 --strip-all --no-sub-pkg --import-path $CANGJIE_STDX_PATH --output-type=staticlib --output-dir $CURRENT_DIR/../target/release/hle -o libhle.entry.a
cjc $COMMON_OPTION -p $CURRENT_DIR/../src --import-path $CANGJIE_STDX_PATH -L $CURRENT_DIR/../target/release/hle -lhle.dtsparser -lhle.tool -lhle.entry -L $CANGJIE_STDX_PATH -lstdx.logger -lstdx.log -lstdx.encoding.json.stream -lstdx.serialization.serialization -lstdx.encoding.json -lstdx.encoding.url -O2 --output-dir $CURRENT_DIR/../target/release/bin -o main

