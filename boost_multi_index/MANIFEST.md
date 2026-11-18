# Boost.MultiIndex for low-latency trading

format-version: 2

## Subject & scope
- **Subject:** Boost.MultiIndex (Boost 1.90) for order stores and trading data sets.
- **Grain:** Given a trading data set that must be looked up several ways (an order store by
  order-id, by (side, price, time), by client-id), decide whether Boost.MultiIndex is the right
  tool, build it correctly, and predict its latency behaviour including the tail.
- **Prerequisites:** fluent C++17/20, `std::map` / `std::unordered_map` / `std::vector` and
  their invalidation rules; a shell. Toolchain: `clang++ -std=c++23 -O2 -I/opt/homebrew/include`
  (verified: Apple clang 21.0.0, libc++, Boost 1.90 headers, Apple M1 Max).
- **Retention target:** use it at work for months, so the course runs 4 spaced reviews over about 31 days.
- **Learner:** an experienced HFT C++ developer who is short on time. Scaffolding is light
  (problem-first, with short worked code), and the HandsOn runs come early.

## Time cost (estimate, honest — not measured with learners)
- **First pass:** about 110 min in 4 sittings (L1 25, L2 25, L3 30, L4 30). The author's
  estimate: 4–6 concept cards, 7–10 answer prompts and 3–5 program runs per lesson.
  Machine time inside that, measured: `bash labs/build.sh` 12.6 s; `bench_lookup` 13.7 s;
  `bench_modify` about 8 s; `capstone` about 4.6 s; each `check()` about 1 s to compile.
- **Per spaced review:** about 10 min per due day, on days 1, 3, 10 and 31 after each lesson
  (four-box 1/2/7/21 schedule, an implementation default rather than a measured optimum).
- **Interruptible:** yes. `drive.py` resumes from `progress.json` after any step.
- **Driver defaults (T1):** run with `--new-limit 5` (one lesson of 5 concepts per study day,
  so 4 study days; the driver's built-in default is 6). Confidence is asked before every key
  by default (`--no-ask-confidence` opts out). A miss marked `sure` or >= 0.7 is flagged
  `HIGH-CONFIDENCE MISS` and re-tested first in the same session and before other due cards
  on later days, until a hit on a later day.
- **Delayed assessment:** about 8 min, taken once at least 7 days after Lesson 04:
  `drive.py lessons --progress progress.json --today YYYY-MM-DD --assessment`
  (paper: `assessment/FORM.md`). 12 unseen items, one retention and one transfer
  item per objective (prerequisite concepts M1C1, M1C3, M2C4, M3C1, M3C3, M4C5).

## Learning objectives (measurable condition, performance and criterion)
1. Given a hand-rolled multi-container order store, the learner can name its three
   desynchronisation failure modes, the multi_index property that removes each, and the
   allocations per insert for both designs, matching the measured 3 vs 1.
2. Given an access-path spec (unique/non-unique, hashed/ordered, composite, computed key),
   the learner can write a `multi_index_container` declaration with `member`/`const_mem_fun`/
   `composite_key` or `key<>`, tags, `get<>`, prefix lookups and `project<>` that compiles and
   passes the executable checks.
3. Given a `modify`/`replace` call on a container with unique and hashed_non_unique indices,
   the learner can predict the outcome on collision (erase vs unchanged), write a rollback
   that keeps the element, and predict the O(group) cost of `modify`.
4. Given an index mix, the learner can predict node bytes from per-index header sizes (to the
   byte, checked by a spy allocator) and state which iterators/pointers survive growth,
   including what `random_access` gives up.
5. Given a latency requirement, the learner can predict and remove tail sources: rehash
   spikes (reserve every hashed index), dependent pointer chasing per lookup structure, and
   allocation-order vs traversal-order locality, and state what an allocator swap does and
   does not fix.
6. Given a trading component, the learner can decide when NOT to use multi_index and choose
   between a price ladder, intrusive containers on a pre-allocated pool,
   `boost::unordered_flat_map` and a dense id→slot array, naming the trade-off of each and
   reading the capstone latency histogram.

## Tiers supported
```
tiers: [T0, T1, T2, T3]        # see guidelines/DUAL-MODE.md
detect:
  - model + file/exec tools -> T3
  - model, no tools         -> T2
  - python3 drive.py        -> T1
  - else                    -> T0 (self-quiz from STUDY.md)
```
T0 learners can still run the labs and checks by hand: `checks/<name>.sh < answer.txt`
prints HIT or MISS.

## Files
- `lessons/lesson-01..04.md`: the concept and question cards (source of truth)
- `labs/*.cpp`: single-file programs the learner runs and breaks; `labs/build.sh` builds
  all labs and benches into `build/` and exits non-zero on any failure
- `bench/*.cpp` + `bench/lat.hpp`: latency benchmarks printing mean/p50/p99/p99.9/max in ns;
  `bench/run5.sh` runs each 5 times into `bench/results/`, `bench/summarize.py` prints per-metric medians
- `checks/*.sh` → `checks/harness.py` + `checks/tpl/*.cpp`: compile and run the learner's
  code inside a test; `checks/selftest.sh` proves each check passes its
  `fixtures/*.right` and fails `fixtures/*.wrong` and an empty answer
- `evals/test_regex_cards.py`: attack test for every regex AcceptIf/RejectIf (68 attack strings; M2-C1 is self-graded)
- `assessment/form.json` + generated `assessment/FORM.md`, run by `v2_assessment.py`
  (`drive.py --assessment`): the separate delayed retention + transfer form
- `evals/replays/*.replay` + `PLAN.tsv`: a recorded 6-day delete-the-model run (4 study
  days, a review day 7 days later, then the assessment); `evals/replay-all.sh` replays it
  into fresh progress and fails on any mismatch or untaught concept;
  `evals/record_replays.py` re-records it with a scripted learner
- `STUDY.md`: the generated paper artifact; `drive.py`, `tutorial_format.py`, `v2_session.py`:
  the T1 bundle; `PROTOCOL.md`: the T2/T3 protocol
- `evals/audit.py`: a copy of the repo audit. One change: it also adds the tutorial root
  to `sys.path` so the standalone bundle imports.

## Coverage map (objective → cards)
| Objective | Cards | Lowest tier that can grade it |
|-----------|-------|-------------------------------|
| 1 | M1-Q1, M1-Q4, M1-C1, M1-INT | T1 (M1-Q4, M1-C1 regex+guard); M1-Q1 T0 self-report |
| 2 | M1-Q2, M1-Q5, M2-Q5, M1-Q3, M2-Q2, M1-C2, M2-C2 | T1 (check: l1-decl, l1-instruments, l2-ahead) |
| 3 | M2-Q1, M2-Q3, M2-Q4, M2-C1, M2-INT | T1 (check: l2-rollback); M2-Q3/Q4 self-report |
| 4 | M3-Q1, M3-Q4, M3-C1 | T1 (check: l3-nodesize); M3-Q4 self-report |
| 5 | M3-Q2, M3-Q5, M3-Q3, M3-C2, M3-INT | T1 (check: l3-reserve); M3-Q3/Q5 self-report |
| 6 | M4-Q1, M4-Q3, M4-Q4, M4-Q2, M4-Q5, M4-C1, M4-C2, M4-INT | T1 (check: l4-ladder, l4-alloc) |

## Sourcing / claims ledger
Measurements: Apple M1 Max (hw.cachelinesize 128, hw.pagesize 16384), Apple clang 21.0.0,
`-std=c++23 -O2`, Boost 1.90 (BOOST_VERSION 109000), macOS. The timer is `steady_clock`
with a measured 41 ns smallest step and 23 ns per `now()` (`bench/clock_probe`). Single-op
samples are quantised to about 41.67 ns; sub-100 ns ops are timed in batches (stated per
bench). **Production HFT is x86-64 Linux with 64-byte lines and 4K/2M/1G pages, usually on
isolated cores. These numbers will differ there, and the learner must re-run.** Nothing in
this tutorial is Linux-only.

| Claim | Grounding |
|---|---|
| hand-rolled 3 allocs/add, MI 1/insert, 0/erase | `build/l1_handrolled`, `build/l1_mi` (run) |
| forgotten index → ASan heap-use-after-free; plain build printed 710 | `build/l1_asan bug`, `build/l1_handrolled bug` (run) |
| stale key in hand-rolled book | `build/l1_handrolled stale` (run) |
| MI node = value + N headers; manual sim allocates N nodes; one word saved per ordered index | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/performance.html (opened) |
| node sizes 64/56/56/48/72/80/96/120; bytes/elem; hand-rolled 176.5 | `build/l3_node_size` (run, N=100000) |
| ordered colour bit packed in parent pointer | `boost/multi_index/detail/ord_index_node.hpp` `ordered_index_node_compressed_base` (read) |
| random_access node has `up()` back-pointer | `boost/multi_index/detail/rnd_index_node.hpp` (read) |
| `key<>` is C++17, resolves to member / const_mem_fun / composite_key | key_extraction.html (opened) + `static_assert` in `labs/l2_keys.cpp` (compiled) |
| composite-key vs tuple comparison uses the shorter length (equality needs the same length) | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/key_extraction.html (opened) |
| tags, `get<>` by tag or number, `project<>` | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/tutorial/basics.html (opened); `labs/l2_keys.cpp` (run) |
| modify erases on failed rearrangement; rollback keeps it if consistent; replace strong, returns false | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html (opened); `build/l4_modify` (run) |
| insert strong guarantee + "allowed by all other indices" | ord_indices.html insert (opened) |
| modify on hashed_non_unique walks the equal-key group; replace compares keys first | `boost/multi_index/hashed_index.hpp` `in_place(...hashed_non_unique_tag)`, `replace_` (read); `build/bench_modify` (run) |
| ordered in_place compares neighbours only | `boost/multi_index/detail/ord_index_impl.hpp` `in_place` (read) |
| MI iterators/pointers valid until erase in all index types; rehash preserves validity | ord/hash/rnd_indices.html (opened); `build/l5_stability` (run) |
| std::unordered_map rehash invalidates iterators, not references | https://en.cppreference.com/w/cpp/container/unordered_map.html (opened) |
| random_access: no contiguity, no `data()`; insert cost O(distance to end); capacity behaviour | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnd_indices.html (opened); `build/l5_stability` about 867 ms vs about 1 ms, 5 runs (run) |
| ranked: nth/rank O(log n) | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnk_indices.html (opened) |
| max_load_factor 1.0; reserve = rehash(ceil(n/mlf)) | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html (opened) |
| container inherits index 0's interface (so `s.reserve` = index 0 only) | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html (opened); `checks/tpl/l3-reserve.cpp` (run) |
| Allocator is the 3rd template parameter, default std::allocator | multi_index_container.html (opened); pmr variants compile and run (`bench_scatter`, `bench_rehash`, `capstone`) |
| rehash spike table (median of 5: mean 52, max 8.4 ms; reserve; arena) | `build/bench_rehash` (run 5 times; bench/results/) |
| ordered-index rebalancing is bounded (at most 2 rotations per insert) and shows no repeatable spike | `detail/ord_index_node.hpp` `rebalance` (read, insert path only); `build/bench_ordered` (run 5 times) |
| `l1_handrolled bug` prints 710 (allocator-dependent), `stale` prints the stale-key line, ASan reports heap-use-after-free | `_evidence/l1-fix-runs.txt` (run) |
| dependent lookup table at 10k / 4M (p50, median of 5) | `build/bench_lookup` (run 5 times) |
| traversal 6.6 vs 81 ns/node; arena 81 | `build/bench_scatter` (run 5 times) |
| modify vs replace table (median of 5, ratios) | `build/bench_modify` (run 5 times) |
| unordered_flat_map open addressing; no pointer stability under rehash | https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html (opened) |
| intrusive: no memory management; user owns lifetime; object in several containers | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html (opened) |
| capstone table (median of 5); all 4 variants agree | `build/capstone` (5 runs), `build/capstone selftest` (run, exit 0) |

Stories: the Lesson 02 cancel/replace incident is labelled hypothetical. No real incidents
are cited.
