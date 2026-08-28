#!/bin/bash
# List failed cases with test file names; group by refactor feature
python3 - <<'PYEOF'
import re
from collections import Counter, defaultdict

cases = [l.split('\t') for l in open('/tmp/cases2.txt').read().splitlines() if l.strip()]
fails = [(s, i, r, f) for s, i, st, r, f, res in cases if st == 'FAILED']
oks = [(s, i, r, f) for s, i, st, r, f, res in cases if st == 'OK']

print(f"== FAILED list ({len(fails)}) ==")
by_reason = defaultdict(list)
for s, i, r, f in fails:
    by_reason[r[:60]].append((s, i, f))
for reason, items in sorted(by_reason.items()):
    print(f"\n--- {reason} ({len(items)}) ---")
    for s, i, f in sorted(items, key=lambda x: (x[0], int(x[1]))):
        print(f"  [{s} {i:>2}] {f}")

print("\n== feature prefix distribution (FAILED vs OK) ==")
feat_fail = Counter(f.split('_')[0] for s, i, r, f in fails)
feat_ok = Counter(f.split('_')[0] for s, i, r, f in oks)
all_feats = sorted(set(feat_fail) | set(feat_ok))
print(f"  {'feature':<28} {'OK':>4} {'FAIL':>4}")
for feat in all_feats:
    print(f"  {feat:<28} {feat_ok.get(feat,0):>4} {feat_fail.get(feat,0):>4}")
PYEOF
echo "=== freopen .err (this run) ==="
ls -la /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin/*freopen*.err 2>/dev/null
cat /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin/applyEdit_freopen.err 2>/dev/null
cat /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin/codeAction_freopen.err 2>/dev/null
