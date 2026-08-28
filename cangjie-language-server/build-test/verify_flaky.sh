#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin
echo "=== rerun case 52 x3 (flaky check) ==="
for n in 1 2 3; do
    ./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/52' > /tmp/t52_$n.log 2>&1
    st=$(grep -cE '^\[       OK \]' /tmp/t52_$n.log)
    diag=$(grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -2 | tr '\n' ';')
    resp=$(grep -oE '\{"id":"[0-9]+","jsonrpc":"2\.0","result":[^}]*' applyEdit_freopen.out | tail -1 | head -c 50)
    echo "run$n: OK=$st diag=[$diag] resp=[$resp]"
done
echo
echo "=== rerun case 61 x2 ==="
for n in 1 2; do
    ./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/61' > /tmp/t61_$n.log 2>&1
    st=$(grep -cE '^\[       OK \]' /tmp/t61_$n.log)
    diag=$(grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -2 | tr '\n' ';')
    echo "run$n: OK=$st diag=[$diag]"
done
echo
echo "=== introduce_parameter.cj: the immutable source ==="
grep -n "a =" /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj | head -8
echo "=== func signature around line 97-101 ==="
sed -n '95,103p' /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj
echo "=== package decl ==="
head -8 /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj | grep -n package
