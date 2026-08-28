#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server

echo "=== git log of introduce_parameter.cj (last 5 commits) ==="
git log --oneline -5 -- test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj
echo
echo "=== when was 'a += 2' (compoundAssignSample) introduced? ==="
git log -S 'a += 2' --oneline -- test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj | tail -3
echo
echo "=== when was IntroduceParameter_Apply_011.base recorded? ==="
git log --oneline -3 -- test/message/applyEdit/IntroduceParameter_Apply_011.base
echo
echo "=== does passing case have diagnostics? run case 0 (ExtractMethod_Apply_003, PASS) ==="
cd output/bin
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/0' > /tmp/t0.log 2>&1
grep -cE '^\[       OK \]' /tmp/t0.log
echo "-- diagnostics from LSP:"
grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -5
echo "-- executeCommand response:"
grep -oE '\{"id":"[0-9]+","jsonrpc":"2\.0","result":[^,}]*' applyEdit_freopen.out | tail -1 | head -c 60
