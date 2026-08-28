#!/bin/bash
# Fix subtype-B test files: insert "initialized" notification after the "initialize" line
python3 - <<'PYEOF'
import os

d = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit'
files = [
    'IntroduceParameter_Apply_011.test',
    'IntroduceParameter_Apply_012.test',
    'IntroduceParameter_Apply_016.test',
    'IntroduceParameter_Apply_018.test',
    'IntroduceParameter_Apply_019.test',
    'IntroduceParameter_Apply_020.test',
    'IntroduceParameter_Apply_021.test',
    'IntroduceParameter_Apply_022.test',
]
INIT = b'{"jsonrpc":"2.0","method":"initialized","params":{}}'

for name in files:
    p = os.path.join(d, name)
    data = open(p, 'rb').read()
    eol = b'\r\n' if b'\r\n' in data else b'\n'
    lines = data.split(eol)
    # skip if already has initialized notification
    if any(b'"method":"initialized"' in ln for ln in lines):
        print(f'{name}: already has initialized, skip')
        continue
    out = []
    inserted = False
    for ln in lines:
        out.append(ln)
        if (not inserted and b'"method":"initialize"' in ln
                and b'"method":"initialized"' not in ln):
            out.append(INIT)
            inserted = True
    if not inserted:
        print(f'{name}: initialize line NOT FOUND, skip')
        continue
    open(p, 'wb').write(eol.join(out))
    print(f'{name}: inserted initialized (eol={"CRLF" if eol==b"\\r\\n" else "LF"})')
print('done')
PYEOF
