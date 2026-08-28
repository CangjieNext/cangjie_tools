#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin

echo -e "case\tstatus\treason\tapplyEdit\tdiag"
for i in 52 53 57 59 60 61 62 63; do
    ./gtest_LSPServer_test --gtest_filter="ApplyEdit/ApplyEditTest.ApplyEditCase/$i" > /tmp/r_$i.log 2>&1
    ok=$(grep -cE '^\[       OK \]' /tmp/r_$i.log)
    reason=$(grep -oE 'the false reason is : .*' /tmp/r_$i.log | head -1 | cut -c25-60)
    ae=$(grep -c '"method":"workspace/applyEdit"' applyEdit_freopen.out)
    diag=$(grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -2 | grep -oE "immutable|server not initialized|package name" | tr '\n' ',')
    echo -e "$i\t$([ $ok -ge 1 ] && echo PASS || echo FAIL)\t$reason\t$ae\t$diag"
done
