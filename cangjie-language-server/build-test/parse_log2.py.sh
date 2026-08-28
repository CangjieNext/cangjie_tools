#!/bin/bash
# Parse lsp_test2.log: classify all failures
python3 - <<'PYEOF'
import re
from collections import Counter

log = open('/tmp/lsp_test2.log', errors='replace').read()
entries = re.split(r'\[ RUN      \] ', log)
results = []
for e in entries[1:]:
    lines = e.split('\n')
    header = lines[0].strip()
    m = re.match(r'(ApplyEdit|CodeAction)/\w+\.(\w+)/(\d+)', header)
    if not m:
        continue
    suite, case_idx = m.group(1), m.group(3)
    ok = any(l.startswith('[       OK ]') for l in lines)
    failed = any(l.startswith('[  FAILED  ]') for l in lines)
    reason = result = testfile = ''
    for ln in lines:
        if ln.startswith('the false reason is : '):
            reason = ln[len('the false reason is : '):]
        elif ln.startswith('result='):
            result = ln[len('result='):]
        elif ln.startswith('file:////') and '.test' in ln:
            testfile = ln.split('/')[-1]
    status = 'OK' if ok else ('FAILED' if failed else 'UNKNOWN')
    results.append((suite, case_idx, status, reason, testfile, result[:40]))

n_ok = sum(1 for r in results if r[2] == 'OK')
n_fail = sum(1 for r in results if r[2] == 'FAILED')
print(f"total: {len(results)}, OK: {n_ok}, FAILED: {n_fail}")
print("\n== failure reasons ==")
for k, v in sorted(Counter((r[0], r[3][:80]) for r in results if r[2] == 'FAILED').items()):
    print(f"  {k}: {v}")
print("\n== per-suite OK/FAILED ==")
for k, v in sorted(Counter((r[0], r[2]) for r in results).items()):
    print(f"  {k}: {v}")
with open('/tmp/cases2.txt', 'w') as f:
    for r in results:
        f.write('\t'.join(r) + '\n')
print("\nsaved /tmp/cases2.txt")

# print summary tail
tail = [l for l in log.split('\n') if l.startswith('[==========]') or l.startswith('[  PASSED  ]') or l.startswith('[  FAILED  ]')]
print('\n'.join(tail[-5:]))
PYEOF
