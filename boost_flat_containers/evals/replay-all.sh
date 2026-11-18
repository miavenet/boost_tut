#!/bin/bash
# Delete-the-model replay: replays every recorded study day in evals/replays/
# (and the delayed assessment) through the real CLI into FRESH progress, with the
# real checks/*.check.sh grading every check() card. Exits non-zero on any driver
# failure, replay mismatch/exhaustion, untaught concept, unanswered card, check()
# card that never passed, or an incomplete assessment.
# Usage (from anywhere): bash evals/replay-all.sh            (needs labs built: bash labs/build.sh)
cd "$(dirname "$0")/.." || exit 2
LIMIT=4
TMP=$(mktemp -d); P="$TMP/progress.json"; LOG="$TMP/out.txt"
trap 'rm -rf "$TMP"' EXIT
fail=0; days=0
for f in evals/replays/????-??-??.replay; do
  d=$(basename "$f" .replay)
  echo "=== study day $d (--new-limit $LIMIT)"
  python3 drive.py lessons --progress "$P" --today "$d" --new-limit $LIMIT --replay "$f" > "$LOG" 2>&1
  rc=$?; cat "$LOG"; echo "rc=$rc"
  [ $rc -ne 0 ] && fail=1
  grep -qE 'Replay exhausted|replay tag mismatch|Traceback' "$LOG" && { echo "!! replay problem on $d"; fail=1; }
  days=$((days+1))
done
for f in evals/replays/????-??-??.assessment.replay; do
  d=$(basename "$f" .assessment.replay)
  echo "=== delayed assessment $d"
  python3 drive.py lessons --progress "$P" --today "$d" --assessment --replay "$f" > "$LOG" 2>&1
  rc=$?; cat "$LOG"; echo "rc=$rc"
  [ $rc -ne 0 ] && fail=1
  grep -qE 'Replay exhausted|replay tag mismatch|Traceback' "$LOG" && { echo "!! replay problem on $d"; fail=1; }
  grep -q 'Assessment complete.' "$LOG" || { echo "!! assessment not complete"; fail=1; }
done
python3 - "$P" "$days" <<'EOF'
import json, sys
sys.path.insert(0, '.')
import tutorial_format as fmt
p = json.load(open(sys.argv[1])); r = p['_runtime']
docs = fmt.parse_lessons('lessons')
labels = [c['id'] for d in docs for c in d['concepts']]
taught = {s.split(':')[2] for s in r['completed_steps'] if s.startswith('teach:')}
cards = {c['id']: c for d in docs for c in d['cards']}
answered = {e['card'] for e in r['attempt_records'] if e.get('mode') == 'T1'}
checks = [cid for cid, c in cards.items() if 'check(' in (c.get('AcceptIf') or '')]
check_hits = {e['card'] for e in r['attempt_records'] if e['card'] in checks and e.get('grade') == 'h'}
misses = [e['card'] for e in r['attempt_records'] if e.get('mode') == 'T1' and e.get('grade') == 'm']
sure_misses = [e['card'] for e in r['attempt_records'] if e.get('hypercorrection')]
flags_left = [c for c in cards if p.get(c, {}).get('hypercorrection')]
boxes = sorted({p[c]['box'] for c in cards if c in p and 'box' in p[c]})
runs = r.get('assessments', {})
done = [x for v in runs.values() for x in v if x.get('complete')]
items = sum(len(x['grades']) for x in done)
untaught = sorted(set(labels) - taught)
print('--- coverage summary')
print(f'concepts taught {len(taught & set(labels))}/{len(labels)}' + (f'  UNTAUGHT {untaught}' if untaught else ''))
print(f'cards answered {len(answered & set(cards))}/{len(cards)}')
print(f'check() cards passing (real checks/) {len(check_hits)}/{len(checks)}')
print(f'misses carried forward {len(misses)} {sorted(set(misses))}; high-confidence (sure) misses {sorted(set(sure_misses))}; flags still open {flags_left}')
print(f'boxes reached {boxes}')
print(f'study days {sys.argv[2]}; assessment runs complete {len(done)} ({items} items graded)')
ok = (not untaught and set(cards) <= answered and len(check_hits) == len(checks)
      and sure_misses and misses and done)
sys.exit(0 if ok else 1)
EOF
[ $? -ne 0 ] && fail=1
echo "rc=$fail"
exit $fail
