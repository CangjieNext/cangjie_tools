#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin

echo "=== case 0 (PASS): has applyEdit notification? ==="
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/0' > /tmp/t0b.log 2>&1
grep -c '"method":"workspace/applyEdit"' applyEdit_freopen.out

echo "=== case 24 (FAIL, only package hint): applyEdit notification? ==="
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/24' > /tmp/t24.log 2>&1
grep -c '"method":"workspace/applyEdit"' applyEdit_freopen.out
echo "-- all diagnostics:"
grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u
echo "-- source file head (extract_interface_interface_all.cj):"
sed -n '1,30p' /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/testChr/refactor/src/ExtractInterface/extract_interface_interface_all.cj
