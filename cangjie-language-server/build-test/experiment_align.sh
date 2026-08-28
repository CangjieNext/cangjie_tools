#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
T=/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit/IntroduceParameter_Apply_011.test
BIN=/home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin

echo "=== EXPERIMENT 3: realign 011.selection to disk (a <<= 2 at 0-based line 100) ==="
cp "$T" /tmp/011.test.orig3
python3 - <<'PYEOF'
import json
path = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit/IntroduceParameter_Apply_011.test'
out = []
for line in open(path, errors='replace'):
    if 'workspace/executeCommand' in line:
        obj = json.loads(line)
        obj['params']['arguments'][0]['selection'] = {
            "start": {"line": 100, "character": 4},
            "end": {"line": 100, "character": 11}
        }
        line = json.dumps(obj, ensure_ascii=False) + '\n'
        print('selection realigned to disk line 100 (a <<= 2)')
    out.append(line)
open(path, 'w').write(''.join(out))
PYEOF

cd $BIN
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/52' > /tmp/exp52c.log 2>&1
echo "-- applyEdit notification count:"
grep -c '"method":"workspace/applyEdit"' applyEdit_freopen.out
echo "-- diagnostics:"
grep -oE '"message":"[^"]*"' applyEdit_freopen.out | grep -vE 'packages indexed|indexing' | sort -u | head -4
echo "-- verdict:"
grep -E '^\[  FAILED  \]|^\[       OK \]' /tmp/exp52c.log | head -1
echo "-- false reason (if any):"
grep -oE 'the false reason is : .*' /tmp/exp52c.log | head -1 | cut -c25-100
echo "-- actual applyEdit params (if produced):"
grep -o '"method":"workspace/applyEdit".*' applyEdit_freopen.out | head -c 600

echo
echo "=== restore ==="
cp /tmp/011.test.orig3 "$T"
echo restored
