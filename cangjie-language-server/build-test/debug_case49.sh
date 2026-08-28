#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/49' > /tmp/case49.log 2>&1
echo '=== gtest result ==='
grep -E '^\[  FAILED  \]|^\[       OK \]' /tmp/case49.log
echo '=== err ==='
cat applyEdit_freopen.err
echo '=== LSP out: message types ==='
grep -oE '"method":"[^"]*"' applyEdit_freopen.out | sort | uniq -c
echo '=== out tail (last 900 chars) ==='
tail -c 900 applyEdit_freopen.out
echo
echo '=== the request sent (executeCommand) ==='
grep -o 'executeCommand.*' applyEdit_freopen.in | head -c 500
