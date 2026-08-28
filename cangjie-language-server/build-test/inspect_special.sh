#!/bin/bash
# Inspect suspicious cases whose selection points at comment lines
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit
for n in 015 018 019 020 021 022; do
    echo "=== $n.base ==="
    head -c 350 IntroduceParameter_Apply_$n.base 2>/dev/null
    echo
done
echo "=== also check what snapshot line these selections point to ==="
python3 - <<'PYEOF'
import json
for n in ['015', '018', '019', '020', '021', '022']:
    for line in open(f'IntroduceParameter_Apply_{n}.test', errors='replace'):
        if 'workspace/executeCommand' in line:
            obj = json.loads(line)
            args = obj['params']['arguments'][0]
            sel = args.get('selection', {})
            sl = sel.get('start', {}).get('line', -1)
            sc = sel.get('start', {}).get('character', -1)
            ec = sel.get('end', {}).get('character', -1)
            el = sel.get('end', {}).get('line', sl)
            extra = args.get('extraOptions', {})
            print(f'{n}: sel=({sl}:{sc})-({el}:{ec}) extra={extra}')
            break
PYEOF
echo
echo "=== snapshot text at those lines (from 015.test didOpen) ==="
python3 - <<'PYEOF'
import json
for line in open('IntroduceParameter_Apply_015.test', errors='replace'):
    if 'didOpen' in line:
        obj = json.loads(line)
        text = obj['params']['textDocument']['text'].split('\n')
        for n, sl in [('015', 1), ('019', 4), ('020', 3), ('021', 4), ('022', 4)]:
            if sl < len(text):
                print(f'  {n}: snapshot[{sl}]: {text[sl][:70]}')
        break
PYEOF
