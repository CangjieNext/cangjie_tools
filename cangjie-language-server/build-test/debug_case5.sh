#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin
./gtest_LSPServer_test --gtest_filter='ApplyEdit/ApplyEditTest.ApplyEditCase/5' > /tmp/case5.log 2>&1
echo '=== gtest result ==='
grep -E '^\[  FAILED  \]|^\[       OK \]' /tmp/case5.log | head -2
echo '=== LSP response methods ==='
grep -oE '"method":"[^"]*"' applyEdit_freopen.out | sort | uniq -c
echo '=== diagnostics messages ==='
grep -oE '"message":"[^"]*"' applyEdit_freopen.out | sort | uniq -c | head -10
echo '=== applyEdit changes in LSP out (uris + textEdit count) ==='
python3 - <<'PYEOF'
import re
out = open('applyEdit_freopen.out', errors='replace').read()
# find applyEdit message
for m in re.finditer(r'"method":"workspace/applyEdit","params":(\{.*?\})\}\s*$', out, re.M):
    pass
# simpler: extract uri keys and newText occurrences
uris = re.findall(r'"(file://[^"]*?)":\[\{', out)
print("changed files:", uris)
newtexts = re.findall(r'"newText":"((?:[^"\\]|\\.)*)"', out)
print(f"newText count: {len(newtexts)}")
for t in newtexts[:8]:
    print("  newText:", t[:120].replace('\\n', ' | '))
PYEOF
echo '=== expect (base) newTexts ==='
python3 - <<'PYEOF'
import re
base = open('/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit/ExtractInterface_Apply_002.base', errors='replace').read()
newtexts = re.findall(r'"newText":"((?:[^"\\]|\\.)*)"', base)
print(f"expect newText count: {len(newtexts)}")
for t in newtexts[:8]:
    print("  expect:", t[:120].replace('\\n', ' | '))
PYEOF
