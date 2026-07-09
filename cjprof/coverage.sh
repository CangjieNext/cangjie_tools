#!/bin/bash
# 用法: WORKSPACE=/ci/workspace branch=main ./coverage.sh
# 本机用法(已装 SDK 于 /test/cangjie, 测试仓库于 /root/cangjie_test):
#   ./coverage.sh    # 自动用本机环境
set -exv;

dailyBranch="DailyDev"
if [[ $branch == "main" ]]; then
    dailyBranch="DailyMain"
elif [[ $branch == "release/1.0" ]]; then
    dailyBranch="DailyRelease10"
elif [[ $branch == "release/1.1" ]]; then
    dailyBranch="DailyRelease11"
fi

# ---------- SDK / 测试仓库自适应 ----------
# CI 环境: WORKSPACE 指向包含 cangjie_test + cangjie_test_framework 的目录。
# 本机环境: SDK 在 /test/cangjie, 测试仓库在 /root/cangjie_test。
if [[ -z "$WORKSPACE" ]]; then
    export WORKSPACE="/root"
fi
if [[ -z "$CANGJIE_HOME" ]] && [[ -f /test/cangjie/envsetup.sh ]]; then
    source /test/cangjie/envsetup.sh
elif [[ -z "$CANGJIE_HOME" ]] && [[ -f "$WORKSPACE/cangjie_sdk/cangjie/envsetup.sh" ]]; then
    source "$WORKSPACE/cangjie_sdk/cangjie/envsetup.sh"
fi

cd "$WORKSPACE/cangjie_tools/cjprof";
REPO_ROOT="$(pwd)";
# 插桩构建产物目录(build.py --code-coverage 默认产物在 build/build)。
# 本机已存在 build/coverage(完整 coverage 构建), 优先用之避免重编。
BUILD_DIR="$REPO_ROOT/build/build";
if [[ ! -x "$BUILD_DIR/src/cjprof" ]] && [[ -x "$REPO_ROOT/build/coverage/src/cjprof" ]]; then
    BUILD_DIR="$REPO_ROOT/build/coverage";
fi
REPORT_DIR="$REPO_ROOT/build/coverage-report";
CJPROF="$BUILD_DIR/src/cjprof";
GCOV_SH="$BUILD_DIR/llvm-gcov.sh";
CANGJIE_TEST="$WORKSPACE/cangjie_test";
FRAMEWORK="$WORKSPACE/cangjie_test_framework";
# 本机测试框架实际嵌在 cangjie_test 仓库内。
if [[ ! -f "$FRAMEWORK/main.py" ]] && [[ -f "$CANGJIE_TEST/testsuites/cangjie_test_framework/main.py" ]]; then
    FRAMEWORK="$CANGJIE_TEST/testsuites/cangjie_test_framework";
fi
LLT_DIR="$CANGJIE_TEST/testsuites/LLT/Tools/cjprof";
HLT_DIR="$CANGJIE_TEST/testsuites/HLT/Tools/cjprof";
HLT_TESTLIST="$CANGJIE_TEST/testsuites/HLT/testlist";
HLT_CFG="$CANGJIE_TEST/testsuites/HLT/configs/cjnative/linux_x64-linux_x64/basic.cfg";

# ---------- 编译插桩版 cjprof ----------
# build.py --code-coverage 注入 -fprofile-arcs -ftest-coverage + -DCOVERAGE_BUILD。
# COVERAGE_BUILD 让 main.cpp 注册 SIGTERM/SIGINT handler 调 exit(0):
# HTTP server 被测试框架 SIGTERM 杀时, exit 触发各模块 atexit flush,
# 否则 libcjprof.so 里的 HttpServer/HttpHandlers/HeapAnalyzer 永远 0% 覆盖。
if [[ ! -x "$CJPROF" ]] || [[ "${REBUILD:-1}" == "1" && -x "$REPO_ROOT/build/build.py" ]]; then
    # 已有 build/coverage 且不想重编时, 设 REBUILD=0 跳过。
    if [[ "$BUILD_DIR" != "$REPO_ROOT/build/coverage" ]] || [[ "${REBUILD:-1}" == "1" ]]; then
        python3 build/build.py clean
        python3 build/build.py build -t debug --code-coverage;
    fi
fi

# 让 main.py 跑用例时能找到 coverage 版 cjprof (test.cfg 里 %cjprof = cjprof 裸命令名)。
export PATH="$BUILD_DIR/src:$PATH";
export LD_LIBRARY_PATH="$BUILD_DIR/src:$LD_LIBRARY_PATH";

# 写 llvm-gcov.sh 包装器: clang/llvm 编译的 .gcno/.gcda 要 llvm-cov gcov 解释
# (系统 gcov 9 读不懂 gcc10/llvm10 的 gcno 版本格式)。
LLVM_COV="${LLVM_COV:-}";
if [[ -z "$LLVM_COV" ]]; then
    for cand in /usr/lib/llvm-10/bin/llvm-cov /usr/lib/llvm-18/bin/llvm-cov llvm-cov; do
        if command -v "$cand" >/dev/null 2>&1 || [[ -x "$cand" ]]; then
            LLVM_COV="$cand"; break;
        fi
    done
fi
cat > "$GCOV_SH" <<EOF
#!/bin/bash
exec $LLVM_COV gcov "\$@"
EOF
chmod +x "$GCOV_SH";

# gcov 的 .gcda 在 vboxsf(共享文件夹) 上 msync 不可靠, 报 "cannot msync"。
# 用 GCOV_PREFIX 把 .gcda 重定向到 ext4(/tmp), 跑完拷回采集。
GCOV_EXT4="${GCOV_PREFIX:-/tmp/cjprof_gcda_$$}";
rm -rf "$GCOV_EXT4"; mkdir -p "$GCOV_EXT4";
export GCOV_PREFIX="$GCOV_EXT4";
# 清旧 .gcda(原位置 + 重定向位置), 保证覆盖数据来自本次用例。
find "$BUILD_DIR" -name "*.gcda" -delete 2>/dev/null || true;
find "$GCOV_EXT4" -name "*.gcda" -delete 2>/dev/null || true;

# ---------- 跑 LLT 用例 ----------
# testlist 的 EXCLUDE 已排除依赖 cjc 的 6 个 LLT 用例。
if [[ -d "$LLT_DIR" ]]; then
    echo "=== 跑 LLT 用例: $LLT_DIR ===";
    python3 "$FRAMEWORK/main.py" \
      --test_list="$LLT_DIR/testlist" \
      --test_cfg="$LLT_DIR/test.cfg" \
      -pFAIL --fail-verbose -j4 --timeout=60 \
      "$LLT_DIR" || true;
fi

# ---------- 跑 HLT 用例 ----------
# 系统已有 cjc + CANGJIE_HOME, HLT 能跑真实采样用例。
if [[ -d "$HLT_DIR" ]]; then
    echo "=== 跑 HLT 用例: $HLT_DIR ===";
    python3 "$FRAMEWORK/main.py" \
      --test_list="$HLT_TESTLIST" \
      --test_cfg="$HLT_CFG" \
      -pFAIL --fail-verbose -j4 --timeout=180 \
      "$HLT_DIR" || echo "(HLT 部分用例失败，继续出报告)";
fi

# ---------- 采集 + 出报告 ----------
# 拷回 ext4 上的 .gcda 到原构建目录(与 .gcno 同位), 供 lcov 采集。
# GCOV_PREFIX 机制: 原 .gcda 绝对路径 $BUILD_DIR/... 被写到
# $GCOV_PREFIX${BUILD_DIR#/}/... (剥掉前导 / 拼到 prefix 后), 所以映射源是
# $GCOV_PREFIX$BUILD_DIR/。
unset GCOV_PREFIX;
# 确保没有遗留的 cjprof 进程占着 .gcda (否则拷回时 Text file busy)。
pkill -f "build/.*/src/cjprof" 2>/dev/null || true;
sleep 1;
GCDA_SRC="$GCOV_PREFIX$BUILD_DIR";
if [[ -d "$GCDA_SRC" ]]; then
    # 先清原位置的旧 .gcda, 再拷回 ext4 上的新数据。
    find "$BUILD_DIR" -name "*.gcda" -delete 2>/dev/null || true;
    cp -a "$GCDA_SRC/." "$BUILD_DIR/" 2>/dev/null || true;
fi

# 屏蔽列表: 第三方、系统头、构建中间产物, 以及无对应测试的代码:
#   Cjprof.cpp JNI API      -> 仅 Java/JNI bridge 调用, 当前无 Java 用例
#   LinuxRecorder/Record/Elf-> 依赖系统 perf 权限, CI 不具备
#   FlameGraph/FlatReporter -> 依赖真实 perf 采样数据
EXCLUDE_PATTERNS=(
    '*/third_party/*'
    '/usr/lib*'
    '/usr/include*'
    '/usr/local*'
    '*/c++/*'
    '*/CMakeFiles/*'
    '*/src/Cjprof.cpp'
    '*/include/Cjprof.h'
    '*/src/Recorder/LinuxRecorder.cpp'
    '*/src/Commands/Record.cpp'
    '*/src/Symbol/Elf.cpp'
    '*/src/Reporter/FlameGraph.cpp'
    '*/src/Reporter/FlatReporter.cpp'
    '*/include/Reporter/FlameGraph.h'
    '*/include/Reporter/FlatReporter.h'
    '*/include/Recorder/Recorder.h'
    '*/include/Symbol/*'
);

mkdir -p "$REPORT_DIR";
lcov --gcov-tool="$GCOV_SH" --rc lcov_branch_coverage=1 \
     --directory "$BUILD_DIR" \
     --capture --output-file "$REPORT_DIR/cjprof.raw.info";

lcov --gcov-tool="$GCOV_SH" --rc lcov_branch_coverage=1 \
     --remove "$REPORT_DIR/cjprof.raw.info" \
     "${EXCLUDE_PATTERNS[@]}" \
     --output-file "$REPORT_DIR/cjprof.info";

genhtml --branch-coverage --function-coverage \
        --output-directory "$REPORT_DIR/html" \
        "$REPORT_DIR/cjprof.info";

# 打印最终覆盖率
echo "=== 最终覆盖率 ===";
lcov --rc lcov_branch_coverage=1 --summary "$REPORT_DIR/cjprof.info" || true;

ls "$REPORT_DIR/html/index.html";
