#!/bin/bash
# Precheck: for each failed case, extract selected snippet from snapshot,
# find its new position in the disk file
python3 - <<'PYEOF'
import json, os, re

msg = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit'
srcroot = '/home/savage/cppProjects/cangjie_tools/cangjie_tools/cangjie-language-server/test'.replace('cangjie_tools/cangjie_tools', 'cangjie_tools')
srcroot = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test'

cases = {
    'IntroduceParameter': ['002','004','005','006','007','008','009','010','011','012','013','014','015','016','018','019','020','021','022'],
    'IntroduceField': ['010','012','013'],
    'ExtractInterface': ['020','022'],
}
srcfiles = {
    'IntroduceParameter': srcroot + '/testChr/refactor/src/IntroduceParameter/introduce_parameter.cj',
    'IntroduceField': srcroot + '/testChr/refactor/src/IntroduceField/introduce_field.cj',
    'ExtractInterface_020': srcroot + '/testChr/refactor/src/ExtractInterface/extract_interface_constructor.cj',
    'ExtractInterface_022': srcroot + '/testChr/refactor/src/ExtractInterface/extract_interface_interface_all.cj',
}

def get_disk(kind, case):
    if kind == 'ExtractInterface':
        return srcfiles[f'{kind}_{case}']
    return srcfiles[kind]

for kind, nums in cases.items():
    for n in nums:
        path = os.path.join(msg, f'{kind}_Apply_{n}.test')
        if not os.path.exists(path):
            print(f'{kind}_{n}: FILE NOT FOUND')
            continue
        diskpath = get_disk(kind, n)
        disk = open(diskpath, errors='replace').read().split('\n')
        snap = sel = None
        for line in open(path, errors='replace'):
            if 'didOpen' in line and 'textDocument' in line:
                obj = json.loads(line)
                snap = obj['params']['textDocument']['text'].split('\n')
            if 'workspace/executeCommand' in line:
                obj = json.loads(line)
                sel = obj['params']['arguments'][0].get('selection', {})
        if snap is None or sel is None:
            print(f'{kind}_{n}: no didOpen/executeCommand')
            continue
        sl, sc = sel['start']['line'], sel['start']['character']
        el, ec = sel['end']['line'], sel['end']['character']
        # snippet from snapshot (handle single/multi-line)
        if sl == el:
            snippet = snap[sl][sc:ec] if sl < len(snap) else '??'
        else:
            snippet = (snap[sl][sc:] + '\n' + '\n'.join(snap[sl+1:el]) + snap[el][:ec]) if el < len(snap) else '??'
        # find in disk (single-line snippet: substring search)
        matches = []
        if '\n' not in snippet and len(snippet.strip()) > 0:
            for i, dl in enumerate(disk):
                c = dl.find(snippet)
                while c != -1:
                    matches.append((i, c))
                    c = dl.find(snippet, c+1)
        print(f'{kind}_{n}: sel=({sl}:{sc})-({el}:{ec}) snippet={snippet[:40]!r} matches={matches[:4]}{"..." if len(matches)>4 else ""}')
PYEOF
