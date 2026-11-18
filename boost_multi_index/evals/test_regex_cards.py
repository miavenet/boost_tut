#!/usr/bin/env python3
"""Attack-test every regex AcceptIf/RejectIf card: right answers must HIT, plausible wrong ones MISS.
Run from the tutorial directory:  python3 evals/test_regex_cards.py   (exit 0 = all behave)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import drive, tutorial_format

TESTS = {  # card id -> (right answers, wrong answers)
    'M1-C1': (["1 node, 3 headers", "one node per element, three index headers"], ["3 nodes, 3 headers", "1 node, 2 headers"]),
    'M1-C2': (["hashed_unique<member<Order, std::uint64_t, &Order::id>>, ordered_non_unique<member<Order, std::int64_t, &Order::price>>",
               "hashed_unique<key<&Order::id>>, ordered_non_unique<key<&Order::price>>"],
              ["ordered_non_unique<key<&Order::price>>, hashed_unique<key<&Order::id>>", "hashed_unique<key<&Order::id>>, ordered_unique<key<&Order::price>>"]),
    'M1-Q4': (["8,000,000 per second hand-rolled vs 2,000,000 with multi_index", "8 million vs 2 million"],
              ["2 million hand-rolled, 8 million multi_index", "16 million vs 2 million"]),
    # M2-C1 has no AcceptIf (free-text negation cannot be graded by a regex); see SELF_GRADED below.
    'M2-C2': (["&Order::id", "key<&Order::id>", "bmi::key<&Order::id>", "boost::multi_index::key<&Order::id>", "key< &Order::id >"],
              ["Order.id", "member<Order,&Order::id>", "key<&Order::id", "&Order::id>", "bmi::key<&Order::id", "key&Order::id>"]),
    'M3-C1': (["24, 16, 16, 8", "ordered 24 B, hashed 16 B, sequenced 16, random_access 8",
               "24, 16, 16, 8 (+ 8 B pointer array)", "24, 16, 16, 8 (plus pointer array)",
               "24, 16, 16, 8 B (+ 8 B pointer array)", "ordered 24 B, hashed 16 B, sequenced 16 B, random_access 8 B (plus 8 B pointer array)",
               "24, 16, 16, 8 + 8 B pointer array", "24, 16, 16, 8 B"],
              ["32, 16, 16, 8", "16, 24, 16, 8", "24, 16, 16, 8 (no, wait: ordered is 32)", "24, 16, 16, 80", "24, 16, 16, 8, 32", "24, 16, 16, 8 (plus 32)",
               "24, 16, 16, 8 (+ 16 B pointer array)", "24, 16, 16, 8 (plus 80)", "24, 16, 16, 8 + 16 B pointer array"]),
    'M3-C2': (["s.get<2>().reserve(n); index 0", "s.get<2>().reserve(n) and only index 0",
               "s.get<2>().reserve(n); index 0 (does not reserve all indices)"],
              ["s.get<1>().reserve(n), index 0", "s.get<2>().reserve(n), all indices",
               "s.get<ByClient>().reserve(n); index 0",   # the question now says: numeric indices, the store has no tags
               "s.get<2>().reserve(n); index 10"]),
    'M4-C1': (["level[p].push_back(o), FIFO order", "p; insertion order", "px, FIFO", "price - band_low; FIFO", "p; queue order",
               "p, FIFO (not sorted)", "p, FIFO, never LIFO", "level[p], first-in first-out"],
              ["level[p], sorted order", "p, LIFO", "p, sorted, not FIFO", "p, not FIFO"]),
    'M4-C2': (["std::uint32_t slot index; elements move on rehash", "uint32_t, they get relocated",
               "std::size_t; move", "std::uint32_t; not pointer-stable, elements move", "uint32_t; elements are not stable, they move"],
              ["Node; they move", "uint32_t slot, but elements are stable and never move", "uint32_t, elements are pointer-stable",
               "Order; they stay put and are stable"]),
}

SELF_GRADED = {'M2-C1'}  # attack strings that fooled the old regex; the card is now graded against its Key by the learner

def grade(card, answer):
    rejected = card['RejectIf'] and drive.eval_predicate(card['RejectIf'], answer, '.')
    return (not rejected) and drive.eval_predicate(card['AcceptIf'], answer, '.')

cards = {c['id']: c for d in tutorial_format.parse_lessons('lessons') for c in d['cards']}
regex_cards = {i for i, c in cards.items() if c['AcceptIf'] and 'check(' not in c['AcceptIf']}
bad = sorted(regex_cards - set(TESTS))
for cid in SELF_GRADED:
    if cards[cid]['AcceptIf'] or cards[cid]['RejectIf']: bad.append(f'{cid} should be self-graded (no AcceptIf/RejectIf)')
for cid, (right, wrong) in TESTS.items():
    for a in right:
        if not grade(cards[cid], a): bad.append(f'{cid} false-reject: {a}')
    for a in wrong:
        if grade(cards[cid], a): bad.append(f'{cid} false-accept: {a}')
print('\n'.join(bad) if bad else f'all {len(TESTS)} regex cards behave ({sum(len(r)+len(w) for r, w in TESTS.values())} attacks)')
sys.exit(1 if bad else 0)
