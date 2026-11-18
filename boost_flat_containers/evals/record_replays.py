#!/usr/bin/env python3
"""Record the committed delete-the-model replays with a SCRIPTED learner (no model).

This is how evals/replays/*.replay were produced. It is not a human pilot:
answers are the authored Key text (self-graded cards) or known-passing code
(check() cards, the same answers tests/test_checks.sh accepts), plus planned
misses. Every check() card is graded by the real checks/*.check.sh scripts.

Usage (from the tutorial root): python3 evals/record_replays.py [OUT_DIR]
It writes OUT_DIR/<date>.replay (default evals/replays/) from fresh progress.
"""
import builtins, json, re, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import drive, tutorial_format as fmt, v2_assessment  # noqa: E402

NEW_LIMIT = 4                       # the tutorial's documented limit
DAYS = ['2026-11-16', '2026-11-17', '2026-11-18', '2026-11-19', '2026-11-20',
        '2026-11-21', '2026-11-27']  # 5 study days, a next-day review, a 6-day gap
ASSESSMENT_DAY = '2026-11-30'       # >= 7 days after the last new concept (11-20)

# check() cards: answers that the real checks accept (see tests/test_checks.sh)
CHECK_ANSWERS = {
    '01-Q3': ['80000', '160000'],    # planned SURE miss first, then the right answer
    '01-Q5': ['-O2 -DNDEBUG'],
    '02-Q4': ['long limit_after_listing(RiskTable& t) { add_listings(t); return t.find(200)->second.max_qty; }'],
    '02-Q6': ['auto c = std::move(s).extract();\nfor (auto& v : c.values) v = 0.0;\ns.replace(std::move(c.keys), std::move(c.values));'],
    '03-Q2': ['FM build(std::vector<std::pair<uint64_t,uint64_t>> v) { std::sort(v.begin(), v.end()); return FM(bc::ordered_unique_range, v.begin(), v.end()); }'],
    '03-Q5': ['boost::container::bad_alloc'],
    '04-Q2': ['const Config* find_cfg(const std::vector<Config>& t, uint32_t id) { return id < t.size() ? &t[id] : nullptr; }'],
    '04-Q4': ['h.reserve(1000000);'],
    '04-Q6': ['risk.reserve(risk.size() + kMaxNewListings);'],
}
# planned self-graded misses: (wrong first answer, grade sequence)
PLANNED = {
    '01-Q2': (['flat_map does fewer comparisons than std::map'], ['m', 'h']),  # unsure miss
    '03-Q7': (['static_vector grows onto the heap like small_vector'], ['m', 'h']),  # sure miss
}
CONFIDENCE = {'01-Q3': ['sure', 'sure'], '01-Q2': ['unsure', 'unsure'],
              '03-Q7': ['sure', 'unsure']}
DEFAULT_CONFIDENCE = 'unsure'


def key_text(card):
    key = card.get('Key', '')
    parts = key if isinstance(key, list) else key.splitlines()
    parts = [str(l).strip().removeprefix('- ').strip() for l in parts if str(l).strip()]
    return ' '.join(parts)[:400].rstrip(':') or 'answer from memory'


def main(out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    docs = fmt.parse_lessons(ROOT / 'lessons')
    cards = {c['id']: c for d in docs for c in d['cards']}
    seen = {}
    last = [None]
    real_print = builtins.print

    def tee(*a, **k):
        text = ' '.join(str(x) for x in a)
        m = re.match(r'^\[(\S+)\] ', text)
        if m: last[0] = m.group(1)
        real_print(*a, **k)

    def nth(seq, i): return seq[min(i, len(seq) - 1)]

    def fake_input(prompt=''):
        cid = last[0]
        n = seen.get(cid, 0)
        if prompt.startswith('Your answer'):
            seen[cid] = n + 1
            n = seen[cid] - 1
            if cid in CHECK_ANSWERS: v = nth(CHECK_ANSWERS[cid], n)
            elif cid in PLANNED: v = nth(PLANNED[cid][0] + [key_text(cards[cid])], n)
            elif cid in cards: v = key_text(cards[cid])
            else: v = 'my answer'
        elif prompt.startswith('Response confidence'):
            v = nth(CONFIDENCE.get(cid, [DEFAULT_CONFIDENCE]), max(n - 1, 0))
        elif prompt.startswith('Self-grade'):
            v = nth(PLANNED[cid][1], max(n - 1, 0)) if cid in PLANNED else 'h'
        elif prompt.startswith('Your response'):
            v = 'my own explanation, written before opening the key'
        else:
            v = ''
        real_print(prompt + v.replace('\n', '\\n'))
        return v

    progress = Path(tempfile.mkdtemp()) / 'p.json'
    real_input = builtins.input
    builtins.input, builtins.print = fake_input, tee
    try:
        for day in DAYS:
            rec = out_dir / f'{day}.replay'
            rec.unlink(missing_ok=True)
            drive.run(str(ROOT / 'lessons'), str(progress), day, new_limit=NEW_LIMIT, record=str(rec))
    finally:
        builtins.input, builtins.print = real_input, real_print
    # assessment: answers are written from memory; its replay is hand-built so the
    # tags are explicit (assessment_answer, confidence, assessment_grade)
    form = v2_assessment.load_form(ROOT)
    lines = []
    for item in form['items']:
        lines += ['assessment_answer\t' + ' '.join(item['key'])[:300], 'confidence\tunsure']
    for item in form['items']:
        lines.append('assessment_grade\t' + ('p' if item['id'] == 'A4-T' else 'h'))
    (out_dir / f'{ASSESSMENT_DAY}.assessment.replay').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main(Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'evals' / 'replays')
