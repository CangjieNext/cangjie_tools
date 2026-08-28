#!/bin/bash
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test

echo "=== 011.test: executeCommand request (what it asks LSP to do) ==="
python3 - <<'PYEOF'
import re
content = open('message/applyEdit/IntroduceParameter_Apply_011.test', errors='replace').read()
m = re.search(r'"method":"workspace/executeCommand","params":(\{.*\})', content)
if m:
    print(m.group(1)[:400])
else:
    # find line with executeCommand
    for line in content.split('\n'):
        if 'executeCommand' in line:
            print(line[:500])
            break
PYEOF

echo
echo "=== 011.base: expected applyEdit (full) ==="
cat message/applyEdit/IntroduceParameter_Apply_011.base
echo
echo "=== current source: lines 90-105 of introduce_parameter.cj ==="
sed -n '90,105p' testChr/refactor/src/IntroduceParameter/introduce_parameter.cj
echo
echo "=== how many .test files use compoundAssignSample region (line 97-101)? ==="
grep -l '"line":9[78]' message/applyEdit/IntroduceParameter_Apply_*.test 2>/dev/null | head
