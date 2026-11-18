#!/bin/sh
# Self-test of every check: each must PASS its fixtures/<name>.right answer and
# FAIL its fixtures/<name>.wrong answer (a plausible mistake) and an empty answer.
# Ignores stdin. Exit 0 only if every check behaves.
cd "$(dirname "$0")" || exit 1
bad=0
for right in fixtures/*.right; do
  n=$(basename "$right" .right)
  ./"$n".sh < "$right" >/dev/null 2>&1; r=$?
  ./"$n".sh < "fixtures/$n.wrong" >/dev/null 2>&1; w=$?
  ./"$n".sh < /dev/null >/dev/null 2>&1; e=$?
  if [ $r -eq 0 ] && [ $w -ne 0 ] && [ $e -ne 0 ]; then echo "ok   $n"; else echo "FAIL $n right=$r wrong=$w empty=$e"; bad=1; fi
done
# Extra accepted spellings: fixtures/<name>.right2 must also PASS (guards against false rejects).
for r2 in fixtures/*.right2; do
  [ -e "$r2" ] || continue
  n=$(basename "$r2" .right2)
  ./"$n".sh < "$r2" >/dev/null 2>&1; r=$?
  if [ $r -eq 0 ]; then echo "ok   $n (right2)"; else echo "FAIL $n right2=$r"; bad=1; fi
done
exit $bad
