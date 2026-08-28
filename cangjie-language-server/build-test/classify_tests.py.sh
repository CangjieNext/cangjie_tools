#!/bin/bash
# Classify .test files by rootUri / didOpen uri path features
python3 - <<'PYEOF'
import re, os
from collections import Counter

root = '/home/savage/cppProjects/cangjie_tools/cangjie-language-server'
cases = [l.split('\t') for l in open('/tmp/cases.txt').read().splitlines() if l.strip()]

def classify_uri(uri):
    if not uri:
        return 'EMPTY'
    if 'wangjunwei' in uri:
        return 'wangjunwei-linux'
    if 'd%3A' in uri or 'd:\\\\' in uri or 'D%3A' in uri:
        return 'windows-d-project'
    if 'savage' in uri:
        return 'current-env'
    if uri.startswith('file:') or uri.startswith('/'):
        return 'other-abs'
    return 'other'

cat_counter = Counter()
detail = []
for suite, idx, reason, testfile, result in cases:
    folder = 'applyEdit' if suite == 'ApplyEdit' else 'codeAction'
    p = os.path.join(root, 'test', 'message', folder, testfile)
    if not os.path.exists(p):
        detail.append((suite, idx, 'FILE_NOT_FOUND', testfile))
        cat_counter[(suite, 'FILE_NOT_FOUND')] += 1
        continue
    content = open(p, errors='replace').read()
    m = re.search(r'"rootUri":"([^"]*)"', content)
    rooturi = m.group(1) if m else ''
    # didOpen uris
    opens = re.findall(r'"textDocument":\s*\{[^}]*"uri":"([^"]*)"', content)
    # file/uri fields in requests
    uris = re.findall(r'"(?:uri|file)":"([^"]*)"', content)
    all_uris = [rooturi] + opens + uris
    cats = set(classify_uri(u) for u in all_uris if u)
    cat = '+'.join(sorted(cats)) if cats else 'NO_URI'
    cat_counter[(suite, cat)] += 1
    detail.append((suite, idx, cat, testfile))

print("== failure classification (suite, uri-category) ==")
for k, v in sorted(cat_counter.items()):
    print(f"  {k}: {v}")

print("\n== per-suite examples ==")
seen = set()
for suite, idx, cat, testfile in detail:
    key = (suite, cat)
    if key not in seen:
        seen.add(key)
        print(f"  [{suite}] {cat}: case {idx} -> {testfile}")

with open('/tmp/case_detail.txt', 'w') as f:
    for d in detail:
        f.write('\t'.join(d) + '\n')
print("\nsaved /tmp/case_detail.txt")
PYEOF
