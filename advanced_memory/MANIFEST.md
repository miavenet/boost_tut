# Pooled, hierarchical memory for hot-path containers — use it, open it, rebuild it

format-version: 2

## Subject & scope
- **Subject:** giving any hot-path container (Boost.MultiIndex, `boost::container::flat_map`,
  `boost::unordered_flat_map`, std containers) a pooled, hierarchical memory supply.
- **Grain:** given a container on the hot path, give it the right memory supply the EASIEST
  correct way: through a small library (`labs/hft_mem/hft_mem.hpp`: one `thread_arena` per thread
  + aliases such as `hft::map`, `hft::flat_map`, `hft::multi_index`). The goal is that the hot path
  never calls malloc, nodes sit close together, and memory lives on the NUMA node of the thread
  that uses it. Order: USE the library (lesson 01) → open each layer (02, 03) → traps, locality,
  rebuild one layer (04).
- **Prerequisites:** fluent C++17 (std::map / unordered_map / vector, allocators at the "I know
  they exist" level, move semantics, threads). Toolchain: a C++23 compiler and Boost ≥ 1.83
  headers (authored with Apple clang 21 + Boost 1.90; Linux parts also compiled with GCC 13.3 and
  clang 18 + Boost 1.83). `libboost_container` for one lab. Production HFT boxes are x86-64 Linux;
  the authoring Mac is not: every measured number is labelled with its machine.
- **Retention target:** use daily at work; spaced reviews over ~3 weeks.

## Time cost (honest estimate, not yet measured on a learner)
- **First pass:** ~110 min estimate = 4 sessions of 25–30 min (lesson 01 ~25, 02 ~25, 03 ~30,
  04 ~30). Each session: 3 pretests, ≤6 concepts with a run-it step, 1–3 completion cards,
  5–7 question cards, 1 synthesis card. Compiling checks adds ~1–2 s per graded code answer.
- **One-time setup:** `bash labs/build.sh` ≈ 15 s here.
- **Per spaced review:** ~5–8 min estimate × 4 reviews over ~3 weeks (1/2/7/21-day boxes).
- **Interruptible:** yes — resumable from `progress.json` after any instructional step.
- **Lessons 05–07 (intrusive containers over a pool, added 2026-11-14):** ~90 min estimate more =
  3 sessions of ~30 min, same shape (3 pretests, ≤6 concepts, 2 completion, 5–8 questions, 1
  synthesis; the second completion card and the chunked concepts, added after the first draft,
  add an estimated 1–2 min per lesson). `bash labs/build.sh` now takes ≈ 35 s here. The full intrusive bench suite
  (`bash bench/run_intrusive.sh`, optional) took about 1.5 minutes here, build included.
- **How the driver spreads it (T1 defaults):** `--new-limit 6` new concepts per date (lesson
  sizes 4, 6, 5, 6, 6, 6, 5 concepts), so the first pass is 7 study days (scripted replay:
  2026-11-16..22); a short lesson's leftover budget starts the next lesson, and nothing from a
  lesson not taught that day is shown. Confidence is asked before every key (`--no-ask-confidence`
  opts out); a `sure` / >= 0.7 miss is flagged for hypercorrection and re-tested first.
- **Delayed assessment:** once, >= 7 days after lesson 07: `assessment/form.json` (28 unseen
  items, a retention and a transfer item per objective), ~45 min estimate; T1 `--assessment`,
  T0 `assessment/FORM.md`.

## Learning objectives (measurable condition, performance and criterion)
1. Given a latency report or histogram from a hot loop, the learner can name the allocator
   mechanism behind the tail (first-touch page fault, growth/rehash copy, pool refill, lock) and
   the fix, matching the key's mechanism.
2. Given a std or Boost container on a hot path, the learner can switch it to pooled memory with
   `hft_mem` (arena + alias, plus `reserve()` for flat containers) so that a compiled test counts
   zero global `operator new` calls in the hot loop.
3. Given a test, the learner can turn an unexpected hot-path allocation into an immediate failure
   (`arena.seal()`, `-DHFT_MEM_TRIPWIRE=1`, a `null_memory_resource()` upstream) and explain why
   `set_default_resource` is the wrong tripwire in a multithreaded binary.
4. Given a chain of std::pmr resources, the learner can predict what allocate/deallocate do at
   each layer and write a buffer → monotonic → pool chain that a compiled test accepts.
5. Given a design constraint, the learner can choose between std::pmr, boost::container::pmr,
   `small_vector`/`static_vector` and Boost.Pool, naming the cost of each choice.
6. Given a fixed region or a fixed block size, the learner can write the overriding functions of a
   custom `memory_resource` (bump region, free list) that pass compiled alignment/exhaustion/LIFO
   tests.
7. Given a Linux NUMA box, the learner can order the pin → mmap → mbind → touch → hot-path steps
   (checked by a script), say what pre-fault, huge pages and mlock each buy, and name three ways to
   verify placement.
8. Given code that uses a stateless or stateful allocator, the learner can diagnose the
   behind-the-scenes traps (lifetime, other-thread free, static init, copy/move propagation,
   nested strings) and write the correct type (compiled test).
9. Given a hot-path container and its access pattern, the learner can pick its memory supply from
   the decision table and the pool layout (per-container vs shared) for locality, and lay out
   per-thread counters that cannot false-share (compiled test).
10. Given an object that sits in several hot-path lists, the learner can answer "is it in list L?"
    from the object in O(1) with a hook, choosing the link mode and hook layout so the answer is
    reliable and names the right list (compiled tests: stale enum, shared venue hook).
11. Given an operation on an intrusive, value or pointer container, the learner can state its
    complexity (O(1) given the object vs O(log n) / average O(1) / O(n) by key or position) and its
    allocations, pointer chases and bytes per object, matching the measured counts.
12. Given an intrusive design over a slab, the learner can write the lifetime-correct operations
    (cancel, dispose, handle check, index-link unlink) that pass compiled tests, and diagnose the
    five traps (destroy-while-linked, owner dies first, auto_unlink, ABA, erase while iterating).
13. Given an HFT structure (order book, timer wheel, pending-ack queue, LRU), the learner can build
    it as intrusive lists over a slab with O(1) state transitions and no hot-path allocation
    (compiled test).
14. Given a workload, the learner can choose intrusive + slab, multi_index + pool, flat containers or
    plain std, and Boost.Intrusive vs hand-rolled parts, citing measured ratios and mechanisms, and
    say when intrusive is the wrong choice.

## Tiers supported
```
tiers: [T0, T1, T2, T3]        # see guidelines/DUAL-MODE.md (the durable-learning method)
detect:
  - model + file/exec tools -> T3
  - model, no tools         -> T2
  - python3 drive.py        -> T1
  - else                    -> T0 (self-quiz from STUDY.md)
```
T1: `python3 drive.py lessons --progress progress.json --today YYYY-MM-DD` (asks confidence
before each key by default; add `--assessment` for the delayed form). Code answers are
graded by `checks/*.sh`, which compile and run them (needs clang++ and Boost headers; set
`CXX`/`BOOST_INC` if yours differ). Conceptual answers are self-graded against the full key.

## Files
- `lessons/*.md` — concept + question cards (source of truth)
- `labs/hft_mem/hft_mem.hpp` — the library; `labs/*.cpp` — one small program per idea; `labs/build.sh`
- `bench/b*.cpp`, `bench/run.sh`, `bench/RESULTS.md` (median of 3 runs), `bench/results/run*.txt` (raw)
- `bench/run_r2.sh`, `bench/summarize_r2.py`, `bench/results_r2/` — the 2026-11-14 re-measurement
  quoted by lessons 01–04 (5 raw runs per program, RESULTS.md = median [min–max])
- `bench/asm_alloc.sh` + `bench/asm_alloc_paths.cpp` — disassembly of the stateful vs stateless path
- `tests/test_hft_mem.cpp`, `tests/run.sh` — library regression tests (built by labs/build.sh, run
  by checks/selftest.sh); `_evidence/` — recorded outputs the lessons cite
- `checks/*.sh` + `checks/lib.inc` — executable graders (exit 0 = pass); `checks/selftest.sh` +
  `checks/fixtures/` (right/wrong answers incl. adversarial answers, regex-card cases)
- `STUDY.md` — generated complete paper artifact with separated answer keys
- `drive.py`, `tutorial_format.py`, `v2_session.py` — T1 driver bundle; `templates/` holds four
  symlinks so the unmodified `evals/audit.py` finds them
- `PROTOCOL.md` — T2/T3 model state machine
- `assessment/form.json` + generated `assessment/FORM.md`, `v2_assessment.py` — the delayed,
  unseen retention + transfer form (prerequisites L1C1 … L7C5, one concept per objective)
- `evals/replays/*.replay` + `evals/replay-all.sh` — recorded zero-model run of the whole course
  (9 study days + the assessment), replayed into fresh progress; `evals/record_replay.py`
  re-records it (scripted learner, real check answers from `checks/fixtures/`)
- `progress.json` — learner state (portable, private; created on first run)

## Coverage map (objective → cards)
| Objective | Cards | Lowest tier that can grade it |
|-----------|-------|-------------------------------|
| 1 tail mechanisms | 01-P1, 01-Q1 (recall), 01-Q5 (apply) | T0/T1 self-report (01-Q5 regex dropped: free text cannot be graded honestly) |
| 2 use the library | 01-P2, 01-C2, 01-Q4 (recall), 01-Q2 (apply) | T1 (01-Q2 check: compiles, counts operator new) |
| 3 tripwire | 01-P3, 01-C1, 02-C2, 01-Q3 (recall), 02-Q5 (apply) | T1 (01-C1, 02-C2 checks); self-report for 01-Q3, 02-Q5 |
| 4 pmr layers | 02-P1, 02-P2, 02-C1, 02-C3, 02-Q1 (recall), 02-Q2 (apply) | T1 (02-Q2 check; 02-C1, 02-C3 regex+guard) |
| 5 Boost options | 02-P3, 02-Q3 (recall), 02-Q4 (apply) | T0/T1 self-report |
| 6 custom resource | 03-C1, 03-Q2 (recall), 03-Q1 (apply), 04-Q6 (apply) | T1 (03-C1, 03-Q1, 04-Q6 checks) |
| 7 placement | 03-P1, 03-P2, 03-Q3 (recall), 03-Q5 (recall), 03-Q4 (apply), 03-Q7 (apply, huge pages) | T1 (03-Q4 check); self-report for 03-Q3, 03-Q5, 03-Q7 |
| 8 behind-the-scenes traps | 03-P3, 04-P1, 04-P3, 04-C1, 04-Q2 (recall), 04-Q1 (apply), 04-Q3 (apply), 03-Q6 (apply) | T1 (04-Q1, 04-Q3 checks; 04-C1 regex+guard) |
| 9 locality and choice | 04-P2, 04-Q4 (recall), 04-Q5 (apply), 04-Q7 (apply) | T1 (04-Q5 check); self-report for 04-Q4, 04-Q7 |
| 10 membership by hook | 05-P1, 05-P2, 05-P3, 05-C1, 05-Q8 (recall), 05-Q6 (apply), 05-Q1 (apply), 05-Q4 (apply), 05-Q7 (apply) | T1 (05-Q1, 05-Q4 checks; 05-C1 regex+guard); self-report for 05-Q6, 05-Q7, 05-Q8 |
| 11 complexity and cost | 05-Q3 (recall), 05-Q5 (recall), 06-P1, 06-P2, 06-Q1 (recall), 06-Q2 (recall), 05-C2, 05-Q2 (apply) | T1 (05-C2, 05-Q2 checks); self-report for recall |
| 12 lifetime-correct operations | 06-P3, 06-C1, 06-C2, 06-Q4 (recall), 06-Q3 (apply), 06-Q5 (apply), 06-Q6 (apply), 06-Q7 (apply) | T1 (06-C2, 06-Q3, 06-Q5, 06-Q6 checks; 06-C1 regex+guard) |
| 13 HFT patterns | 07-P1, 07-C2, 07-Q1 (recall), 07-Q2 (apply), 07-Q5 (apply) | T1 (07-C2, 07-Q2 checks); self-report for 07-Q1, 07-Q5 |
| 14 choice and verdict | 07-P2, 07-P3, 07-C1, 07-Q3 (recall), 07-Q4 (apply), 07-Q6 (apply) | T1 (07-C1 regex+guard); self-report for the rest |
| synthesis | 01-INT, 02-INT, 03-INT, 04-INT, 05-INT, 06-INT, 07-INT | T0 self-report |

Transfer items vary a real dimension: 01-Q5 reads a different container's bench line; 02-Q4 moves
to Boost.Pool in a two-thread list; 02-Q5 to a test-runner; 03-Q6 to 2,000 small maps; 04-Q7 to two
different threads' access patterns; 04-Q5 to the machine you are on (64 vs 128 B lines);
05-Q6 to a risk engine's breached-account set; 05-Q7 to a market-data fan-out of 64 gateways;
06-Q7 to a shared-memory snapshot for a risk process; 07-Q4 to a per-symbol quote cache;
07-Q5 to throttle timers in a risk check; 07-Q6 to a colleague's hand-roll proposal.
Delayed, unseen form: `assessment/form.json` — 14 retention items (recall in new wording) and
14 transfer items in a new setting (audit log, web-server parser, telemetry module, device
staging buffer, database buffer pool, config reload, physics particles, ride-hailing dispatcher,
editor undo history, network proxy, session store, reference data). Keys use only lesson facts.
Spaced re-asking of the lesson cards is practice, not the outcome measure.

## Sourcing (claims ledger)
Every measured number: Apple M1 Max (8P+2E), macOS (Darwin 27), Apple clang 21.0.0, libc++,
Boost 1.90, `-std=c++23 -O2`, page 16 KiB, line 128 B, `steady_clock` step ≈ 41 ns, measured
2026-11-13 on a shared (not isolated) machine; lessons 01–04 re-measured 2026-11-14 (5 runs,
bench/results_r2/). N is printed in each output line.

| Claim | Grounding |
|---|---|
| std::pmr (`<memory_resource>`) compiles and runs here; `__cpp_lib_memory_resource` = 201603 | compiled + ran probe, 2026-11-13 |
| boost::container::pmr resources need `-lboost_container` (link error text quoted) | link failure recorded in _evidence/r2/link-error.txt; https://www.boost.org/doc/libs/1_90_0/doc/html/container/cpp_conformance.html |
| boost vs std memory_resource are different types; mixing fails "no matching constructor" | `static_assert` in labs/l2_boost_pmr.cpp; compile error observed |
| boost flat_map works with `std::pmr::polymorphic_allocator<std::pair<K,V>>` | labs/l2_boost_pmr.cpp, bench/b1 |
| static_vector overflow throws boost::container::bad_alloc, not caught as std::bad_alloc | labs/l2_boost_pmr.cpp run |
| monotonic: dealloc no-op, release(), geometric growth, not thread-safe | https://en.cppreference.com/w/cpp/memory/monotonic_buffer_resource ; labs/l2_layers.cpp |
| pool resources: pools by block size, chunks, oversize → upstream, (un)synchronized | https://en.cppreference.com/w/cpp/memory/unsynchronized_pool_resource ; .../synchronized_pool_resource |
| pool refills in the loop: uncapped (max_blocks_per_chunk=0) max 273–403 µs; capped at 256: max 7.6–11 µs but p99.9 1.0–3.9 µs; warm() removes both (one flag changed per run) | labs/l1_tail_histogram modes nowarm / nowarm nocap; bench/results_r2/RESULTS.md (5 runs each); https://en.cppreference.com/w/cpp/memory/pool_options (0 = implementation limit) |
| null_memory_resource throws std::bad_alloc; set_default_resource process-wide, returns previous, initial new_delete | https://en.cppreference.com/w/cpp/memory/null_memory_resource ; https://en.cppreference.com/w/cpp/memory/set_default_resource ; labs/l2_layers.cpp |
| do_allocate/do_is_equal signatures and rules | https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate ; .../do_is_equal |
| polymorphic_allocator: implicit from memory_resource*, default uses get_default_resource, no propagation, uses-allocator construct, copy → default-constructed | https://en.cppreference.com/w/cpp/memory/polymorphic_allocator (+ /polymorphic_allocator, /select_on_container_copy_construction) ; labs/l4_traps.cpp |
| multi_index Allocator is last template arg; assignment follows propagate traits | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html |
| unordered_flat_map signature; reserve/rehash invalidate; pointer stability not kept on rehash | https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html |
| Boost.Pool: singleton memory kept after container ("Exiting the function does NOT free..."), ordered free O(N), fast_pool_allocator for list / pool_allocator for vector | https://www.boost.org/doc/libs/1_90_0/libs/pool/doc/html/boost_pool/pool/interfaces.html ; .../boost/fast_pool_allocator.html ; labs/l2_boost_pool.cpp |
| Pool allocators' default Mutex = std::mutex here; pool_allocator uses ordered_malloc/ordered_free | /opt/homebrew/include/boost/pool/detail/mutex.hpp (default_mutex), poolfwd.hpp, pool_alloc.hpp lines 247/272; static_assert in labs/l2_boost_pool.cpp |
| glibc creates extra arenas on mutex contention | https://man7.org/linux/man-pages/man3/malloc.3.html (NOTES) |
| first touch: 4,096 faults on 64 MiB, ≈1 µs/page (1.0–1.1 in 5 runs); mlock(64 MiB) returns 0 here | labs/l3_first_touch.cpp; bench/results_r2/RESULTS.md |
| MAP_HUGETLB, MAP_POPULATE, MAP_HUGE_2MB, EPERM rule | https://man7.org/linux/man-pages/man2/mmap.2.html (read, NOT executed here) |
| MADV_HUGEPAGE (CONFIG_TRANSPARENT_HUGEPAGE, aligned boundaries), MADV_POPULATE_WRITE since 5.14 | https://man7.org/linux/man-pages/man2/madvise.2.html (NOT executed) |
| THP enabled modes, AnonHugePages in smaps; nr_hugepages, HugePages_Total/Free | https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html ; .../hugetlbpage.html (NOT executed) |
| default policy = local allocation on the CPU that triggered it | https://man7.org/linux/man-pages/man2/set_mempolicy.2.html |
| mbind MPOL_BIND, -lnuma, numaif.h, only new allocations; nodemask "up to maxnode bits" (hft_mem: 128-bit mask, nodes 0..126) | https://man7.org/linux/man-pages/man2/mbind.2.html (compiled + ran in Linux container 2026-11-13 and 2026-11-14: EPERM there; NUMA not verified; _evidence/r2/linux-container.txt) |
| numa_alloc_onnode rounding, slow, numa_free; numa_node_of_cpu | https://man7.org/linux/man-pages/man3/numa.3.html (container: returned -1) |
| move_pages with nodes NULL reports current node | https://man7.org/linux/man-pages/man2/move_pages.2.html (container: status -1) |
| numa_maps N<node>= fields; numastat -p and counters | https://man7.org/linux/man-pages/man7/numa.7.html ; https://man7.org/linux/man-pages/man8/numastat.8.html (NOT executed) |
| pthread_setaffinity_np signature, _GNU_SOURCE, CPU_SET; CPU_SETSIZE "one greater than the maximum CPU number" | https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html ; https://man7.org/linux/man-pages/man3/CPU_SET.3.html (compiled + ran in container; pin_this_thread(-1)/(1<<20) return false there) |
| mlock residency, RLIMIT_MEMLOCK, CAP_IPC_LOCK | https://man7.org/linux/man-pages/man2/mlock.2.html |
| hardware_destructive_interference_size = 256, constructive = 64 (libc++); GCC 13 aarch64 = 256; clang 18 + libstdc++: not defined | probe + bench/b3 on macOS and in Linux container; https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size |
| stateless allocator compiles to a direct branch to unsynchronized_pool_resource::do_allocate; TLS read is `blr`; stateful is `br` through the vtable (libc++ names) | bench/asm_alloc.sh → _evidence/r2/asm-alloc.txt (2026-11-14) |
| node sizes (map 64, unordered_map 48 libc++ / 40 libstdc++, unordered_node_map 32, 2-index multi_index 64) | labs/l4_node_size.cpp on macOS and in Linux container |
| all latency/percentile numbers in lessons 01–04 | bench/results_r2/RESULTS.md (median [min–max] of 5) where re-measured; else bench/RESULTS.md (median of 3), bench/results/run*.txt |
| lab exit codes quoted (139, 134, 3) and `tiny` output | _evidence/r2/labs-raw-rc.txt (raw $?, no pipe) |
| every alias + scratch + pin_this_thread exercised | labs/l1_every_alias.cpp (lab) ; bench/b1_containers.cpp (a)/(e) rows incl. hft::string ; pin: called, returns false on macOS, no latency row possible here |
| is_always_equal = true holds only on one thread under one arena | labs/hft_mem/hft_mem.hpp comment ; tests/run.sh cross-thread-after-overflow ; https://en.cppreference.com/w/cpp/named_req/Allocator |

## Lessons 05–07: intrusive containers over a pool (added 2026-11-14)
Grain: given objects that live in several lookup structures at once on a hot path (price-level
FIFO, per-client list, id index, state lists, timers), decide between intrusive containers over a
pooled slab and value-owning containers, build the intrusive version correctly on top of
`hft_mem`, and predict its cost per operation, per object and in the tail. Headline use case:
O(1) state membership given the order ("in the active list?", "queued for venue X?").

New library parts: `labs/hft_mem/hft_slab.hpp` (`hft::slab<T>`: fixed capacity, carved once from
the thread arena, O(1) LIFO free list, `try_create`/`create` exhaustion policy, generation
handles), `labs/hft_mem/hft_book.hpp` (example intrusive order book),
`labs/hft_mem/hft_handroll.hpp` (minimum hand-rolled lists, index list, hash index, pool, for the
Boost-vs-hand comparison). New files: `lessons/lesson-05..07.md`, `labs/l5_*.cpp`, `l6_*.cpp`,
`l7_*.cpp`, `bench/b4_membership.cpp`, `b5_book.cpp`, `b6_handroll.cpp`, `bench/asm_hot_ops.cpp`,
`asm_count.py`, `asm_intrusive.sh`, `ct_boost.cpp`, `ct_hand.cpp`, `compile_time.sh`,
`run_intrusive.sh`, `summarize_intrusive.py`; results in `bench/results_intrusive/` (run1..5 raw,
RESULTS.md medians + ranges, floor.txt, asm.md, compile_time.txt, labs-output.txt);
`checks/l5_*.sh`, `l6_*.sh`, `l7_*.sh`, `checks/selftest.sh` + `checks/fixtures/`.

Measured 2026-11-14 on the same M1 Max, Apple clang 21.0.0, Boost 1.90, `-std=c++23 -O2`
(benches `-DNDEBUG`), shared machine with a Time Machine backup in its "Copying" phase during
every bench run (recorded per run), load average 2.6–3.7. Each bench row ran in its own process.

| Claim (lessons 05–07) | Grounding |
|---|---|
| is_linked precondition safe_link/auto_unlink, "true, if the node belongs to a container", constant | /opt/homebrew/include/boost/intrusive/list_hook.hpp 144–151; normal_link compile error: detail/generic_hook.hpp 204 (compiled, error quoted) |
| safe mode: default state, destructor assert, erase resets; asserts are BOOST_ASSERT | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html ; detail/assert.hpp 26–40 ; boost/assert.hpp ; runs of l6_traps with and without NDEBUG |
| default link mode safe_link; two base hooks need different tags | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage.html ; options.hpp 263 |
| auto_unlink: self-unlink, incompatible with constant_time_size (static assert), thread-unsafe, "surprising effects" | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/auto_unlink_hooks.html ; list.hpp 125 (error quoted) ; l5_link_modes, l6_traps autounlink |
| ~list linear for safe/auto modes, else constant; iterator_to constant | list.hpp 249–257, 1301–1309 ; l6_traps container-dies (1,426–1,458 µs vs 0.0–1.7 µs for 1,000,000; 12 runs in _evidence/r3/lab-repeats.txt + labs-output-l5-l7.txt) |
| s_iterator_to for list/slist/set/multiset; not for unordered | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/obtaining_iterators_from_values.html |
| slist erase linear in elements before i | slist.hpp 987 ; b6 slist erase row |
| unordered_set: user bucket array, no automatic rehash, bucket lifetime, power_2_buckets, store_hash, optimize_multikey | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/unordered_set_unordered_multiset.html ; l6_buckets |
| key_of_value requirements, find(key); heterogeneous find(key, hash, eq) | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/map_multimap.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/advanced_lookups_insertions.html |
| optimize_size packs the colour bit (3 pointers) | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/set_multiset.html ; l6_hooks (24 vs 32 B) |
| container families and per-element overhead | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html ; treap_set.hpp / splay_set.hpp / sg_set.hpp present locally ; bstree.hpp 66 |
| erase_and_dispose / clear_and_dispose / disposers | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/erasing_and_disposing.html ; l6_buckets, l6_dispose check |
| intrusive advantages/disadvantages; when (not) to use; avoid in public interfaces | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage_when.html |
| offset_ptr hooks via void_pointer, shared-memory ready | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html ; l6_index_list (memcpy copy walks) |
| custom NodeTraits for node algorithms | https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/node_algorithms.html (read; an index-based NodeTraits was NOT built) |
| 28 option macros | /opt/homebrew/include/boost/intrusive/options.hpp (grep BOOST_INTRUSIVE_OPTION) |
| unordered_flat_map max_load "may decrease on erasure under high-load conditions"; reserved-for-live churn reallocates 1 MiB 9× in 200,000 ops, 0× with 2× reserve | https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; labs/l6_flat_drift.cpp ; b6 flatmap rows (max 531 µs) |
| object_pool::destroy → ordered_free, O(N) in free-list length; bimodal across processes, correlated with address order of free blocks | /opt/homebrew/include/boost/pool/object_pool.hpp 126, pool.hpp 480 ; labs/l7_ordered_free.cpp (10 runs in _evidence/r3/lab-repeats.txt: 7 × 65.3% → 1,429–1,470 ns, 3 × 0.0% → 4.0 ns); slow b6 processes: ordered_free 4/5 + 3/5, object_pool 3/5 + 2/5 (results_intrusive, results_intrusive_r2 run*.txt) |
| ordered index insertion log(n) | https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html |
| hook sizes, model counts, traps, patterns | lab outputs in bench/results_intrusive/labs-output.txt ; after the header fixes: _evidence/r3/labs-output-l5-l7.txt, repeats in _evidence/r3/lab-repeats.txt |
| instruction counts and excerpts | bench/results_intrusive/asm.md (static counts from clang -S, regenerate with bench/asm_intrusive.sh) |
| compile times and lines of code | bench/results_intrusive_r2/compile_time.txt (after the fixes: 224 + 205 lines) |
| all percentile numbers in lessons 05–07 | bench/results_intrusive/RESULTS.md (median of 5, min..max ranges), run1..5.txt, floor.txt; the b5 table and pool rows: bench/results_intrusive_r2/ (5 runs after the header fixes, with same-session pre-fix rows) |
| library fixes (slab ctor-throw, one allocation, double destroy, forged handles; book 2× id reserve, duplicate id, range, transitions; hr copy/move, ctor-throw, bucket count) | tests/test_intrusive.cpp via bash tests/run.sh; `HFT_INC=<older headers>` shows each test failing on pre-fix headers |

Causes NOT isolated (taught as such): A vs A2 walk difference; LIFO vs FIFO-ring gap (different
bookkeeping; the slab's create-side generation store IS isolated: ~1.7 ns, _evidence/r3/lab-repeats.txt);
bi::unordered_set erase+insert ≈ 12 ns slower than the hand hash index; hook vs enum membership
gap (b4); multi_index cancel with vs without the pool; A's cancel 2× vs 1× id reserve (spreads
overlap, _evidence/r3/b5-A-isolation.txt); the exact fault address in the `iterate` trap. Linux / x86 numbers were not measured for lessons 05–07.
