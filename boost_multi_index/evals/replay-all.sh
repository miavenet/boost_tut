#!/bin/bash
# Delete-the-model replay: replays every recorded study day (evals/replays/PLAN.tsv)
# through the real T1 driver into a FRESH temp progress file, with the real checks/.
# Exits non-zero if any day fails, a replay mismatches or runs dry, any concept is
# left untaught, any card is never answered, any check() card never passes, or the
# assessment does not complete.  Usage (from anywhere):  bash evals/replay-all.sh
# Re-record (changes the replay files):  python3 evals/record_replays.py
cd "$(dirname "$0")/.." || exit 1
LIMIT=5
TMP=$(mktemp -d)
P="$TMP/progress.json"
trap 'rm -rf "$TMP"' EXIT
fail=0
days=0
while IFS=$'\t' read -r day kind; do
  [ -z "$day" ] && continue
  if [ "$kind" = assessment ]; then
    f="evals/replays/$day.assessment.replay"
    echo "=== $day assessment ($f)"
    python3 drive.py lessons --progress "$P" --today "$day" --assessment --replay "$f" > "$TMP/out.txt" 2>&1
  else
    f="evals/replays/$day.replay"
    echo "=== $day session ($f) --new-limit $LIMIT"
    python3 drive.py lessons --progress "$P" --today "$day" --new-limit "$LIMIT" --replay "$f" > "$TMP/out.txt" 2>&1
  fi
  rc=$?
  cat "$TMP/out.txt"
  echo "rc=$rc"
  days=$((days + 1))
  [ $rc -ne 0 ] && fail=1
  if grep -qE 'Replay exhausted|replay tag mismatch|drive.py: error|Traceback' "$TMP/out.txt"; then
    echo "REPLAY FAILURE on $day"; fail=1
  fi
done < evals/replays/PLAN.tsv

python3 - "$P" "$days" "$fail" <<'EOF'
import json, sys
sys.path.insert(0, '.')
import tutorial_format as f
p = json.load(open(sys.argv[1])); days = int(sys.argv[2]); fail = int(sys.argv[3])
docs = f.parse_lessons('lessons')
rt = p['_runtime']
concepts = [':'.join(map(str, c['identity'])) for d in docs for c in d['concepts']]
taught = [c for c in concepts if 'teach:' + c in rt['completed_steps']]
cards = [c for d in docs for c in d['cards']]
recs = [r for r in rt['attempt_records'] if r.get('mode') == 'T1']
answered = {r['card'] for r in recs}
checks = [c['id'] for c in cards if (c.get('AcceptIf') or '').strip().startswith('check(')]
checks_hit = {r['card'] for r in recs if r['grading_source'] == 'check' and r['grade'] == 'h'}
checks_miss = sorted({r['card'] for r in recs if r['grading_source'] == 'check' and r['grade'] == 'm'})
misses = sorted({r['card'] for r in recs if r['grade'] == 'm'})
sure = sorted({r['card'] for r in recs if r.get('hypercorrection')})
still_flagged = sorted(k for k, v in p.items() if isinstance(v, dict) and v.get('hypercorrection'))
runs = [r for runs in rt.get('assessments', {}).values() for r in runs]
assessed = runs and all(r.get('complete') for r in runs)
items = sum(len(r['grades']) for r in runs)
print('--- coverage summary')
print(f'concepts taught: {len(taught)}/{len(concepts)}')
print(f'cards answered: {len(answered)}/{len(cards)}')
print(f'check() cards passing for real: {len(checks_hit & set(checks))}/{len(checks)} (real check MISS seen: {", ".join(checks_miss) or "none"})')
print(f'deliberate misses: {", ".join(misses)}; high-confidence (sure) misses: {", ".join(sure) or "none"}; still flagged: {", ".join(still_flagged) or "none"}')
print(f'assessment: {"complete" if assessed else "NOT complete"}, {items} items graded')
print(f'days: {days}')
ok = (not fail and len(taught) == len(concepts) and len(answered) == len(cards)
      and set(checks) <= checks_hit and sure and assessed)
print(f'rc={0 if ok else 1}')
sys.exit(0 if ok else 1)
EOF
