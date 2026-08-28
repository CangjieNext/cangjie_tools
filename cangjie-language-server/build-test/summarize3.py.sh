#!/bin/bash
python3 - <<'PYEOF'
import re
from collections import Counter

log = open('/tmp/lsp_test3.log', errors='replace').read()
entries = re.split(r'\[ RUN      \] ', log)
results = []
for e in entries[1:]:
    lines = e.split('\n')
    m = re.match(r'(ApplyEdit|CodeAction)/\w+\.(\w+)/(\d+)', lines[0].strip())
    if not m:
        continue
    suite, case_idx = m.group(1), m.group(3)
    ok = any(l.startswith('[       OK ]') for l in lines)
    reason = testfile = ''
    for ln in lines:
        if ln.startswith('the false reason is : '):
            reason = ln[len('the false reason is : '):][:55]
        elif ln.startswith('file:////') and '.test' in ln:
            testfile = ln.split('/')[-1]
    results.append((suite, case_idx, 'OK' if ok else 'FAILED', reason, testfile))

n_ok = sum(1 for r in results if r[2] == 'OK')
n_fail = sum(1 for r in results if r[2] == 'FAILED')
print(f"total: {len(results)}, PASS: {n_ok}, FAIL: {n_fail}")
print("\n== per suite ==")
for k, v in sorted(Counter((r[0], r[2]) for r in results).items()):
    print(f"  {k}: {v}")
print("\n== failure reasons ==")
for k, v in sorted(Counter((r[0], r[3]) for r in results if r[2] == 'FAILED').items()):
    print(f"  {k}: {v}")
print("\n== gtest summary ==")
for l in log.split('\n'):
    if l.startswith('[==========]') and 'ran' in l or l.startswith('[  PASSED  ]') or l.startswith('[  FAILED  ]') and 'test' in l:
        print(l)
PYEOF
