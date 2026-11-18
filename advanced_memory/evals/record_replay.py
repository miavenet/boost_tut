#!/usr/bin/env python3
"""Record the full delete-the-model replay with a scripted learner (no model).

Usage (from the tutorial root):
  python3 evals/record_replay.py PROGRESS DATE [--assessment]
Writes evals/replays/DATE.replay (or DATE.assessment.replay) through the driver's
own --record path, so the file holds exactly the prompts the driver asked.

The learner answers by card ID:
  - check() cards: the passing fixture checks/fixtures/<check>.right (graded for real);
  - regex cards: the first `accept` row of checks/fixtures/regex_cards.tsv;
  - self-graded cards: grade `h`, except the scripted misses in SCRIPT below.
Only used to (re)record; evals/replay-all.sh replays the committed files.
"""
import builtins, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import drive, tutorial_format  # noqa: E402

progress, today = sys.argv[1], sys.argv[2]
assessment = '--assessment' in sys.argv[3:]
NEW_LIMIT = 6  # the documented default (drive.py --new-limit)

# Scripted misses: {date: {card: [grade for attempt 1, attempt 2, ...]}} (self-graded cards),
# confidence per (date, card, attempt). 01-Q1 is a plain miss (unsure); 01-Q3 a SURE miss.
SCRIPT = {'2026-11-16': {'01-Q1': ['m', 'h'], '01-Q3': ['m', 'm']}}
CONF = {('2026-11-16', '01-Q1', 1): 'unsure', ('2026-11-16', '01-Q3', 1): 'sure',
        ('2026-11-16', '01-C1', 1): '0.3'}
# A deliberate wrong code answer on a check() card on day 1 (graded MISS by the real check),
# then the right one on the same-session retry.
WRONG_FIRST = {('2026-11-16', '01-C1')}

docs = tutorial_format.parse_lessons(ROOT / 'lessons')
cards = {c['id']: c for d in docs for c in d['cards']}
regex_ok = {}
for line in (ROOT / 'checks/fixtures/regex_cards.tsv').read_text(encoding='utf-8').splitlines():
    if line and not line.startswith('#'):
        cid, want, answer = line.split('\t')
        if want == 'accept':
            regex_ok.setdefault(cid, answer)


def right_answer(cid, attempt):
    c = cards[cid]
    m = re.match(r'check\((checks/[^)]+)\.sh\)', (c.get('AcceptIf') or '').strip())
    if m:
        kind = 'wrong' if (today, cid) in WRONG_FIRST and attempt == 1 else 'right'
        return (ROOT / 'checks/fixtures' / (Path(m.group(1)).name + '.' + kind)).read_text(encoding='utf-8').rstrip('\n')
    if c.get('AcceptIf'):
        return regex_ok[cid]
    return 'answer written from memory against the key'


last = [None]
attempts = {}
real_print = builtins.print


def tracking_print(*args, **kwargs):
    text = ' '.join(str(a) for a in args)
    for line in text.splitlines():
        m = re.match(r'^\[(\S+)\] ', line)
        if m:
            last[0] = m.group(1)
    real_print(*args, **kwargs)


def fake_input(prompt=''):
    cid = last[0]
    if prompt.startswith('Your answer') and assessment:
        value = 'response written from memory'
    elif prompt.startswith('Your answer'):
        attempts[cid] = attempts.get(cid, 0) + 1
        value = right_answer(cid, attempts[cid])
    elif prompt.startswith('Response confidence'):
        value = CONF.get((today, cid, attempts.get(cid, 1)), '0.8' if not assessment else '0.6')
    elif prompt.startswith('Self-grade'):
        plan = SCRIPT.get(today, {}).get(cid, [])
        n = attempts.get(cid, 1)
        value = plan[n - 1] if n <= len(plan) else 'h'
    elif prompt.startswith('Your response'):
        value = 'my own words'
    else:  # Continue / Stretch attempt
        value = ''
    real_print(prompt + value.replace('\n', '\\n'))
    return value


builtins.print = tracking_print
builtins.input = fake_input
out = ROOT / 'evals/replays' / (today + ('.assessment' if assessment else '') + '.replay')
out.parent.mkdir(parents=True, exist_ok=True)
if out.exists():
    out.unlink()
if assessment:
    # v2_assessment has no --record; write the tagged lines it reads.
    import json, v2_assessment
    form = v2_assessment.load_form(ROOT)
    lines = []
    for item in form['items']:
        lines += ['assessment_answer\tresponse written from memory', 'confidence\t0.6']
    for item in form['items']:
        lines.append('assessment_grade\t' + ('p' if item['id'] == 'A11R' else 'h'))
    out.write_text('\n'.join(lines) + '\n', encoding='utf-8')
else:
    drive.run(str(ROOT / 'lessons'), progress, today, new_limit=NEW_LIMIT, record=str(out))
builtins.print = real_print
