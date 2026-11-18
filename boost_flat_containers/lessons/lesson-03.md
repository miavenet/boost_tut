# Lesson 03 — The write path: bulk builds, growth spikes, and zero-heap tables

The mean lied. Appending 1,048,576 keys to a flat_map averaged **26 ns per
insert**, which sounds great. The slowest single insert took **about 1.0 ms**
(0.95 to 1.09 ms over 5 runs), which
on a trading host is a missed market. Same container, same run (bench/insert_tail.cpp,
`-DNDEBUG`). This lesson is about the write path: how to build a table
without a quadratic blow-up, where the tail spikes come from, and how to
remove them. One of the fixes needs no heap at all.

Start now (≈1 min): `labs/bin/l3_sequences` is built by `bash labs/build.sh`.
Do not run it yet; there is a prediction card first.

## Pretest — commit your guesses FIRST

### CARD 03-P1  [type: pretest]  [topic: bulk]  [box: 1]
Q: Cold guess. Building a flat_map of 100,000 random keys by calling `insert` once
   per key took X ms. Sorting the keys first and using the `ordered_unique_range`
   constructor took Y ms. Guess X and Y.
Key:
  - X ≈ 865 ms, Y ≈ 6.6 ms (bench/results/median/bulk_build.ndebug.txt; one-by-one varied only 863–867 ms over 5 runs). One-by-one is quadratic: going from 10k to 100k keys (10×) made it ~127× slower (6.8 → 865 ms). CONCEPT L3C1.
Concepts: L3C1
Source: bench/bulk_build.cpp; bench/results/median/bulk_build.ndebug.txt

### CARD 03-P2  [type: pretest]  [topic: growth]  [box: 1]
Q: Cold guess. Appending 1,048,576 ascending keys to an empty Boost flat_map
   (no reserve): how many times does it reallocate? About 20, 31, or 1,000?
Key:
  - 31 (measured). Boost's vector grows by 60% by default (growth_factor_60), and 1.6^30 ≈ 1.3M. The last of those 31 moves a ~1M-entry array: that is the ~1 ms max. CONCEPT L3C2.
Concepts: L3C2
Source: bench/results/median/insert_tail.ndebug.txt; /opt/homebrew/include/boost/container/vector.hpp (line 768)

### CARD 03-P3  [type: pretest]  [topic: sequence]  [box: 1]
Q: Cold guess. A flat_map whose underlying sequence is
   `static_vector<pair<int,int>, 16>` holds 16 entries. What happens on the 17th
   insert, and which `catch` clause sees it: `std::bad_alloc` or something else?
Key:
  - It throws, and NOT std::bad_alloc: it is `boost::container::bad_alloc`, which derives from `boost::container::exception` → `std::exception`. `catch (const std::bad_alloc&)` misses it. CONCEPT L3C4.
Concepts: L3C4
Source: labs/l3_sequences.cpp (run); /opt/homebrew/include/boost/container/throw_exception.hpp (lines 25–80)

---

## Concept cards

### CONCEPT L3C1  Bulk build: never insert one by one
Each insert into a sorted array shifts on average half of it, so N random
inserts cost O(N²) element moves. Build in bulk instead. (1) Put the pairs in a vector,
sort them, and construct with `flat_map(boost::container::ordered_unique_range, first, last)`.
That constructor is linear and *trusts* you that the input is sorted and
unique. (2) Fill a `FM::sequence_type`, sort it, and
`adopt_sequence(ordered_unique_range, std::move(seq))`, which is O(1) and copies
nothing. (3) Use the plain range constructor `FM(first, last)`, which sorts
for you. Measured, uint64→uint64, p50 (`-DNDEBUG`):

| N | one-by-one insert | sort + ordered_unique_range ctor | sort + adopt_sequence | range ctor (Boost sorts) |
|---|---|---|---|---|
| 10,000 | 6.80 ms | 0.48 ms | 0.47 ms | 0.42 ms |
| 100,000 | 865.1 ms | 6.57 ms | 6.52 ms | 5.63 ms |

If you lie to `ordered_unique_range` with unsorted data you get a broken
table (exactly what the check for 03-Q2 rejects).
Elaborate: Why is one-by-one insertion quadratic for a flat_map but only
O(N log N) for std::map?
ElaborateKey:
  - flat_map: each insert moves ~N/2 elements on average, so the total is ~N²/4 moves.
  - std::map: each insert allocates one node and relinks O(log N) pointers. Nothing else moves.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/bulk_build.ndebug.txt

### CONCEPT L3C2  Growth is a latency spike; reserve removes it (the mean hides it)
When `size() == capacity()`, the next insert allocates a block 60% larger and
moves every element across. Boost's default is growth_factor_60
(boost/container/vector.hpp line 768, options.hpp). Most inserts are cheap, so
the mean barely moves, but each reallocation is one very slow insert. Measured
per single append, 1,048,576 entries, ns (Apple M1 Max, Apple clang 21, `-std=c++23 -O2 -DNDEBUG`, 5 runs, per-cell median; max cells vary a lot: no-reserve 0.95–1.09 ms, reserve(N) 4.5–12.9 µs, so quote the mechanism, not the digit):

| case | p50 | p99 | p99.9 | max | mean |
|---|---|---|---|---|---|
| no reserve (31 reallocations) | 41 | 42 | 208 | 998,875 | 25.9 |
| `reserve(N)` first | 41 | 42 | 42 | 6,125 | 22.5 |

(p50 = 41 is one timer tick, so most inserts are below the clock's resolution.)
The max that remains with reserve, about 6 µs (4.5–12.9 µs over 5 runs), is the OS and the machine, not
the container. Here the Mac was shared with other jobs. On a tuned,
isolated Linux core you would expect less noise. Rule: **size the table at
startup for the session's maximum**, off the hot path.
Elaborate: Why did the mean change by only ~3.4 ns while the max changed by ~1 ms?
ElaborateKey:
  - Only 31 of ~1M inserts reallocated. Their cost (one ~1 ms copy, many smaller ones) is spread over 1M samples in the mean, but the max reports the single worst insert.
Source: /opt/homebrew/include/boost/container/vector.hpp (line 768); bench/insert_tail.cpp; bench/results/median/insert_tail.ndebug.txt

### CONCEPT L3C3  Write position and batching
The bytes an insert moves depend on where it lands (lesson 1, CONCEPT L1C2). Three practical moves:
**update in place**: `it->second = x` touches one element and moves nothing,
so never erase+reinsert just to change a value. **Batch**: collect a burst of
new keys, sort them, and merge once with
`m.insert(boost::container::ordered_unique_range, first, last)` (Boost: "Complexity: Linear"). Measured,
1,000 new keys into a 100,000-entry table: one by one 17.5 ms, sorted batch
0.66 ms (copy of the table, 0.036 ms, included in both). **Rebuild off the hot
path**: build the next table on another thread or during a quiet period, then
swap a pointer on the hot path, so readers never see the shift.
Elaborate: Why is a sorted batch merge so much cheaper than 1,000 single inserts?
ElaborateKey:
  - Each single insert shifts the tail of a 100k array (~50k elements on average), so k inserts cost about k·N/2 moves. The sorted-range insert is documented as "Linear": each existing element moves about once, plus the k log k sort of the batch.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/bulk_build.ndebug.txt

### CONCEPT L3C4  Choose the sequence: small_vector and static_vector for zero heap
The fourth template parameter of `boost::container::flat_map` can be a
sequence container instead of an allocator:
`flat_map<K, T, std::less<K>, boost::container::small_vector<std::pair<K,T>, 16>>`.
**small_vector<P, N>** keeps up to N elements inside the object. Beyond N it
moves to the heap and keeps working ("capacity can grow beyond the initial
preallocated capacity"). **static_vector<P, N>** stores elements "within the
object itself" and never allocates. Going past N throws when the default
`throw_on_overflow<true>` option is set. The type thrown is
`boost::container::bad_alloc`, not `std::bad_alloc`, unless you build with
`BOOST_CONTAINER_USE_STD_EXCEPTIONS`. With `throw_on_overflow<false>`,
overflowing is undefined behaviour. Measured in labs/l3_sequences: default
(heap) table: 8 allocations for 16 inserts, sizeof 24. small_vector<16>: 0
allocations for 16, 1 at the 17th, sizeof 152. static_vector<16>: 0
allocations, and the 17th insert throws while size stays 16, sizeof 136. The
inline storage counts toward sizeof, so do not nest big ones carelessly.
Elaborate: For a per-connection table of "orders I sent in the last 100 µs"
(usually ≤ 8, never more than 32 by protocol), which sequence would you choose
and why?
ElaborateKey:
  - static_vector<…, 32> if 32 really is a hard protocol bound, because it never touches the heap and overflow is a loud error. Otherwise small_vector<…, 8>, which avoids the heap in the common case and degrades to a heap allocation instead of failing.
HandsOn: Predict the allocation counts for Heap/Small/Static after 16 and 17
inserts, then run `labs/bin/l3_sequences`.
HandsOnKey:
  - `Heap sizeof=24 allocs after 16: 8 after 17th: 8`; `Small sizeof=152 allocs after 16: 0 after 17th: 1`; `Static sizeof=136 allocs after 16: 0 17th insert threw: "boost::container::bad_alloc thrown" (size still 16)`.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.small_vector ; https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1static__vector.html ; /opt/homebrew/include/boost/container/throw_exception.hpp; labs/l3_sequences.cpp (run)

---

## Completion cards

### CARD 03-C1  [type: completion]  [topic: bulk]  [box: 1]
Q: Complete the O(1) hand-off: fill and sort `FM::sequence_type seq;` then call
   `m.______(boost::container::______, std::move(seq));`
Key:
  - adopt_sequence; ordered_unique_range.
Concepts: L3C1
Source: CONCEPT L3C1

### CARD 03-C2  [type: completion]  [topic: growth]  [box: 1]
Q: Complete: a reallocation spike happens when ______ equals ______; Boost's
   default vector grows capacity by ______ %; the fix is ______ at startup.
Key:
  - size(); capacity(); 60 (growth_factor_60); reserve(expected maximum).
Concepts: L3C2
Source: CONCEPT L3C2

### CARD 03-C3  [type: completion]  [topic: sequence]  [box: 1]
Q: Complete: past its inline capacity, `small_vector` ______, while
   `static_vector` (default options) ______.
Key:
  - moves to heap storage and keeps working (one allocation); throws boost::container::bad_alloc and never allocates.
Concepts: L3C4
Source: CONCEPT L3C4

---

## Question cards

### CARD 03-Q1  [type: recall]  [topic: bulk]  [box: 1]
Q: From memory: name three ways to build a Boost flat_map from N unsorted pairs
   without per-element insert, and say what `ordered_unique_range` promises.
Key:
  - Sort + `flat_map(ordered_unique_range, first, last)`; sort a `sequence_type` + `adopt_sequence(ordered_unique_range, std::move(seq))`; plain range constructor `flat_map(first, last)` (sorts for you). Also `adopt_sequence(std::move(seq))`, which sorts and removes duplicates.
  - ordered_unique_range promises the input is already sorted by the comparator and has unique keys. The library does not check it.
Concepts: L3C1
Source: CONCEPT L3C1

### CARD 03-Q2  [type: apply]  [topic: bulk]  [box: 1]
Q: Write `FM build(std::vector<std::pair<uint64_t,uint64_t>> v)` (FM is
   `boost::container::flat_map<uint64_t,uint64_t>`, `bc` is `boost::container`).
   `v` is in random order with unique keys. It must build 100,000 pairs at least 4x
   faster than one-by-one insert timed in the same run. (Compiled, run, and timed.)
Key:
  - e.g. `std::sort(v.begin(), v.end()); return FM(bc::ordered_unique_range, v.begin(), v.end());` (or adopt_sequence, or `FM(v.begin(), v.end())`).
  - Passing unsorted data with ordered_unique_range, or inserting one by one (~0.9 s here, no faster than itself), fails.
AcceptIf: check(checks/l3-bulk.check.sh)
Concepts: L3C1
Source: CONCEPT L3C1

### CARD 03-Q3  [type: recall]  [topic: growth]  [box: 1]
Q: From memory: explain how a flat_map can have a ~26 ns mean insert time and a
   ~1 ms max in the same run, and give the fix.
Key:
  - Growth reallocates and moves the whole array when size()==capacity(). That is rare (31 times for 1M appends), so the mean hides it, but the worst insert copies ~MBs.
  - Fix: reserve() the session's maximum size at startup (or build in bulk at the final size).
Concepts: L3C2
Source: CONCEPT L3C2

### CARD 03-Q4  [type: apply]  [topic: batch]  [box: 1]
Q: Transfer: an end-of-day job applies 50,000 corporate-action updates. 40,000 of
   them change values of existing symbols and 10,000 add new symbols to a 2M-entry
   flat_map. Sketch the fastest correct approach.
Key:
  - Update existing entries in place (find + assign it->second; nothing moves).
  - Collect the 10,000 new pairs, sort them, and merge once with `insert(ordered_unique_range, first, last)` (or rebuild the sequence and adopt it), never 10,000 single inserts into a 2M array.
Concepts: L3C3
Source: CONCEPT L3C3

### CARD 03-Q5  [type: apply]  [topic: sequence]  [box: 1]
Q: Name a type `T` such that `catch (const T&)` catches the failure of the 17th
   insert into a flat_map backed by `static_vector<std::pair<int,int>, 16>`.
   (Compiled and run against the real throw.)
Hint: it is not in namespace std::... or is it a base?
Key:
  - `boost::container::bad_alloc` (or its bases `boost::container::exception` / `std::exception`). `std::bad_alloc` does NOT catch it with default Boost settings.
AcceptIf: check(checks/l3-overflow.check.sh)
Concepts: L3C4
Source: CONCEPT L3C4

### CARD 03-Q6  [type: apply]  [topic: sequence]  [box: 1]
Q: Transfer: a feed handler creates one small "pending replace" map per order
   (usually 0–2 entries) and creates/destroys ~1M of them per second. Which
   flat_map sequence do you pick, and what is the trade-off you accept?
Key:
  - small_vector with a small inline N (e.g. 2–4). The common case makes zero heap allocations, and rare bigger cases still work.
  - Trade-off: a larger sizeof per map (inline storage), and the first overflow costs a heap allocation (a latency blip).
Concepts: L3C4
Source: CONCEPT L3C4

### CARD 03-Q7  [type: recall]  [topic: sequence]  [box: 1]
Q: From memory: how do you make a Boost flat_map use `small_vector` or
   `static_vector` storage, and what does each do when it is full?
Key:
  - Pass the sequence as the 4th template argument: `flat_map<K, T, std::less<K>, small_vector<std::pair<K,T>, N>>` (or static_vector<…, N>).
  - small_vector: past N it allocates on the heap and keeps working. static_vector: never allocates; past N it throws boost::container::bad_alloc (default throw_on_overflow<true>; UB if overflow checking is turned off).
Concepts: L3C4
Source: CONCEPT L3C4

---

## Integrate

### CARD 03-INT  [type: synthesize]  [topic: all]  [box: 1]
Q: You own a flat_map on a hot path. List the four write-path rules from this
   lesson in priority order and the failure each prevents.
Key:
  - Build in bulk (sorted + ordered_unique_range / adopt_sequence), which prevents O(N²) startup.
  - reserve the session maximum at startup, which prevents reallocation spikes (1 ms max vs 26 ns mean).
  - Update in place, append or batch-merge, and avoid front/middle inserts on the hot path, which prevents O(N) shifts per event.
  - Use small_vector/static_vector when the size is small and bounded, which prevents heap allocation entirely (catch boost::container::bad_alloc for static_vector overflow).
Concepts: L3C1, L3C2, L3C3, L3C4
Source: CONCEPTS L3C1–L3C4
