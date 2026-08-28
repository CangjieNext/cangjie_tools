#!/bin/bash
# Parse /tmp/lsp_test.log: per-case (suite, idx, reason, testfile, result)
python3 - <<'PYEOF'
import re
from collections import Counter

log = open('/tmp/lsp_test.log', errors='replace').read()
entries = re.split(r'\[ RUN      \] ', log)
results = []
for e in entries[1:]:
    lines = e.split('\n')
    header = lines[0].strip()
    m = re.match(r'(ApplyEdit|CodeAction)/\w+\.(\w+)/(\d+)', header)
    if not m:
        continue
    suite, case_idx = m.group(1), m.group(3)
    reason = result = testfile = ''
    for ln in lines[1:]:
        if ln.startswith('the false reason is : '):
            reason = ln[len('the false reason is : '):]
        elif ln.startswith('result='):
            result = ln[len('result='):]
        elif ln.startswith('file:////') and '.test' in ln:
            testfile = ln.split('/')[-1]
    results.append((suite, case_idx, reason, testfile, result))

print(f"total parsed: {len(results)}")
print("\n== by (suite, reason) ==")
for k, v in sorted(Counter((r[0], r[2]) for r in results).items()):
    print(f"  {k}: {v}")
print("\n== result value distribution (first 30 chars) ==")
for k, v in sorted(Counter((r[0], r[4][:30]) for r in results).items()):
    print(f"  {k}: {v}")
with open('/tmp/cases.txt', 'w') as f:
    for r in results:
        f.write('\t'.join(r) + '\n')
print("\nsaved /tmp/cases.txt")
PYEOF
