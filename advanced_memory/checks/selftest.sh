#!/bin/bash
# Self-test of every check that has fixtures: each must PASS every fixtures/<name>.right* (an
# alternative correct spelling is .right2, .right3 ...), and FAIL every fixtures/<name>.wrong*
# (plausible mistakes and adversarial answers) and an empty answer. Then:
#   - regex cards: every case in fixtures/regex_cards.tsv is graded with the card's real
#     AcceptIf/RejectIf from lessons/ through drive.eval_predicate;
#   - the library regression tests: bash ../tests/run.sh.
# Ignores stdin. Exit 0 only if everything behaves.
# (Same mechanism as ../boost_multi_index/checks/selftest.sh.)
cd "$(dirname "$0")" || exit 1
bad=0
for right in fixtures/*.right; do
    n=$(basename "$right" .right)
    r=0; w=0
    bash "$n.sh" < "$right" >/dev/null 2>&1; r=$?
    for wrong in fixtures/"$n".wrong*; do
        bash "$n.sh" < "$wrong" >/dev/null 2>&1; rc=$?
        if [ $rc -eq 0 ]; then w=0; echo "FAIL $n accepted $(basename "$wrong")"; bad=1; break; fi
        w=$rc
    done
    bash "$n.sh" < /dev/null >/dev/null 2>&1; e=$?
    if [ $r -eq 0 ] && [ $w -ne 0 ] && [ $e -ne 0 ]; then echo "ok   $n"; else echo "FAIL $n right=$r wrong=$w empty=$e"; bad=1; fi
done
for r2 in fixtures/*.right[0-9]*; do
    [ -e "$r2" ] || continue
    n=$(basename "${r2%.right*}"); tag=${r2##*.}
    bash "$n.sh" < "$r2" >/dev/null 2>&1; r=$?
    if [ $r -eq 0 ]; then echo "ok   $n ($tag)"; else echo "FAIL $n $tag=$r"; bad=1; fi
done

# Regex cards (lessons 01-07): case table -> the card's own predicates.
python3 - fixtures/regex_cards.tsv <<'PY' || bad=1
import sys
sys.path.insert(0, '..')
import drive, tutorial_format
cards = {c['id']: c for d in tutorial_format.parse_lessons('../lessons') for c in d['cards']}
bad = 0
for line in open(sys.argv[1], encoding='utf-8'):
    if not line.strip() or line.startswith('#'):
        continue
    cid, want, answer = line.rstrip('\n').split('\t')
    c = cards[cid]
    if not c['AcceptIf'] or c['AcceptIf'].startswith('check('):
        print(f'FAIL {cid}: no regex AcceptIf'); bad = 1; continue
    rej = bool(c['RejectIf']) and drive.eval_predicate(c['RejectIf'], answer, '..')
    got = (not rej) and drive.eval_predicate(c['AcceptIf'], answer, '..')
    ok = got == (want == 'accept')
    print(('ok  ' if ok else 'FAIL') + f' regex {cid} {want:6} {answer!r}')
    bad |= not ok
sys.exit(bad)
PY

# Library regression tests (labs/hft_mem/hft_mem.hpp).
bash ../tests/run.sh || bad=1
exit $bad
