# Lesson 01 — "Flat" means one sorted array (and what that costs)

Your order gateway keeps a table of 64 instruments. Somebody swapped
`std::map` for `boost::container::flat_map` "because flat is faster" and the
lookup got faster. Then someone with 64-byte config records did the same swap
in a debug-ish build and it got *slower* than `std::map`. Both people are
right. By the end of these 25 minutes you will know why, and you will have
measured it yourself.

How this works: you guess before you are told, and you answer from memory
before you see the key. That feels harder than reading. The extra effort is
the method working, not a sign you are doing badly. Every number here was
measured on an Apple M1 Max (Apple clang 21, `-std=c++23 -O2`, Boost 1.90,
`steady_clock` tick ≈ 41.7 ns). **Run protocol:** each bench was run 5 times
on a shared machine with other jobs running; raw runs are in
`bench/results/run1..run5/`, the per-cell median is in `bench/results/median/`,
and the spread is in `bench/results/median/SPREAD.txt`. Lookup p50s moved about
4–5% run to run (median cell; 90% of cells within ~13%), single cells up to 42%; tail maxima moved
by factors of 2–10, so tails are taught as mechanisms, not as exact figures.
**Numbers moved 20–50% between two sessions on this same machine depending on background load** (the 1,048,576-entry `std::map` lookup read 623.7 ns in one session and 423.8 ns in the next), **so trust ratios and shapes, and re-run on your own box.** A tuned, isolated x86-64 Linux core will give
different numbers. **Re-run the benches on your own box**; trust mechanisms,
not my numbers.

First thing, right now (≈1 min): `bash labs/build.sh` from the tutorial directory.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 01-P1  [type: pretest]  [topic: lookup]  [box: 1]
Q: Cold guess. 64 entries, `uint64_t` key → `uint64_t` value, random successful
   lookups, `-O2 -DNDEBUG`. Rank these from fastest to slowest per lookup:
   `std::map`, `boost::container::flat_map`, `std::unordered_map`,
   `boost::unordered_flat_map`, and a linear scan over an unsorted array.
Key:
  - Measured here (bench/results/median/lookup_crossover.ndebug.txt, N=64, p50),
    fastest to slowest: boost::unordered_flat_map (about 3 ns), std::unordered_map (about 6),
    flat_map (about 8), then linear scan and std::map in a near-tie (about 22 and 25,
    within run-to-run spread; accept either order for those two). Key idea, not the
    digits: hash < sorted array < tree. If you put flat_map first, you are in
    good company. boost::unordered_flat_map won point lookups at every N
    measured. CONCEPT L1C3 shows where flat_map does earn its place.
Concepts: L1C3
Source: bench/lookup_crossover.cpp, bench/results/median/lookup_crossover.ndebug.txt

### CARD 01-P2  [type: pretest]  [topic: write]  [box: 1]
Q: Cold guess. A `flat_map<uint64_t, uint64_t>` holds 10,000 entries and has
   spare capacity. You insert a key smaller than every existing key. Roughly how
   long does that one insert take: ~40 ns, ~200 ns, ~5 µs or ~1 ms?
Key:
  - About 5 µs (p50 5,459 ns measured). Every one of the 10,000 pairs (16 bytes
    each) shifts one slot right. A `std::map` insert+erase pair at the same
    size took ~208 ns. CONCEPT L1C2.
Concepts: L1C2
Source: bench/insert_tail.cpp, bench/results/median/insert_tail.ndebug.txt

### CARD 01-P3  [type: pretest]  [topic: ndebug]  [box: 1]
Q: Cold guess. Same source file, same `-O2`. Build A has no extra flags. Build B
   adds `-DNDEBUG`. With 4,096 entries and 64-byte values, how much faster is
   `flat_map::find` in build B: about the same, 1.2×, or about 4×?
Key:
  - About 4× (80.2 → 22.0 ns per lookup in labs/l1_ndebug.cpp). Boost.Container
    runs `BOOST_ASSERT` checks inside its iterators, and those are plain
    `assert()` calls unless `NDEBUG` is defined. CONCEPT L1C4.
Concepts: L1C4
Source: labs/l1_ndebug.cpp; /opt/homebrew/include/boost/assert.hpp

---

## Concept cards

### CONCEPT L1C1  A flat_map is one sorted, contiguous sequence of pairs
`boost::container::flat_map<K, T>` stores a single vector-like sequence of
`std::pair<K, T>`, kept sorted by key. There is no tree and there are no nodes.
`find` is a binary search over that array, and iteration walks plain memory.
Because the storage is one array you also get random access: `nth(i)` and
`index_of(it)` run in O(1). Those are Boost extensions; `std::map` has no
equivalent. The Boost docs call these "ordered, vector-like container based,
associative containers" and list the trade: faster lookup and much faster
iteration, but slower insertion and erasure than node-based containers.
Figure: [10|1.0][20|2.0][30|3.0][40|4.0] laid end to end, 16 bytes per slot, versus std::map with four separately allocated nodes linked by pointers.
Elaborate: In your own words: why does one sorted array make iteration and
`nth()` cheap, and what has to happen to that array when a new smallest key
arrives?
ElaborateKey:
  - Elements sit at fixed offsets in one block, so the next element is the next address and element i is at base + i·sizeof(value_type).
  - A new smallest key must go at index 0, so every existing element moves one slot right (and the block may be reallocated).
HandsOn: Before running, predict (a) the byte gap between consecutive keys,
(b) the order keys print in, (c) whether key 10's address changes after
`emplace(5, …)`. Then run `labs/bin/l1_layout`.
HandsOnKey:
  - Gap is 16 bytes, which is sizeof(std::pair<int,double>). Keys print in sorted order 10, 20, 30, 40 although they were inserted as 40, 10, 30, 20.
  - After emplace(5) key 10 sits at offset +16 and its address CHANGED. The std::map nodes show scattered addresses.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/non_standard_containers.html#container.non_standard_containers.flat_xxx ; labs/l1_layout.cpp (run)

### CONCEPT L1C2  Writes are O(N) moves: cost = elements after the slot × sizeof(value_type)
An insert does a binary search for the slot and then shifts every element
after it one position right. An erase shifts them one position left. If
`size() == capacity()`, the insert first reallocates and moves the whole
array. So **position decides the price**: the bytes moved are (elements after
the slot) × `sizeof(value_type)`. Boost's reference gives the same complexity:
"logarithmic search time plus linear insertion to the elements with bigger
keys". Measured at 10,000 `uint64_t→uint64_t` entries (p50, one op per sample):
insert at the front 5,459 ns, at a random position 2,584 ns, at the back 42 ns.
Erase at the front took 2,708 ns and at the back 42 ns. A `std::map`
insert+erase pair took 208 ns, because it allocates one node and moves nothing.
Figure: insert at index 0 → memmove of [0, N) to [1, N+1); insert at the back → no move.
Elaborate: Why is "append in ascending key order" the cheapest way to fill a
flat_map one element at a time, and why does a random insert cost about half of a
front insert?
ElaborateKey:
  - A key larger than all others lands at the end, so zero elements move (amortised constant, plus occasional reallocation).
  - A random slot has on average N/2 elements after it, so it moves half as many bytes as a front insert, which moves all N.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/doxygen/boost_container_header_reference/classboost_1_1container_1_1flat__map.html ; bench/results/median/insert_tail.ndebug.txt

### CONCEPT L1C3  Lookup mechanism and the crossover you measured
A `std::map` lookup makes about log2(N) dependent pointer chases. Each hop
loads a node somewhere on the heap, and the next address is unknown until
that load lands. A flat_map does the same number of comparisons, but inside
one array: the first few probe points are reused by every lookup and stay in
cache, and the array has no pointers or allocator headers. A hash table
computes one bucket and looks there. Measured p50 ns per successful lookup,
8-byte key and value, `-DNDEBUG`:

| N | flat_map | std::flat_map | std::map | std::unordered_map | boost::unordered_flat_map | linear scan |
|---|---|---|---|---|---|---|
| 64 | 8.5 | 8.5 | 24.8 | 6.5 | 2.6 | 22.1 |
| 4,096 | 19.5 | 19.5 | 61.2 | 7.8 | 3.2 | – |
| 1,048,576 | 162.8 | 128.2 | 423.8 | 24.7 | 11.7 | – |

What the table says: with 8-byte keys, flat_map beat `std::map` at every N
measured (8 to 1M), by 2.6–3.1× at the three sizes shown (24.8/8.5, 61.2/19.5, 423.8/162.8). With 32-byte keys
they tied at N=8. `boost::unordered_flat_map` beat both at every N.
`std::unordered_map` was faster than flat_map at every size except N=32
with 8-byte keys (both value sizes), where flat_map was ahead by 0.7 ns
(7.8 vs 8.5), less than one timer tick per lookup, so treat that as a tie.
The naive linear scan beat binary search only with 32-byte keys and
N ≤ 16 (15.0 vs 18.9 ns at N=8). Choose flat_map for what a hash cannot give:
**sorted order, range queries (`lower_bound`), cheap ordered iteration,
compactness**, and a lookup that is ~3× better than `std::map` (2.6–3.1× here).
Figure: std::map: root→node→node→… (each arrow a cache miss at large N); flat_map: probes at N/2, N/4, … within one array; hash: one bucket.
Elaborate: Explain why the gap between std::map and flat_map grows as N grows
from 4,096 to 1M, while both remain worse than a hash table.
ElaborateKey:
  - At large N the tree's nodes are scattered across many cache lines and pages, so each of ~20 hops is a likely cache miss with a dependent address. The flat array keeps its top probe levels hot and has no per-node overhead.
  - Both still do ~log2(N) dependent probes, while a hash lookup does O(1) expected probes, so a good hash table wins point lookups.
HandsOn: Run `bench/run_all.sh` (about 15 minutes: 5 runs, normal priority, quiet machine)
or just `clang++ -std=c++23 -O2 -DNDEBUG -I/opt/homebrew/include bench/lookup_crossover.cpp -o /tmp/lc && /tmp/lc quick`.
Find the smallest N at which flat_map's p50 is at least twice std::map's on YOUR machine.
HandsOnKey:
  - On the authoring machine that happens by N=16 (7.8 vs 18.2 ns; at N=64 it is 8.5 vs 24.8). Your N may differ. What should carry over is the shape: the gap widens as N grows, and hash tables lead throughout.
Source: bench/lookup_crossover.cpp; bench/results/median/lookup_crossover.ndebug.txt

### CONCEPT L1C4  Boost.Container ships with its asserts ON unless you say otherwise
Boost's `vector` iterator (which flat_map uses inside) runs `BOOST_ASSERT(!!m_ptr)` in
`operator*`, `++` and `+=` (boost/container/vector.hpp). `BOOST_ASSERT` expands
to plain `assert()` unless `NDEBUG` or `BOOST_DISABLE_ASSERTS` is defined
(boost/assert.hpp, lines 36–72). The plain `-O2` build therefore keeps
the checks inside the binary-search loop. With 64-byte values (asserts ON),
flat_map lost to `std::map` at every N up to 65,536 (82.7 vs 65.8 ns at
N=4,096; 145.8 vs 127.0 at N=65,536) and only won at N=1,048,576 (354.8 vs 583.3). With `-DNDEBUG` it was 9.1 vs 24.8 ns at N=64. `-DBOOST_DISABLE_ASSERTS`
alone gave the same 22.0 ns as `-DNDEBUG` in labs/l1_ndebug.cpp (5 runs of each
build, all 22.0; asserts ON: 80.1–80.4; bench/results/probes/probes-and-l1-disable-asserts.txt). The lesson
for benchmarking: before you believe a layout story, check what you compiled.
Elaborate: Why is a benchmark of a header-only library without `-DNDEBUG`
not a fair test of its data structure?
ElaborateKey:
  - Header-only code is compiled with YOUR flags, so its debug checks run in your build and get measured along with the data structure.
  - Without NDEBUG (or BOOST_DISABLE_ASSERTS), BOOST_ASSERT checks execute inside the hot search loop, so the timing includes checks that a production build would remove.
HandsOn: Run `labs/bin/l1_assert_on` then `labs/bin/l1_assert_off` and write
down both ns/lookup values and their ratio.
HandsOnKey:
  - Authoring run: 80.2 vs 22.0 ns per lookup (≈3.6×). Your ratio may differ, but the assert-on build should be clearly slower.
Source: /opt/homebrew/include/boost/assert.hpp (lines 36–72); /opt/homebrew/include/boost/container/vector.hpp (line 158 onward); labs/l1_ndebug.cpp (run)

---

## Completion cards (fill the gap)

### CARD 01-C1  [type: completion]  [topic: layout]  [box: 1]
Q: Complete: a `boost::container::flat_map<K,T>` stores one ______ sequence of
   ______, kept ______ by key, and `find` does a ______ over it.
Key:
  - contiguous (vector-like); std::pair<K,T>; sorted; binary search.
Concepts: L1C1
Source: CONCEPT L1C1

### CARD 01-C2  [type: completion]  [topic: write]  [box: 1]
Q: Complete the cost formula for one flat_map insert with spare capacity:
   bytes moved = (number of elements ______ the insertion slot) × ______.
Key:
  - after (with larger keys than) the slot; sizeof(value_type), i.e. sizeof(std::pair<K,T>).
Concepts: L1C2
Source: CONCEPT L1C2

### CARD 01-C3  [type: completion]  [topic: ndebug]  [box: 1]
Q: Complete: `BOOST_ASSERT` expands to ______ unless the macro ______ or
   ______ is defined.
Key:
  - assert(expr); NDEBUG; BOOST_DISABLE_ASSERTS.
Concepts: L1C4
Source: CONCEPT L1C4

---

## Question cards (free recall + transfer)

### CARD 01-Q1  [type: recall]  [topic: layout]  [box: 1]
Q: From memory: describe the memory layout of `boost::container::flat_map`, and
   name two operations it makes cheap that `std::map` cannot do as cheaply.
Hint: one block of pairs; think "random access".
Key:
  - One contiguous, sorted sequence of std::pair<K,T>; no nodes or per-element allocations.
  - Cheap: ordered iteration over contiguous memory, O(1) nth()/index_of() random access (lookup by binary search is also faster than std::map's tree walk).
Concepts: L1C1
Source: CONCEPT L1C1

### CARD 01-Q2  [type: recall]  [topic: lookup]  [box: 1]
Q: From memory: why is a flat_map lookup faster than a std::map lookup with the
   same number of comparisons, and why does neither beat a hash table on point
   lookups in the bench?
Key:
  - std::map does ~log2 N dependent pointer chases to scattered heap nodes (cache misses at large N). flat_map searches one array whose top probe levels stay cache-hot, with no node overhead.
  - A hash table does O(1) expected probes rather than ~log2 N; boost::unordered_flat_map won every point-lookup row measured.
Concepts: L1C3
Source: CONCEPT L1C3

### CARD 01-Q3  [type: apply]  [topic: write]  [box: 1]
Q: `flat_map<uint64_t, uint64_t>` with 10,000 entries and spare capacity. You
   insert a key smaller than all of them. How many bytes does the insert move?
   Give a number. (Your answer is checked by a script.)
Hint: sizeof(std::pair<uint64_t,uint64_t>) × how many elements?
Key:
  - 160,000 bytes: all 10,000 elements × 16 bytes shift one slot right.
AcceptIf: check(checks/l1-bytes.check.sh)
Concepts: L1C2
Source: CONCEPT L1C2

### CARD 01-Q4  [type: apply]  [topic: write]  [box: 1]
Q: Different domain: a market-data handler keeps a `flat_map<Price, Level>` order
   book side (bids sorted ascending by price, best bid at the BACK). Most updates
   touch the best few levels. Is a flat_map a reasonable fit for this access
   pattern, and what would change if the best bid were stored at the FRONT?
Key:
  - Yes, plausibly: updates near the back move only the few elements after the slot, so inserts and erases near the best price are cheap.
  - With the best price at the front, every new best level shifts the whole side (O(N) bytes per update), which hurts the write path and the tail.
Concepts: L1C2
Source: CONCEPT L1C2

### CARD 01-Q5  [type: apply]  [topic: ndebug]  [box: 1]
Q: Your teammate's flat_map benchmark used `clang++ -std=c++23 -O2`. Give the
   extra flag(s) you add so that Boost.Container's internal assertions are
   compiled out. (Your flags are passed to clang++ and tested.)
Hint: one macro name from the C standard library.
Key:
  - -DNDEBUG (or -DBOOST_DISABLE_ASSERTS, which turns off BOOST_ASSERT only).
AcceptIf: check(checks/l1-ndebug.check.sh)
Concepts: L1C4
Source: CONCEPT L1C4

### CARD 01-Q6  [type: apply]  [topic: lookup]  [box: 1]
Q: Transfer: a risk service needs "all limits for symbols between `AAPL` and `AMZN`"
   every 100 ms, plus point lookups per order. Using the measured table, argue for
   flat_map over `boost::unordered_flat_map`, and say what you give up.
Key:
  - flat_map keeps keys sorted, so the range query is lower_bound + a contiguous walk. A hash table has no order and would need a full scan or a second index.
  - You give up point-lookup speed: the hash table was ~1.5–14× faster per lookup in the bench (e.g. 3.2 vs 19.5 ns, 6×, at N=4,096; 11.7 vs 162.8 ns, 14×, at N=1M).
Concepts: L1C3
Source: CONCEPT L1C3

---

## Integrate (day close)

### CARD 01-INT  [type: synthesize]  [topic: all]  [box: 1]
Q: In three sentences: what one property of flat_map makes lookups and
   iteration cheap, how does that same property make writes expensive, and what
   must you check about the build before trusting any flat_map benchmark?
Key:
  - Property: one contiguous, sorted array of pairs, which gives cache-friendly binary search and linear iteration.
  - Same property: inserts and erases shift every element after the slot, and growth reallocates and moves everything (O(N) bytes).
  - Build check: compile with -DNDEBUG (or BOOST_DISABLE_ASSERTS), otherwise Boost's BOOST_ASSERT checks run inside the search loop.
Concepts: L1C1, L1C2, L1C3, L1C4
Source: CONCEPTS L1C1–L1C4
