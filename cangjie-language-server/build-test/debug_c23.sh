#!/bin/bash
source /home/savage/sdk/cangjie-sdk-linux-x64-1.2.0-alpha.20260618020030/cangjie/envsetup.sh >/dev/null 2>&1
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/output/bin
./gtest_LSPServer_test --gtest_filter='CodeAction/CodeActionTest.CodeActionCase/23' > /tmp/c23.log 2>&1
echo '=== expect titles (base) ==='
python3 - <<'PYEOF'
import re
base = open('/home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/codeAction/IntroduceField_Constraint_004.base', errors='replace').read()
titles = re.findall(r'"title":"((?:[^"\\]|\\.)*)"', base)
for i, t in enumerate(titles):
    print(f"  [{i}] {t}")
print('  kinds:', re.findall(r'"kind":"([^"]*)"', base))
PYEOF
echo '=== actual titles (LSP out) ==='
python3 - <<'PYEOF'
import re, json
out = open('codeAction_freopen.out', errors='replace').read()
# find the codeAction response (id from request)
bodies = re.findall(r'\{"id":[^{}]*"result":\[.*?\]', out, re.S)
for b in bodies:
    try:
        obj = json.loads(b[:b.rfind(']')+1] if b.endswith(']') else b)
    except Exception:
        pass
titles = re.findall(r'"title":"((?:[^"\\]|\\.)*)"', out)
for i, t in enumerate(titles):
    print(f"  [{i}] {t}")
print('  kinds:', re.findall(r'"kind":"([^"]*)"', out))
PYEOF
echo '=== source file diagnostics ==='
grep -oE '"message":"[^"]*"' codeAction_freopen.out | grep -v indexed | head -5
