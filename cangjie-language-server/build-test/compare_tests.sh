#!/bin/bash
cd /home/savage/cppProjects/cangjie_tools/cangjie-language-server/test/message/applyEdit
echo '=== method sequence diff: 010 (immutable-fail) vs 011 (not-initialized-fail) ==='
grep -oE '"method":"[^"]*"' IntroduceParameter_Apply_010.test > /tmp/m010.txt
grep -oE '"method":"[^"]*"' IntroduceParameter_Apply_011.test > /tmp/m011.txt
diff /tmp/m010.txt /tmp/m011.txt && echo '(same methods)'
echo '=== id sequence 010 vs 011 ==='
grep -oE '"id":[0-9]+' IntroduceParameter_Apply_010.test | tr '\n' ' '; echo
grep -oE '"id":[0-9]+' IntroduceParameter_Apply_011.test | tr '\n' ' '; echo
echo '=== which cases are "not-initialized" (52,53,57,59-63 = 011,012,016,018-022) vs immutable (002-010,013-015) ==='
echo '=== file sizes ==='
wc -c IntroduceParameter_Apply_002.test IntroduceParameter_Apply_010.test IntroduceParameter_Apply_011.test IntroduceParameter_Apply_012.test IntroduceParameter_Apply_016.test IntroduceParameter_Apply_018.test IntroduceParameter_Apply_022.test 2>/dev/null
echo '=== head of 011.test (first 700 bytes) ==='
head -c 700 IntroduceParameter_Apply_011.test
echo
echo '=== Content-Length headers in 010 vs 011 ==='
grep -aoE 'Content-Length:[0-9]+' IntroduceParameter_Apply_010.test | tr '\n' ' '; echo
grep -aoE 'Content-Length:[0-9]+' IntroduceParameter_Apply_011.test | tr '\n' ' '; echo
