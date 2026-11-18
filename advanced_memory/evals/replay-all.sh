#!/bin/bash
# Full delete-the-model replay: every recorded study day in evals/replays/ is replayed through
# drive.py (no model, real checks/ graders) into a FRESH temporary progress file, in date order,
# then the delayed assessment. Exit non-zero on any driver error, replay mismatch or exhausted
# replay, on any untaught concept / unanswered card, or if a check() card never HIT.
#   bash evals/replay-all.sh                      # from anywhere
# Re-record (only if lessons or the driver change): python3 evals/record_replay.py (see its header).
cd "$(dirname "$0")/.." || exit 1
NEW_LIMIT=6                                  # the drive.py default; recordings used the same
W="$(mktemp -d "${TMPDIR:-/tmp}/amreplay.XXXXXX")"; trap 'rm -rf "$W"' EXIT
P="$W/progress.json"
bad=0; days=0
for f in $(ls evals/replays/*.replay | sort); do
    d=$(basename "$f" .replay)
    if [[ "$d" == *.assessment ]]; then
        d=${d%.assessment}
        echo "=== $d (assessment)"
        python3 drive.py lessons --progress "$P" --today "$d" --assessment --replay "$f" > "$W/out" 2>&1
    else
        echo "=== $d"
        python3 drive.py lessons --progress "$P" --today "$d" --new-limit "$NEW_LIMIT" --replay "$f" > "$W/out" 2>&1
        days=$((days + 1))
    fi
    rc=$?
    cat "$W/out"
    echo "rc=$rc"
    if [ $rc -ne 0 ] || grep -qE 'Replay exhausted|replay tag mismatch|drive.py: error|Traceback' "$W/out"; then
        echo "FAIL replay $f"; bad=1
    fi
done
python3 - "$P" "$days" "$bad" <<'PY'
import json, sys
sys.path.insert(0, '.')
import tutorial_format as fmt, v2_assessment
progress, days, bad = json.load(open(sys.argv[1])), int(sys.argv[2]), int(sys.argv[3])
docs = fmt.parse_lessons('lessons')
rt = progress['_runtime']
concepts = [':'.join(map(str, c['identity'])) for d in docs for c in d['concepts']]
taught = {s[len('teach:'):] for s in rt['completed_steps'] if s.startswith('teach:')}
cards = [c for d in docs for c in d['cards']]
recs = rt['attempt_records']
answered = {r['card'] for r in recs if r.get('mode') == 'T1'}
checks = [c['id'] for c in cards if (c.get('AcceptIf') or '').strip().startswith('check(')]
check_hit = {r['card'] for r in recs if r.get('grading_source') == 'check' and r['grade'] == 'h'}
misses = sorted({r['card'] for r in recs if r.get('mode') == 'T1' and r['grade'] == 'm'})
sure = sorted({r['card'] for r in recs if r.get('hypercorrection')})
still_flagged = sorted(k for k, v in progress.items() if isinstance(v, dict) and v.get('hypercorrection'))
form = v2_assessment.load_form('.')
runs = rt.get('assessments', {}).get(form['id'], [])
graded = len(runs[-1]['grades']) if runs else 0
untaught = [c for c in concepts if c not in taught]
unanswered = [c['id'] for c in cards if c['id'] not in answered]
nohit = [c for c in checks if c not in check_hit]
print('--- coverage summary')
print(f'concepts taught {len(concepts) - len(untaught)}/{len(concepts)}' + (f' MISSING {untaught}' if untaught else ''))
print(f'cards answered {len(cards) - len(unanswered)}/{len(cards)}' + (f' MISSING {unanswered}' if unanswered else ''))
print(f'check() cards graded HIT by the real check {len(checks) - len(nohit)}/{len(checks)}' + (f' NO HIT {nohit}' if nohit else ''))
print(f'misses (carried forward to the next day): {", ".join(misses)}')
print(f'high-confidence (sure) misses re-tested first: {", ".join(sure)}; still flagged at end: {still_flagged or "none"}')
print(f'assessment {form["id"]}: {graded}/{len(form["items"])} items graded, complete={bool(runs and runs[-1]["complete"])}')
print(f'study days {days} + 1 assessment day; attempt records {len(recs)}')
ok = not (bad or untaught or unanswered or nohit or not sure or not misses or graded != len(form['items']))
print(f'rc={0 if ok else 1}')
sys.exit(0 if ok else 1)
PY
