#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
T=/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit/IntroduceParameter_Apply_011.test
BIN=/home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin

echo "=== EXPERIMENT: add 'var' to parameter in didOpen text (remove immutable error) ==="
cp "$T" /tmp/011.test.orig
python3 - <<'PYEOF'
import json
path = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit/IntroduceParameter_Apply_011.test'
lines_out = []
for line in open(path, errors='replace'):
    if 'didOpen' in line:
        obj = json.loads(line)
        text = obj['params']['textDocument']['text']
        newtext = text.replace('func compoundAssignSample(a: Int64)', 'func compoundAssignSample(var a: Int64)')
        if newtext != text:
            obj['params']['textDocument']['text'] = newtext
            line = json.dumps(obj, ensure_ascii=False) + '\n'
            print('patched didOpen text: a -> var a')
    lines_out.append(line)
open(path, 'w').write(''.join(lines_out))
PYEOF

cd $BIN
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/52' > /tmp/exp52.log 2>&1
echo "-- applyEdit notification count:"
grep -c '"method":"workspace/applyEdit"' applyEdit_freopen.out
echo "-- immutable diagnostics now:"
grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -4
echo "-- test verdict:"
grep -E '^\[  FAILED  \]|^\[       OK \]' /tmp/exp52.log | head -1
echo "-- false reason (if any):"
grep -oE 'the false reason is : .*' /tmp/exp52.log | head -1 | cut -c25-90

echo
echo "=== restore original .test ==="
cp /tmp/011.test.orig "$T"
echo restored
