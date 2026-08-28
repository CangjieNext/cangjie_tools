#!/bin/bash
# Survey: for each IntroduceParameter_Apply .test, extract selection range,
# selected text (from didOpen snapshot) and disk file line content
python3 - <<'PYEOF'
import json, os, re

msg_dir = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit'
src = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj'
disk = open(src, errors='replace').read().split('\n')

for name in sorted(os.listdir(msg_dir)):
    if not name.startswith('IntroduceParameter_Apply_') or not name.endswith('.test'):
        continue
    case = name.replace('IntroduceParameter_Apply_', '').replace('.test', '')
    for line in open(os.path.join(msg_dir, name), errors='replace'):
        if 'workspace/executeCommand' not in line:
            continue
        try:
            obj = json.loads(line)
            args = obj['params']['arguments'][0]
        except Exception:
            continue
        sel = args.get('selection', {})
        sl, sc = sel.get('start', {}).get('line', -1), sel.get('start', {}).get('character', -1)
        el, ec = sel.get('end', {}).get('line', -1), sel.get('end', {}).get('character', -1)
        # disk content at that line
        dcontent = disk[sl][:60] if 0 <= sl < len(disk) else '<out of range>'
        # snapshot text
        print(f'{case}: sel=({sl}:{sc})-({el}:{ec}) tweakID={args.get("tweakID")}')
        print(f'    disk[{sl}]: {dcontent}')
        break
PYEOF
