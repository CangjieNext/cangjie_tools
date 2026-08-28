#!/bin/bash
# Batch-run the 24 "no params" failed ApplyEdit cases; collect per-case:
#   source file, LSP diagnostics errors, executeCommand response
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin

CASES="22 24 34 36 37 43 45 46 47 48 49 50 51 52 53 54 55 56 57 59 60 61 62 63"

echo -e "case\ttestfile\tsource\tdiag_errors\texec_response" > /tmp/type1.tsv
for i in $CASES; do
    ./gtest_LSPServer_test --gtest_filter="ApplyEdit/ApplyEditTest.ApplyEditCase/$i" > /tmp/t_$i.log 2>&1
    testfile=$(grep -oE '[A-Za-z_0-9]+\.test' /tmp/t_$i.log | head -1)
    # source .cj file opened
    src=$(grep -oE '"uri":"file://[^"]*\.cj"' applyEdit_freopen.in | head -1 | grep -oE '[A-Za-z_0-9]+\.cj' | head -1)
    # diagnostics errors (exclude progress/index messages)
    errs=$(grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -4 | sed 's/"message":"//;s/"$//' | tr '\n' ';' | head -c 200)
    # executeCommand response (request id with result)
    resp=$(grep -oE '\{"id":"[0-9]+","jsonrpc":"2\.0","result":[^}]*' applyEdit_freopen.out | tail -1 | head -c 60)
    echo -e "$i\t$testfile\t$src\t$errs\t$resp" >> /tmp/type1.tsv
done
column -t -s $'\t' /tmp/type1.tsv
