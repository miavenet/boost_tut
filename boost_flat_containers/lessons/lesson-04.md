# Lesson 04 — Pick the right table, then kill a tail spike (capstone)

You now know what a flat_map costs. The professional question is a different
one: *should this table be a flat_map at all?* Sometimes the best data
structure is no data structure. Instrument ids handed out densely by your own
reference-data service are just array indices. Sometimes it is a hash table
with a 9.6 ms rehash hiding in it. Sometimes it is a sorted array laid out in a
different order. Then you will take a realistic order-entry loop, watch one
intraday listing produce a quarter-millisecond spike, and remove it.

Start now (≈1 min): `labs/bin/capstone`. Look at the `max` and the last
histogram rows. Keep that output; you will beat it.

## Pretest — commit your guesses FIRST

### CARD 04-P1  [type: pretest]  [topic: alternatives]  [box: 1]
Q: Cold guess. Instrument ids are dense, 0..999,999. Per lookup (p50 of batches),
   how do `table[id]` on a `std::vector`, `flat_map::find` and
   `boost::unordered_flat_map::find` compare?
Key:
  - Measured at N=1M: direct index about 3.3 ns, unordered_flat_map about 14 ns, flat_map about 115 ns, i.e. direct is ~4x faster than the hash and ~35x faster than flat_map (bench/results/median/direct_index.ndebug.txt; flat_map ranged 113–124 over 5 runs). Dense keys are an array index. Do not search for what you can compute. CONCEPT L4C1.
Concepts: L4C1
Source: bench/direct_index.cpp; bench/results/median/direct_index.ndebug.txt

### CARD 04-P2  [type: pretest]  [topic: hash]  [box: 1]
Q: Cold guess. Inserting 1,048,576 random keys into an un-reserved
   `boost::unordered_flat_map`: mean insert ~40 ns. What is the max single insert:
   ~1 µs, ~100 µs, or ~10 ms?
Key:
  - ~10 ms (about 9.6 ms measured, 9.35–9.84 over 5 runs; 17 rehashes). Each rehash moves every element into a new bucket array. With `reserve(N)` first, the max was only ~5 µs (4.0–6.9 µs over 5 runs; noise, no rehash). CONCEPT L4C2.
Concepts: L4C2
Source: bench/insert_tail.cpp; bench/results/median/insert_tail.ndebug.txt

### CARD 04-P3  [type: pretest]  [topic: eytzinger]  [box: 1]
Q: Cold guess. 16M sorted `uint64_t` keys (128 MiB). Binary search on the sorted
   array takes ~500 ns per lookup. Same keys, same comparisons, stored in
   breadth-first (Eytzinger) order with a prefetch: ~450 ns, ~250 ns, or ~100 ns?
Key:
  - ~100 ns (about 94 vs 497 ns measured: ~5.3x; 5-run ranges 94–97 and 491–511). The layout turns scattered probes into predictable, prefetchable ones. CONCEPT L4C3.
Concepts: L4C3
Source: bench/eytzinger.cpp; bench/results/median/eytzinger.ndebug.txt

---

## Concept cards

### CONCEPT L4C1  The cheapest lookups: a direct index, or a scan of a tiny array
If keys are dense small integers you control (instrument ids, session ids, venue
ids), make them array indices: `table[id]` is one address computation and one
load. A bounds check `id < table.size()` is the only logic. Measured (dense ids,
24-byte records, p50 ns): N=2,000: 0.7 (direct) vs 16.3 (flat_map) vs 2.6
(unordered_flat_map). N=1M: 3.3 vs 115.2 vs 13.7 (per-cell median of 5 runs; flat_map 113–124 over the 5 runs). These are batch averages of
*independent* lookups, so the core overlaps them and the result is throughput.
One isolated lookup is never below a load latency. If ids are sparse, a remap
step (exchange symbol → dense internal id, done once at subscription time)
gets you back to direct indexing. For **tiny** tables, a linear scan is the
other "no structure" option. In this bench a naive scan beat binary search only
with 32-byte keys at N ≤ 16 (lesson 1). A SIMD key-only scan was not measured here.
Elaborate: Why is a dense-id remap at subscription time worth its complexity on a hot path?
ElaborateKey:
  - It moves the search (hash or binary) off the hot path, where it runs once per symbol, and leaves one bounds-checked array load per order on the hot path.
Source: bench/direct_index.cpp; bench/results/median/direct_index.ndebug.txt

### CONCEPT L4C2  Open addressing: boost::unordered_flat_map / unordered_node_map
`boost::unordered_flat_map` (`<boost/unordered/unordered_flat_map.hpp>`) uses
open addressing: "the elements … are held directly in the bucket array". It was
the fastest point lookup at every N in lesson 1's bench. Its costs: **no
order** (no range queries), and "pointer stability is not kept under
rehashing". The bucket array grows on insert once the load would exceed
`max_load_factor()`, and that factor is "managed internally and can't be set by
the user" (the setter "does nothing"). That growth is a rehash spike. Measured,
1M inserts: 17 rehashes, mean 38.5 ns, **max 9.64 ms** (9.35–9.84 over 5 runs). With `reserve(N)` first
the max was about 5 µs (4.0–6.9 µs over 5 runs, noise). `reserve(n)` is defined as `rehash(ceil(n / max_load_factor()))`.
If you need references that survive rehashing, `boost::unordered_node_map`
(`<boost/unordered/unordered_node_map.hpp>`) gives pointer stability "at the
expense of being slower" (Boost's words; not benched here).
Elaborate: Lesson 3's flat_map reallocation and this rehash are the same kind
of tail spike. Name what they share and the shared fix.
ElaborateKey:
  - Both are rare whole-container moves triggered by growth (size reaching capacity or the max load), which the mean hides and the max exposes.
  - Shared fix: size for the session's maximum at startup with reserve(), so growth never happens on the hot path.
Source: https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/intro.html ; bench/results/median/insert_tail.ndebug.txt

### CONCEPT L4C3  Large sorted sets: change the layout (Eytzinger, B-tree)
At millions of keys, a sorted array's binary search jumps across the whole
array, and each of the deeper probes is a likely cache miss whose address
depends on the previous comparison. The **Eytzinger** layout stores the same
keys in breadth-first (heap) order: the children of slot k are 2k and 2k+1. The
next probes are therefore at predictable addresses that can be prefetched
several levels ahead, and the comparison becomes `k = 2k + (b[k] < x)` with no
branch. Khuong & Morin ("Array Layouts for Comparison-Based Searching",
arXiv:1509.05053) found sorted order with a good binary search best for small
arrays and Eytzinger "usually the fastest" for larger ones. B-tree layouts
(many keys per node, one cache line or more) are the other cache-friendly
option and were not benched here. Measured p50 ns, uint64 keys (Apple M1 Max, Apple clang 21, `-O2 -DNDEBUG`, per-cell median of 5 runs after adding a warm-up loop to bench/eytzinger.cpp; N=1M `lower_bound` ranged 86–100 over the 5 runs and `flat_set::find` 92–106, so the first two columns are a tie and the Eytzinger gap is the claim):

| N | std::lower_bound | boost flat_set::find | Eytzinger + prefetch |
|---|---|---|---|
| 4,096 | 16.9 | 18.2 | 11.7 |
| 1,048,576 | 93.8 | 100.9 | 39.7 |
| 16,777,216 | 496.8 | 485.0 | 93.8 |

The cost: the layout is static. An insert means a rebuild, so it suits sets
built once per session.
Elaborate: Why does Eytzinger help most at 16M keys and least at 4,096?
ElaborateKey:
  - At 4,096 keys (32 KiB) everything fits in L1/L2, so there are few misses to hide. At 16M keys (128 MiB) nearly every deep probe misses cache, and Eytzinger's prefetchable, branch-free path overlaps those misses.
Source: https://arxiv.org/abs/1509.05053 ; bench/eytzinger.cpp; bench/results/median/eytzinger.ndebug.txt

### CONCEPT L4C4  Honest pros/cons, and when NOT to use a flat_map
| | flat_map (Boost) | std::map | boost::unordered_flat_map | direct array |
|---|---|---|---|---|
| point lookup (N=4,096, measured) | 19.5 ns | 61.2 ns | 3.2 ns | – (dense ids only) |
| ordered iteration / range query | yes, contiguous | yes, pointer chasing | no | by index |
| insert/erase | O(N) moves | O(log N) + 1 allocation | O(1) avg, rehash spike | O(1) |
| stable references | no | yes | no (node_map: yes) | yes (fixed size) |
| heap allocations | per growth (0 with static_vector) | per element | per growth | none after startup |

**Do not use flat_map when:** the table is write-heavy on the hot path
(random-position inserts cost µs at 10k entries); N is large *and* writes are
frequent (shift cost grows linearly with N); you need **stable references or
iterators** across mutations (use a node container, or store keys); or you
only need point lookups (a good hash table won every row). **Use it when** the table is
read-mostly, needs sorted order or ranges, is small-to-medium, and can be built
in bulk and reserved.
Elaborate: A colleague proposes flat_map for a live order book keyed by order id
with ~50k resting orders, inserts and cancels at random ids, plus pointers from a
FIFO queue. List every reason this is a poor fit.
ElaborateKey:
  - Random-position inserts and erases on ~50k entries shift ~25k elements per event (µs each, worse than std::map's ~200 ns at 10k).
  - The FIFO's pointers dangle on every insert/erase (no reference stability).
  - Order-id lookups need no ordering, so a hash table (or a dense-id slot array) fits better.
Source: CONCEPTS L1C2–L1C3, L2C1, L4C1–L4C3; bench/results/*.ndebug.txt

### CONCEPT L4C5  Capstone: an order-entry table pair with a deliberate tail spike
`labs/capstone.cpp` builds the two tables an order-entry path needs: an
instrument-id → `InstrumentConfig` table (dense ids → direct index,
`ConfigTableT`) and a symbol → `RiskLimit` table (sparse 8-byte symbols →
`RiskTableT`, a Boost flat_map). The start-of-day risk table is built with
`adopt_sequence` and then `shrink_to_fit`, so capacity == size. Every 4,000th
order, a "new listing" inserts into it **on the hot path**. Shipped run (100,000
symbols, 400,000 orders; Apple M1 Max, Apple clang 21, `-O2 -DNDEBUG`, 5 runs, median run; the spread is
in bench/results/median/SPREAD.txt): p50 83 ns (identical in all 5 runs), mean 81.2 ns (80.9–81.6),
**max 232 µs** (204–262 µs), 1 hot-path heap
allocation. That is the first listing reallocating 2.4 MB. Your fix goes in
`prepare()` (startup, off the hot path). After `reserve(size + max listings)`
the per-cell median showed max 55.6 µs (54.9–56.1 in 5 runs), 0 allocations, and the same mean (80.1; 79.8–81.7). The
remaining ~55 µs is the **shift**: a middle insert into a 100k array moves
~1.2 MB. Stage 2, `-DSTAGE_LISTINGS`, sends new listings to a small side
table, `RiskTableT<static_vector<…, 100>>`, which needs no heap; the hot path
checks the main table and then the side table, and you merge them off the hot
path. Measured (median of 5, without your reserve): max 9.5 µs (4.7–13.3 µs), at the price of mean 105.0 ns (+~24 ns, about +29%; 103.7–109.7 over 5 runs); p50 stayed 84 ns (one run showed 125).
The cause of that extra mean was not isolated: in this bench every lookup hits the main table (`misses=0`), so the side-table search ran zero times.
In production an order for an intraday listing would also pay a second search when the main table misses.
That is a real trade, so decide it with numbers.
**The memory policy is one template argument.** `ConfigTableT<Alloc>` and
`RiskTableT<AllocOrSeq>` default to Boost's/the standard's allocators. Swapping
in pooled or arena memory, such as the drop-in allocators from
`../advanced_memory`, is a one-line change to the aliases, verified here with the
standard's polymorphic allocator (`labs/bin/capstone_pmr`):
`using RiskTable = RiskTableT<std::pmr::polymorphic_allocator<std::pair<Sym8, RiskLimit>>>;`
Elaborate: The reserve fix left the mean unchanged and cut the max by ~4×. Why
did the mean not move, and what is the remaining max made of?
ElaborateKey:
  - Only one order in 400,000 hit the reallocation, so it barely affects the mean. The max is exactly that one event.
  - The remaining ~55 µs is the O(N) shift of a middle insert into the 100k-entry array (plus OS noise), which reserve cannot remove.
HandsOn: Run `labs/bin/capstone` and then `labs/bin/capstone_staged` (which
already has staging but NOT your reserve). Record max and "hot-path heap
allocations" for both.
HandsOnKey:
  - capstone: max in the hundreds of µs and 1 hot-path allocation (median 232 µs). capstone_staged without reserve: a much smaller max (about 5–13 µs) and 0 allocations, because the main table is never inserted into. Your exact numbers will differ.
Source: labs/capstone.cpp; bench/results/run1..run5/capstone*.txt; bench/results/median/capstone*.txt

---

## Completion cards

### CARD 04-C1  [type: completion]  [topic: hash]  [box: 1]
Q: Complete: `boost::unordered_flat_map` stores elements ______ in the bucket
   array, so references are invalidated on ______; its max load factor ______
   be set by the user; the fix for the rehash spike is ______.
Key:
  - directly; rehash (growth); cannot; reserve(expected max) before the hot path.
Concepts: L4C2
Source: CONCEPT L4C2

### CARD 04-C2  [type: completion]  [topic: eytzinger]  [box: 1]
Q: Complete the Eytzinger descent: children of slot `k` are ______ and ______;
   the branch-free step is `k = ______;`
Key:
  - 2k; 2k+1; `2*k + (b[k] < x)`.
Concepts: L4C3
Source: CONCEPT L4C3

### CARD 04-C3  [type: completion]  [topic: capstone]  [box: 1]
Q: Complete: in the capstone, `reserve` removed the ______ (and the hot-path
   heap allocation), but not the ______, which staging new listings in a ______
   table removed.
Key:
  - reallocation spike; O(N) shift of the middle insert; small side (static_vector-backed) table.
Concepts: L4C5
Source: CONCEPT L4C5

---

## Question cards

### CARD 04-Q1  [type: recall]  [topic: alternatives]  [box: 1]
Q: From memory: name the four alternatives to flat_map from this lesson and the
   one condition under which each wins.
Key:
  - Direct-indexed array: keys are dense small integers. Linear scan over a tiny array: very small N (here only 32-byte keys, N ≤ 16).
  - boost::unordered_flat_map: point lookups only, no order needed (fastest at every N measured; reserve to avoid rehash spikes); unordered_node_map if references must survive rehashing.
  - Eytzinger (or B-tree) layout: large, mostly static sorted sets (≈5× faster than binary search at 16M keys here).
Concepts: L4C1, L4C2, L4C3
Source: CONCEPTS L4C1–L4C3

### CARD 04-Q2  [type: apply]  [topic: alternatives]  [box: 1]
Q: Instrument ids are dense, 0..N-1. Write
   `const Config* find_cfg(const std::vector<Config>& t, uint32_t id)` that returns
   the config or `nullptr` for an out-of-range id, with no search. (Compiled and
   run under ASan.)
Key:
  - `return id < t.size() ? &t[id] : nullptr;`
AcceptIf: check(checks/l4-direct.check.sh)
Concepts: L4C1
Source: CONCEPT L4C1

### CARD 04-Q3  [type: recall]  [topic: notwhen]  [box: 1]
Q: From memory: give four situations in which you should NOT use a flat_map.
Key:
  - Write-heavy hot path, especially with random-position inserts and erases (O(N) shifts).
  - Large N combined with frequent writes.
  - Stable references, pointers or iterators are needed across mutations.
  - Only point lookups are needed, with no order or ranges (a hash table or direct index is faster).
Concepts: L4C4
Source: CONCEPT L4C4

### CARD 04-Q4  [type: apply]  [topic: hash]  [box: 1]
Q: Write the statement(s) to run at startup so that inserting 1,000,000 distinct
   keys into `boost::unordered_flat_map<uint64_t,uint64_t> h` never changes
   `h.bucket_count()`. (Compiled and run.)
Key:
  - `h.reserve(1000000);` — reserve takes an element count. Calling `max_load_factor(…)` does nothing for this container.
AcceptIf: check(checks/l4-rehash.check.sh)
Concepts: L4C2
Source: CONCEPT L4C2

### CARD 04-Q5  [type: apply]  [topic: eytzinger]  [box: 1]
Q: Transfer: a pre-trade check asks "is this account id on the 20M-entry
   restricted list?" per order. The list is published once a day. Choose a layout
   and justify it with mechanism, then name the cost you accept.
Key:
  - A static Eytzinger (or B-tree) layout, or a hash set: built once per day off the hot path. Eytzinger turns ~24 dependent probes (most of them cache misses at this size) into prefetchable, branch-free probes (≈5× faster than binary search at 16M here). A hash set would avoid the log N probes entirely if no order is needed.
  - Cost: rebuild on every update (the layout is static), plus extra code to maintain.
Concepts: L4C3
Source: CONCEPT L4C3

### CARD 04-Q6  [type: apply]  [topic: capstone]  [box: 1]
Q: Capstone. Write the body of `prepare(ConfigTable& cfg, RiskTable& risk)` in
   `labs/capstone.cpp` so the hot loop performs **zero** heap allocations.
   Constants available: `kMaxNewListings` (new listings per session). (Your body is
   compiled into the capstone and run; it must print
   `hot-path heap allocations: 0` and `lookups ok`.)
Hint: the risk table is built tight (capacity == size), and up to kMaxNewListings inserts arrive intraday.
Key:
  - `risk.reserve(risk.size() + kMaxNewListings);` — any reserve with room for all intraday listings. `risk.reserve(risk.size())` or touching only `cfg` still allocates on the hot path.
AcceptIf: check(checks/l4-capstone.check.sh)
Concepts: L4C5
Source: CONCEPT L4C5

### CARD 04-Q7  [type: apply]  [topic: capstone]  [box: 1]
Q: Transfer: your capstone fix made "hot-path allocations: 0" but the max is still
   ~55 µs. A colleague says "reserve more". Explain why that will not help and
   propose two changes that would, with the trade-off of each.
Key:
  - The remaining spike is the O(N) shift of a middle insert (~1.2 MB moved), not allocation. More capacity does not change how many elements move.
  - Fixes: stage new listings in a small side table and merge off the hot path (measured max ~9.5 µs, 5–13 over 5 runs; mean +~24 ns), or rebuild the table off-thread and swap a pointer. Or choose a structure with O(1)/O(log N) insert for this table (hash map, node map), giving up order or cache density.
Concepts: L4C5
Source: CONCEPT L4C5

---

## Integrate

### CARD 04-INT  [type: synthesize]  [topic: all]  [box: 1]
Q: A new hot-path table lands on your desk. Write your decision procedure as
   ordered questions (≤6), each with the structure it leads to.
Key:
  - Are keys dense ints I control (or can I remap them once)? → direct array.
  - Do I only need point lookups? → boost::unordered_flat_map, reserved (unordered_node_map if references must be stable).
  - Do I need order or ranges, with read-mostly use and small/medium N? → flat_map (bulk-built, reserved, -DNDEBUG; small_vector/static_vector if tiny and bounded).
  - Is the set huge and static? → Eytzinger/B-tree layout.
  - Are writes frequent at random positions, or are stable references needed? → not flat_map (node container or hash).
  - Always: measure p50/p99/p99.9/max on the target box, and put the allocator in a template parameter so memory policy is a one-line swap.
Concepts: L4C1, L4C2, L4C3, L4C4, L4C5
Source: CONCEPTS L4C1–L4C5
