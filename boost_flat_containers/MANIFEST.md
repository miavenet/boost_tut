# Boost flat containers for low-latency lookup tables

format-version: 2

## Subject & scope
- **Subject:** `boost::container::flat_map` / `flat_set` (and multi variants), with
  `small_vector` / `static_vector` storage, compared with `std::map`,
  `std::unordered_map`, C++23 `std::flat_map`, `boost::unordered_flat_map`, direct
  indexing and Eytzinger layout.
- **Grain:** Given a lookup table in a hot path, choose between Boost flat_map/flat_set,
  the std:: equivalents and the alternatives, and predict lookup/insert/erase
  latency, including the tail.
- **Prerequisites:** fluent C++ (std::vector, std::map, std::unordered_map, iterators,
  move semantics); a shell; clang++ with C++23; Boost headers (authored with Boost 1.90
  at /opt/homebrew/include). No Boost compiled libraries are needed.
- **Retention target:** use it at work for years, so it gets four spaced reviews over about a month.
- **Related:** pooled/arena allocators live in `../advanced_memory`. This tutorial only
  leaves the template slot for them (lesson 4, CONCEPT L4C5).

## Time cost (an ESTIMATE, not a measurement; never timed with a learner)
- **How it was estimated:** 55 cards (12 pretest, 12 completion, 10 recall, 17 apply, 4 synthesize) at assumed 0.5 / 0.75 / 0.75 / 3 / 4 min each = ~90 min, plus ~15 min of reading and running labs. Those per-card figures are the author's assumptions. A slower reader, or one who types real code for the apply cards, will exceed it; time your first sitting and treat that as the real number.
- **Delayed assessment:** `assessment/form.json` (paper: `assessment/FORM.md`), 14 unseen items, one retention + one varied-domain transfer item per objective (prerequisites L1C3, L1C2, L2C1, L2C3, L3C4, L4C4, L4C5 = objectives 1–7). ~15 min estimate for 14 written answers (the generated FORM.md header's "about 8 minutes" is fixed template text). Take it once, >= 7 days after the last lesson is taught: `python3 drive.py lessons --progress progress.json --today YYYY-MM-DD --assessment`. Responses are saved before any key is shown; it stays outside the box schedule.
- **First pass:** ~105 min estimate, as four lesson sittings of 25–30 min on 5 study dates (date 5 only teaches the capstone concept L4C5), plus an optional ~15-minute `bench/run_all.sh` run (5 runs of every bench).
- **Per spaced review:** ~10 min estimate × 4 reviews over ~31 days (four-box 1/2/7/21-day default).
- **Interruptible:** yes, resumable from `progress.json` after any instructional step (`:quit` any time).
- Recommended driver call, one lesson per sitting: `--new-limit 4` (the driver default is 6). Lesson 4 has five concepts, so its capstone concept lands on the next date: **5 study dates** for first exposure (seen in the recorded replay, 2026-11-16..20).
- Driver behaviour to expect: a lesson's pretests are asked on the date that lesson is taught, before its teaching; later lessons print nothing until their date. Confidence is asked before every key by default (`--no-ask-confidence` opts out). A miss marked `sure` or >= 0.7 is flagged `HIGH-CONFIDENCE MISS`, retried first in the same session and asked before every other due card on the next session, and the flag clears only after a hit on a later date.
- Delete-the-model evidence: `bash evals/replay-all.sh` replays `evals/replays/` (7 study dates + the assessment, recorded with a scripted learner by `evals/record_replays.py`, not a human) into fresh progress.

## Learning objectives (measurable condition, performance and criterion)
1. Given a table's N, key/value sizes and build flags, the learner can rank flat_map, std::map, std::unordered_map, boost::unordered_flat_map and a linear scan by point-lookup latency, explaining the mechanism (pointer chases vs one array vs one bucket) and the NDEBUG effect, consistent with the measured table.
2. Given an insert/erase/bulk-build/growth scenario, the learner can compute the bytes a flat_map moves, identify the reallocation spike, and pick the bulk/reserve/batch remedy, with code that compiles and passes the timed check.
3. Given code that keeps references, pointers or iterators into a flat_map, or writes `it->first`, the learner can predict the failure (use-after-free vs silent wrong element vs broken ordering) and repair it so it passes under AddressSanitizer.
4. Given a requirement, the learner can state the structural and API differences between Boost flat_map and C++23 std::flat_map (one sequence vs two containers, proxy reference, ordered_unique_range vs sorted_unique, extract_sequence/adopt_sequence vs extract/replace, reserve) and write a working extract/modify/replace.
5. Given a small or bounded table, the learner can choose small_vector or static_vector as the underlying sequence and state exactly what happens at capacity (heap fallback vs boost::container::bad_alloc), verified by a compiled catch.
6. Given a hot-path lookup problem, the learner can choose among direct indexing, linear scan, boost::unordered_flat_map/node_map, Eytzinger/B-tree and flat_map, and state when NOT to use flat_map, citing a mechanism and a measured number.
7. Given a latency histogram with a tail spike from a table write, the learner can attribute it (reallocation vs shift vs rehash) and remove it so the capstone reports zero hot-path allocations.

## Tiers supported
```
tiers: [T0, T1, T2, T3]
detect:
  - model + file/exec tools -> T3   (Socratic; selects authored stretch/transfer items)
  - model, no tools         -> T2   (grades free text against Key bullets)
  - python3 drive.py        -> T1   (check() scripts compile+run answers; self-report otherwise)
  - else                    -> T0   (STUDY.md; checks can still be run by hand)
```

## Files
- `lessons/lesson-0{1..4}.md` contain the cards and are the source of truth. `STUDY.md` is the generated paper copy, with keys in a separate final section.
- `drive.py`, `tutorial_format.py`, `v2_session.py`, `v2_assessment.py` form the T1 bundle. `templates/` holds symlinks so `evals/audit.py` can import them. `assessment/` holds the delayed form; `evals/replays/` + `evals/replay-all.sh` are the recorded delete-the-model run. `PROTOCOL.md` is the T2/T3 tutor contract.
- `labs/*.cpp` (built by `labs/build.sh` into `labs/bin/`) are the hands-on programs. `bench/*.cpp` and `bench/run_all.sh` are the latency benches. `bench/results/*.txt` holds the authoring runs.
- `checks/*.check.sh` are executable graders (stdin = answer, exit 0 = HIT). `checks/lib.sh` holds their shared helpers. `tests/test_checks.sh` proves each check passes right answers and fails wrong ones.

## Coverage map (objective → cards)
| Objective | Cards | Lowest tier that can grade it |
|-----------|-------|-------------------------------|
| 1 | 01-Q1, 01-Q2, 01-Q5, 01-Q6 | T1 (01-Q5 check(); rest self-report) |
| 2 | 03-Q1, 03-Q3, 01-Q3, 01-Q4, 03-Q2, 03-Q4 | T1 (01-Q3, 03-Q2 check()) |
| 3 | 02-Q1, 02-Q2, 02-Q4, 02-Q5 | T1 (02-Q4 check() under ASan) |
| 4 | 02-Q3, 02-Q6, 02-Q7 | T1 (02-Q6 check()) |
| 5 | 03-Q7, 03-Q5, 03-Q6 | T1 (03-Q5 check()) |
| 6 | 04-Q1, 04-Q3, 04-Q2, 04-Q4, 04-Q5 | T1 (04-Q2, 04-Q4 check()) |
| 7 | 03-Q3, 04-Q6, 04-Q7 | T1 (04-Q6 check() runs the capstone) |
| synthesis | 01-INT, 02-INT, 03-INT, 04-INT | T0 self-report |

Each concept also has a pretest card (`*-P*`) and a completion card (`*-C*`). Transfer
cards change the domain (order book, feed handler, corporate actions, restricted list),
not just the variable names.

## Measurement method (read before quoting any number)
- Machine: Apple M1 Max (8P+2E), macOS, Apple clang 21.0.0, `-std=c++23 -O2`, Boost 1.90.
  Page 16 KiB, cache line 128 B (`sysctl`). Production HFT is usually x86-64 Linux
  (64 B lines, 4K/2M/1G pages), so **re-run `bench/run_all.sh` there**.
- `steady_clock` ticks every ~41.7 ns here (`bench/timer_res.cpp` printed 41 ns). Single
  writes are timed one per sample. Lookups are timed in batches of 64 and divided.
  For batches, "p99" means p99 of batch averages, and the result measures throughput of
  independent lookups (the core overlaps them), not one isolated lookup's latency.
- The machine was shared with other jobs during the 5 recorded runs, and numbers moved 20–50% between two sessions on the same machine depending on background load,
  so trust ratios and shapes; the µs-scale maxima include OS noise. The ms-scale spikes (reallocation, rehash) reproduced across runs.
- Every bench and the capstone were run 5 times; raw runs are `bench/results/run1..run5/`, `bench/median.py` writes the
  per-cell median to `bench/results/median/` and the spread to `bench/results/median/SPREAD.txt`. Lessons quote the median.
  Lookup p50s: median-cell spread 4–5% (90% of cells within ~13%, single cells up to 42%); tail maxima vary 2-10x, so tails are taught as mechanisms.
- Lesson numbers are `-DNDEBUG` unless stated; `bench/results/median/*.txt` (no `.ndebug`) are the
  asserts-ON builds the plain flags produce.

## Sourcing / claims ledger
| Claim | Grounding |
|---|---|
| flat_map = sorted vector-like associative container; non-stable iterators; faster lookup/iteration, slower insert/erase; weaker exception safety; non-movable types can't be stored | Boost docs, non_standard_containers.html#…flat_xxx (opened 2026-11-13) |
| value_type = std::pair<Key,T> (vs map's pair<const Key,T>); extract_sequence / adopt_sequence (ordered and unordered forms, complexities); ordered_unique_range ctor linear; insert(ordered_unique_range,…) "Linear"; range insert N log(size()+N); reserve/capacity/nth/index_of; erase invalidates keys not less than erased; insert "might invalidate elements" | Boost class reference classboost_1_1container_1_1flat__map.html (opened 2026-11-13); local headers flat_map.hpp lines 335, 1233, 1630 |
| it->first assignable (Boost) vs compile error (std::map, std::flat_map); find(20) fails after key edit | labs/l2_mutable_key.cpp run; compile errors observed |
| no extract(key) on Boost flat_map; std::map node handle works; non-movable mapped type fails in flat_map, compiles in std::map | tests/compile_claims.sh (12 compile claims, run 2026-11-13) |
| std::flat_map: adaptor, two containers, reference = pair<const K&, T&>, extract() &&, replace, keys()/values(), sorted_unique, linear insert/erase; no reserve member | cppreference flat_map and flat_map/extract (opened); labs/l2_std_vs_boost.cpp run; compile errors observed |
| `__cpp_lib_flat_map` = 202511 with Apple clang 21/libc++ | _probes/cpp_lib_flat_map.cpp compiled and run, bench/results/probes/probes-and-l1-disable-asserts.txt |
| BOOST_ASSERT = assert() unless NDEBUG / BOOST_DISABLE_ASSERTS; vec_iterator asserts in *, ++, += | /opt/homebrew/include/boost/assert.hpp lines 36–72; boost/container/vector.hpp line 158 onward |
| Boost vector default growth factor 60% | boost/container/vector.hpp line 768 (growth_factor_60) |
| static_vector: in-object storage; throw_on_overflow<true> default; else UB | Boost static_vector reference (opened) |
| overflow throws boost::container::bad_alloc (→ boost::container::exception → std::exception) unless BOOST_CONTAINER_USE_STD_EXCEPTIONS | boost/container/throw_exception.hpp lines 25–80; labs/l3_sequences.cpp run; checks/l3-overflow.check.sh |
| small_vector grows beyond inline capacity | Boost docs non_standard_containers.html#…small_vector (opened) |
| unordered_flat_map: open addressing, elements in bucket array, no pointer stability under rehash, max load factor not user-settable (setter does nothing), reserve = rehash(ceil(n/mlf)); unordered_node_map pointer-stable but slower | Boost.Unordered reference unordered_flat_map.html and intro.html (opened) |
| Eytzinger fastest for large arrays, sorted best for small | Khuong & Morin, arXiv:1509.05053 (abstract opened) |
| every latency/count number in the lessons | bench/*.cpp and labs/*.cpp, outputs in bench/results/run1..run5/ and bench/results/median/ |
| `-DBOOST_DISABLE_ASSERTS` = same 22.0 ns as `-DNDEBUG` (80.2 asserts on) | 5 runs of each build, bench/results/probes/probes-and-l1-disable-asserts.txt |
| exact compile diagnostics in lesson 2 | bench/results/probes/compile_claims.txt |
