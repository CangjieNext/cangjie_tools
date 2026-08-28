#!/bin/bash
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit
echo '=== compoundAssignSample occurrences in 011.test (didOpen text snapshot) ==='
grep -o 'compoundAssignSample[^\\]*' IntroduceParameter_Apply_011.test | head -4
echo
echo '=== func signature in didOpen text ==='
grep -o 'func compoundAssignSample(a[^\\]*' IntroduceParameter_Apply_011.test | head -2
echo
echo '=== didOpen text line count check: number of \\n in text field ==='
python3 - <<'PYEOF'
import json
for line in open('IntroduceParameter_Apply_011.test', errors='replace'):
    line = line.strip()
    if not line or 'didOpen' not in line:
        continue
    obj = json.loads(line)
    text = obj['params']['textDocument']['text']
    lines = text.split('\n')
    print(f'didOpen text: {len(lines)} lines')
    # find compoundAssignSample in the text
    for i, ln in enumerate(lines):
        if 'compoundAssignSample' in ln:
            print(f'  line {i} (0-based): {ln}')
    # context lines 90-105
    print('  --- lines 92-104 (0-based) ---')
    for i in range(92, min(105, len(lines))):
        print(f'  {i:>3}: {lines[i]}')
    break
PYEOF
