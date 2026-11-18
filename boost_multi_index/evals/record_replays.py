#!/usr/bin/env python3
"""Record the full multi-day delete-the-model replay with a scripted learner (no model).

Run from the tutorial directory:  python3 evals/record_replays.py
It deletes evals/replays/, then drives the real T1 driver (drive.run, real
checks/*.sh compiled and run) one study day at a time with --new-limit 5 and
--record, until every concept is taught and every card answered, then records
one review day after a 7-day gap and writes the delayed assessment replay.
Deliberate misses (visible carry-forward and the sure-miss retest):
  - M1-Q4 on day 1: wrong number answered "sure"  -> HIGH-CONFIDENCE MISS, retried first.
  - M2-C2 on day 2: `member<...>` answered 0.3    -> ordinary miss, retried, due next day.
  - M3-Q2 on day 3: `s.reserve(n)` (real check MISS), then the right answer on retry.
  - M1-Q1 on day 1: self-graded partial            -> due next day.
Replay with:  bash evals/replay-all.sh
"""
import builtins, io, json, os, re, shutil, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
os.chdir(ROOT)
import drive, tutorial_format  # noqa: E402

LIMIT = 5
OUT = ROOT / 'evals' / 'replays'
FIX = ROOT / 'checks' / 'fixtures'

def fixture(name):
    return (FIX / name).read_text(encoding='utf-8').rstrip('\n')

# Correct answers (regex cards from the Key; check cards from checks/fixtures/*.right).
RIGHT = {
    'M1-C1': '1 node per element, 3 headers (one per index)',
    'M1-C2': 'hashed_unique<member<Order, std::uint64_t, &Order::id>>, ordered_non_unique<member<Order, std::int64_t, &Order::price>>',
    'M1-Q2': fixture('l1-decl.right'),
    'M1-Q3': fixture('l1-instruments.right'),
    'M1-Q4': '8,000,000 allocations/s hand-rolled vs 2,000,000/s with multi_index',
    'M2-C2': 'key<&Order::id>',
    'M2-Q1': fixture('l2-rollback.right'),
    'M2-Q2': fixture('l2-ahead.right'),
    'M3-C1': '24, 16, 16, 8',
    'M3-C2': 's.get<2>().reserve(n); index 0',
    'M3-Q1': '40+16+24',
    'M3-Q2': fixture('l3-reserve.right'),
    'M4-C1': 'level[p].push_back(o); FIFO insertion order',
    'M4-C2': 'std::uint32_t slot index, because elements move on rehash',
    'M4-Q1': fixture('l4-ladder.right'),
    'M4-Q2': fixture('l4-alloc.right'),
}
# First-attempt wrong answers (consumed once, then RIGHT is used).
WRONG = {
    'M1-Q4': ('4,000,000 allocations/s hand-rolled vs 2,000,000/s with multi_index', 'sure'),
    'M2-C2': ('member<&Order::id>', '0.3'),
    'M3-Q2': (fixture('l3-reserve.wrong'), '0.4'),
}
GRADES_ONCE = {'M1-Q1': 'p'}          # first self-grade only
CONF = {'M1-Q2': '0.9', 'M2-Q1': 'unsure', 'M3-Q1': '80%'}  # others blank

state = {'card': None, 'used_wrong': set(), 'used_grade': set()}
real_stdout = sys.stdout


class Tee(io.TextIOBase):
    def write(self, text):
        for line in text.splitlines():
            m = re.match(r'^\[(\S+)\] ', line)
            if m:
                state['card'] = m.group(1)
        return real_stdout.write(text)


def fake_input(prompt=''):
    card = state['card']
    if prompt.startswith('Your answer'):
        if card in WRONG and card not in state['used_wrong']:
            value = WRONG[card][0]
            state['pending_conf'] = WRONG[card][1]
            state['used_wrong'].add(card)
        else:
            value = RIGHT.get(card, f'my answer to {card} from memory')
            state['pending_conf'] = CONF.get(card, '')
    elif prompt.startswith('Response confidence'):
        value = state.pop('pending_conf', '')
    elif prompt.startswith('Self-grade'):
        if card in GRADES_ONCE and card not in state['used_grade']:
            state['used_grade'].add(card)
            value = GRADES_ONCE[card]
        else:
            value = 'h'
    elif prompt.startswith('Your response'):
        value = 'my own words: done and compared with the key'
    else:  # Continue / Stretch
        value = ''
    print(prompt + value.replace('\n', '\\n'))
    return value


def coverage(progress_path):
    docs = tutorial_format.parse_lessons('lessons')
    p = json.loads(Path(progress_path).read_text(encoding='utf-8'))
    taught = [s for s in p['_runtime']['completed_steps'] if s.startswith('teach:')]
    n = sum(len(d['concepts']) for d in docs)
    ids = {c['id'] for d in docs for c in d['cards']}
    return len(taught), n, len(ids - set(p)), len(ids)


ASSESS = [
    ('assessment_answer', 'Forgotten index: client container keeps dangling pointers to destroyed orders. Also stale key and partial failure. 3 allocations per insert vs 1 for multi_index.'),
    ('confidence', 'sure'),
    ('assessment_answer', 'Ward 3 still lists the patient: stale key. MI elements are const, ward only changes via modify/replace which re-file every index. 2 allocations vs 1.'),
    ('confidence', '0.8'),
    ('assessment_answer', 'get<Tag>() returns a reference to that index (std-container-like); tags survive reordering. project converts the iterator to the same element in the other index in O(1). structure_uniqueness.'),
    ('confidence', '0.7'),
    ('assessment_answer', 'hashed_unique<key<&Book::isbn>>, ordered_non_unique<key<&Book::author, &Book::year>>; c.get<1>().equal_range(std::make_tuple(std::string("Le Guin")))'),
    ('confidence', 'unsure'),
    ('assessment_answer', 'Plain modify erases it, returns false, size -1. Rollback form restores and keeps it; capture the old value first. replace leaves it unchanged and returns false.'),
    ('confidence', 'sure'),
    ('assessment_answer', 'No erase for rating. Slow: city index walks its equal-key group (~50k). Use replace. Duplicate plate: erased by plain modify; use rollback or replace.'),
    ('confidence', '0.6'),
    ('assessment_answer', 'Colour bit packed in parent pointer. Ranked adds a size word for nth/rank. random_access has a pointer array.'),
    ('confidence', '0.5'),
    ('assessment_answer', '104 B. Pointer stays valid (nodes never move); vector would move. random_access: 96 B plus pointer array, no contiguity, O(n) front inserts.'),
    ('confidence', '0.7'),
    ('assessment_answer', 'One insert crosses max_load_factor (1.0) and rehashes everything. s.reserve only reaches index 0; reserve get<0> and get<2>.'),
    ('confidence', 'sure'),
    ('assessment_answer', 'Reserve index 1 too; look up via the hashed index not the ordered one (~22 misses); pre-touched allocator for the remaining tail; the arena does not fix the walk.'),
    ('confidence', '0.6'),
    ('assessment_answer', 'Ladder array; dense slot array; unordered_flat_map with slot index; intrusive lists on a pool. Use MI for warm multi-path tables.'),
    ('confidence', '0.8'),
    ('assessment_answer', 'MI for the player directory; dense array for rating by id. Guild totals: index on guild.'),
    ('confidence', 'sure'),
] + [('assessment_grade', g) for g in ['h', 'h', 'h', 'h', 'h', 'h', 'p', 'h', 'h', 'h', 'h', 'p']]


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(parents=True)
    tmp = ROOT / 'evals' / '.record-progress.json'
    if tmp.exists():
        tmp.unlink()
    builtins.input = fake_input
    sys.stdout = Tee()
    days = ['2026-11-16', '2026-11-17', '2026-11-18', '2026-11-19', '2026-11-20', '2026-11-21']
    done_day = None
    plan = []
    try:
        for day in days:
            drive.run('lessons', str(tmp), day, new_limit=LIMIT, record=str(OUT / f'{day}.replay'))
            plan.append(f'{day}\tsession')
            t, n, unseen, _ = coverage(tmp)
            if t == n and unseen == 0:
                done_day = day
                break
        if done_day is None:
            raise SystemExit('coverage not reached')
        import datetime
        gap = (datetime.date.fromisoformat(done_day) + datetime.timedelta(days=7)).isoformat()
        drive.run('lessons', str(tmp), gap, new_limit=LIMIT, record=str(OUT / f'{gap}.replay'))
        plan.append(f'{gap}\tsession')
        assess_day = (datetime.date.fromisoformat(gap) + datetime.timedelta(days=1)).isoformat()
        (OUT / f'{assess_day}.assessment.replay').write_text(
            ''.join(f'{t}\t{v}\n' for t, v in ASSESS), encoding='utf-8')
        plan.append(f'{assess_day}\tassessment')
    finally:
        sys.stdout = real_stdout
    (OUT / 'PLAN.tsv').write_text('\n'.join(plan) + '\n', encoding='utf-8')
    tmp.unlink()
    print('recorded:', ', '.join(plan))


if __name__ == '__main__':
    main()
