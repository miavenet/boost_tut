# Lesson 03 — Bytes, stability and the tail

**Time: about 30 min. Environment for every number in this lesson:** Apple M1 Max
(128-byte cache line, 16 KiB pages), Apple clang 21, `clang++ -std=c++23 -O2`,
Boost 1.90, `steady_clock` with a 41.67 ns tick (values 41/42/84/125 are
tick multiples). Byte sizes are deterministic. Every timing was run 5 times
on a machine shared with other jobs and is shown as the **median of 5 runs,
with min..max in brackets where it matters** (raw runs: `bench/results/`).
Spreads are large for maxima and for the modify benchmark, small for the
lookup and scatter ratios: learn the ratios and mechanisms. Production HFT
runs x86-64 Linux (64-byte line, 4K/2M/1G pages) on isolated cores: the
mechanisms transfer, the numbers do not.

An insert that "costs about 50 nanoseconds" also cost **about 8 milliseconds**,
once, out of a million (5 runs: 8.2 to 8.7 ms). At that moment the market
moved and you were not there. The mean never told you. In this lesson you will make that spike happen, then make
it go away.

**Do this now (1 min):** `./build/l3_node_size`. Before reading the output,
answer the pretest.

## Pretest — commit your guesses FIRST

### CARD M3-P1  [type: pretest]  [topic: tail]  [box: 1]
Q: Cold guess. One million single inserts into a hashed_unique index with no
   `reserve()`. Mean latency is about 50 ns. What is the **max** latency of a
   single insert: about 100 ns, about 10 µs, or about 10 ms?
Key:
  - About 10 ms (milliseconds, not µs). Measured max was 8.2 to 8.7 ms over 5
    runs (mean about 52 ns), and 6 inserts took over 100 µs in every run
    (`build/bench_rehash`). Those few inserts each triggered a
    rehash of everything already stored. CONCEPT M3C3.
Concepts: M3C3
Source: bench/bench_rehash.cpp (run 5 times this session, N=1,000,000; bench/results/summary.txt)

### CARD M3-P2  [type: pretest]  [topic: stability]  [box: 1]
Q: Cold guess. You keep an iterator from a multi_index **hashed** index, then
   insert 100,000 more elements (the bucket count grows from 53 to 196613).
   Is the iterator still valid? Is a `std::unordered_map` iterator valid after
   a rehash?
Key:
  - Multi_index hashed: yes. Its `rehash` postcondition says "Validity of
    iterators and references to the elements contained is preserved."
  - std::unordered_map: iterators are invalidated by a rehash, though
    references and pointers to elements survive. CONCEPT M3C2.
Concepts: M3C2
Source: https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html ; https://en.cppreference.com/w/cpp/container/unordered_map.html (Iterator invalidation) ; labs/l5_stability.cpp (run this session)

---

## Concept cards

### CONCEPT M3C1  Node cost per index type: measured, not assumed
Every element lives in one node: the value plus one header per index. A spy
allocator (the container's last template parameter) printed these sizes for a
40-byte `Order`, with 8-byte pointers:

| index mix | node bytes | header bytes | live bytes/elem incl. buckets |
|---|---|---|---|
| ordered_unique | 64 | 24 | 64.0 |
| hashed_unique | 56 | 16 | 71.7 |
| sequenced | 56 | 16 | 56.0 |
| random_access | 48 | 8 (+ 8 B/elem pointer array) | 56.5 |
| ranked_unique | 72 | 32 | 72.0 |
| hashed + ordered | 80 | 40 | 95.7 |
| hashed + ordered + hashed | 96 | 56 | 127.5 |
| std::map<id,Order> | 80 | — | 80.0 |
| hand-rolled 3 containers | 3 nodes | — | 176.5 |

The ordered header is 3 words, not 4. The Boost docs say ordered indices save
"one system word per ordered index", which matches a red-black colour bit
packed into a pointer. `ranked` adds a size word so it can answer "the k-th
element" and "rank of x" in O(log n). Headers just add up: the cost of an
index is its header plus its work, and nothing else.
HandsOn: Run `./build/l3_node_size` and find the `hashed + ordered` line.
Compute 40 + 16 + 24 yourself.
HandsOnKey:
  - `hashed + ordered node=80 B`, and 40 + 16 + 24 = 80. Live bytes per
    element (95.7) is higher because the hashed bucket array is extra.
Source: labs/l3_node_size.cpp (run this session, N=100000) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/performance.html ("Nodes ... hold the value of the element plus N headers"; "one system word per ordered index") ; /opt/homebrew/include/boost/multi_index/detail/ord_index_node.hpp (`ordered_index_node_compressed_base`: colour stored in the low bit of the parent pointer) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnk_indices.html (nth, rank: O(log(n)))

### CONCEPT M3C2  Stability: nodes never move, and what random_access gives up
Because elements sit in nodes, **pointers, references and iterators stay
valid in every index type until that element is erased**. That includes a
hashed index growing its buckets and a random_access index growing its
pointer array. `l5_stability` shows a pointer that survives buckets going
53 → 196613 and capacity 15 → 106710. `std::vector` by contrast moves its
elements on growth.
`random_access` looks like a vector, but it is an array of **pointers to
nodes**. You get `operator[]` and stability. You give up **contiguity**: 0 of
999 neighbours were adjacent in memory, and there is no `data()`. You also
pay O(distance-to-end) for inserts in the middle: 50,000 `push_front` took
about 867 ms (5 runs: 865 to 871) against about 1 ms (0.9 to 1.0) for 50,000 `push_back`, because every front insert
shifts the whole pointer array and updates each node's back-pointer.
HandsOn: Run `./build/l5_stability`.
HandsOnKey:
  - "pointer to element still valid? yes", "hashed iterator still valid after
    rehash? yes", "std::vector element moved on growth? yes".
  - "adjacent elements contiguous in memory: 0 of 999".
  - push_front is hundreds of times slower than push_back.
Source: labs/l5_stability.cpp (run this session) ; /opt/homebrew/include/boost/multi_index/detail/rnd_index_node.hpp (each node keeps an `up()` pointer to its array slot) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/rnd_indices.html ("Random access indices do not provide memory contiguity"; iterator validity "regardless of the capacity status") ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html (rehash postcondition)

### CONCEPT M3C3  The rehash spike, and `reserve()` on EVERY hashed index
A hashed index keeps `size/buckets ≤ max_load_factor` (1.0 by default). When
an insert crosses that limit, **that one insert** rehashes every element
already stored: O(n) work on a single message. Mean about 52 ns, max about 8 ms.
`reserve(n)` sizes the buckets up front. Careful: `container.reserve(n)`
forwards to **index 0 only**. A second hashed index (by client, say) still
rehashes unless you call `s.get<2>().reserve(n)` too (the check for this card
proves it). Measured, N = 1,000,000 inserts, ns, **median of 5 runs** (max
column shows min..max over the runs):

| case | mean | p50 | p99 | p99.9 | max | >100 µs |
|---|---|---|---|---|---|---|
| MI hashed, no reserve | 52 | 42 | 84 | 1,292 | 8,357,750 [8.23M..8.67M] | 6 |
| MI hashed, reserve(N) | 37 | 42 | 84 | 875 | 35,792 [35.5k..42.2k] | 0 |
| MI hashed, reserve + pre-touched pmr arena | 28 | 41 | 42 | 125 | 10,250 [5.1k..29.8k] | 0 |
| std::unordered_map, no reserve | 50 | 41 | 84 | 1,375 | 6,460,750 [5.69M..10.26M] | 5 to 6 |
| boost::unordered_flat_map, no reserve | 53 | 41 | 125 | 1,250 | 13,358,416 [13.16M..13.69M] | 8 |
| boost::unordered_flat_map, reserve(N) | 28 | 41 | 84 | 167 | 14,500 [6.1k..26.6k] | 0 |

(ns. Values of 41/42/84 are timer ticks: anything below 41 ns reads as 0 or
41.) After `reserve`, the remaining p99.9 comes from allocating fresh nodes,
which includes the first touch of new pages. The arena row hands out nodes
from memory that was already touched, and that p99.9 drops to about 125 ns
(in one run of five it was 1,208 ns, so treat it as "much lower", not as a figure). The
"how" of pools and pre-faulting is the subject of `../advanced_memory`.
**Ordered indices: is rebalancing a tail source?** An ordered index is a
red-black tree. In `ord_index_node.hpp` (`rebalance`, read in Boost 1.90) an
insert recolours nodes while climbing toward the root as long as the uncle is
red. The branch that rotates does at most two rotations, then the loop ends
because the parent is black. So rebalancing after an insert is bounded work,
and no single insert re-walks the whole container as a rehash does. (I read
the insert path only, not the erase path.) Measured with `build/bench_ordered`,
1,000,000 ops on one `ordered_unique` index, nodes from a pre-touched arena so
malloc and page faults are out of the loop; ns, median of 5 runs [min..max]:

| op | p50 | p99 | p99.9 | max | samples >100 µs, per run |
|---|---|---|---|---|---|
| insert, ascending keys | 208 [208..209] | 292 [292..333] | 500 [417..500] | 41,375 [30.1k..168.8k] | 0, 0, 1, 0, 0 |
| insert, random keys | 459 [459..500] | 1,250 [1,208..1,375] | 3,709 [1,500..4,666] | 53,459 [36.5k..508.4k] | 0, 0, 0, 0, 1 |
| erase, random keys | 459 [417..500] | 1,083 [1,042..1,166] | 2,709 [1,292..4,291] | 40,667 [34.8k..831.2k] | 0, 0, 0, 1, 0 |

Read it: there is no repeatable millisecond spike. Zero or one sample per
million exceeded 100 µs in each run, against 5 to 8 per run for an unreserved
hash. Random keys cost about 2.2x ascending keys at p50. I did not separate
cache misses from rebalancing work, so do not attribute that gap to either.
The rare 100 µs to 800 µs maxima came on a machine shared with other jobs and
I did not isolate their cause. The tail you can plan for is the hash rehash;
the ordered index's cost is steady and higher per operation.
HandsOn: Run `./build/bench_rehash` twice. Compare the max column with and
without reserve. Then run `./build/bench_ordered` and count the samples over
100 µs.
HandsOnKey:
  - Without reserve, max is in milliseconds and a handful of samples exceed
    100 µs. With reserve, max is tens of µs and none exceed 100 µs. The mean
    moves by tens of ns while the max moves by orders of magnitude. That is the "mean is fine, tail is ugly" picture.
Source: bench/bench_rehash.cpp, bench/bench_ordered.cpp (run 5 times this session; bench/results/summary.txt) ; /opt/homebrew/include/boost/multi_index/detail/ord_index_node.hpp (`rebalance`, insert path) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/hash_indices.html ("Upon construction, max_load_factor() is 1.0"; reserve = rehash(ceil(n/max_load_factor()))) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html ("all public methods and types of the first index specified are inherited by multi_index_container") ; checks/tpl/l3-reserve.cpp (container-level reserve leaves index 2 rehashing: run this session)

### CONCEPT M3C4  Pointer chasing: count the dependent cache misses per lookup
In a lookup the next address depends on the last load, so the CPU cannot
overlap the misses. An **ordered** index on n elements is a tree of depth
about log2 n, so at 4M orders that is roughly 22 dependent node visits. A
**node-based hash** costs a bucket load plus a node load. **Open addressing**
(`unordered_flat_map`) keeps elements in the bucket array itself. A **dense
array** indexed by your own id costs one load. Measured, dependent lookups,
ns per lookup (p50, median of 5 runs; 4M column also shows min..max):

| structure | 10k orders (cache-resident) | 4M orders (DRAM) |
|---|---|---|
| dense vector<Order>[id] | 7.8 | 92 [90..94] |
| boost::unordered_flat_map | 19.5 | 145 [143..147] |
| MI hashed_unique | 16.9 | 237 [234..246] |
| std::unordered_map | 19.5 | 288 [280..289] |
| MI ordered_unique | 85.9 | 1,025 [939..1,057] |

At 10k everything fits in cache and the differences are small. At 4M each
pointer hop is a DRAM trip, and the ranking follows the miss count. The
ordered index is about 3.6x to 11x slower than the other four here. Use
ordered indices for **range** questions (best-N), not for id lookup.
HandsOn: Run `./build/bench_lookup`.
HandsOnKey:
  - At 4M the order is dense < flat_map < MI hashed ≲ std::unordered_map ≪
    MI ordered. The ordered index is several times slower than any hash.
Source: bench/bench_lookup.cpp (run 5 times this session; each key depends on the previous result; the modulo cost is included in every row) ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ("held directly in the bucket array ... open addressing")

### CONCEPT M3C5  Allocation order vs traversal order, and the allocator parameter
A best-N query walks the book in **price order**. Whether that walk is fast
depends on where the nodes sit, which depends on **allocation order**. When
orders arrive in price order, each next node is right next door: about 6.6 ns per
node. When they arrive at random prices (the normal case), each step is a
cache miss: about 81 ns per node, **about 12× slower**, same data, same
container (median of 5 runs; min..max 6.4..6.6 and 78..82).
The container's last template parameter is the allocator:
`multi_index_container<Order, indexed_by<...>, std::pmr::polymorphic_allocator<Order>>`.
A bump arena (`std::pmr::monotonic_buffer_resource`) removes malloc from the
hot path and pre-touched memory removes page-fault spikes (M3C3). It does **not**
fix the 81 ns, because it lays nodes out in allocation order (also about 81 ns
per node measured, with the arena sized so it asked its upstream for exactly 1 block). Fixing traversal locality needs a different data layout: the price
ladder in Lesson 04.
HandsOn: Run `./build/bench_scatter`.
HandsOnKey:
  - Sorted arrival: under 10 ns per node with 100% "next node within 64 B".
    Random arrival: about 80 ns per node with 0%. The pmr arena rows match
    their std::allocator rows.
Source: bench/bench_scatter.cpp (run 5 times this session, N=2,000,000; bench/results/summary.txt) ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html (Allocator template parameter) ; pmr compiled and run here with Apple clang 21 / libc++

---

## Completion cards

### CARD M3-C1  [type: completion]  [topic: bytes]  [box: 1]
Q: Fill the blanks with the header sizes measured on this 64-bit build:
   ordered ____ B, hashed ____ B, sequenced ____ B, random_access ____ B (plus
   its pointer array).
Key:
  - ordered 24, hashed 16, sequenced 16, random_access 8
AcceptIf: regex(/^\D*24\D+16\D+16\D+8(?:\s*(?:B|bytes?))?(?:\s*\(?\s*(?:plus|\+)\s*(?:(?:its|an?)\s+)?(?:8\s*(?:B|bytes?)?\s*)?[^0-9)]*\)?)?\s*\.?$/i)
RejectIf: regex(/\b(?:32|80)\b|(?:plus|\+)\s*(?!8\b)\d+/i)
Concepts: M3C1
Source: CONCEPT M3C1

### CARD M3-C2  [type: completion]  [topic: tail]  [box: 1]
Q: Complete: a store with hashed indices 0 and 2 must call `s.get<0>().reserve(n)`
   and ______ before the session starts, because `s.reserve(n)` only reaches
   index ______. Use numeric indices (this store declares no tags).
Key:
  - `s.get<2>().reserve(n)`
  - index 0
AcceptIf: regex(/get\s*<\s*2\s*>\s*\(\s*\)\s*\.\s*reserve.*\b0\b/s)
RejectIf: regex(/get\s*<\s*1\s*>\s*\(\s*\)\s*\.\s*reserve/)
Concepts: M3C3
Source: CONCEPT M3C3

---

## Question cards

### CARD M3-Q1  [type: recall]  [topic: bytes]  [box: 1]
Q: Predict the node size, in bytes, of a multi_index_container of the 40-byte
   `Order` with `hashed_unique<key<&Order::id>>` and
   `ordered_non_unique<key<&Order::side, &Order::price, &Order::ts>>`. Type a
   number (an expression like `40+16+24` also works). The check measures it
   with a spy allocator.
Key:
  - 80: value 40 + hashed header 16 + ordered header 24.
AcceptIf: check(checks/l3-nodesize.sh)
Concepts: M3C1
Source: checks/tpl/l3-nodesize.cpp (measured 80 this session)

### CARD M3-Q2  [type: recall]  [topic: tail]  [box: 1]
Q: Write `void prepare(Store& s, std::size_t n)` for a store with three
   indices: 0 = `hashed_unique` id, 1 = `ordered_non_unique` price,
   2 = `hashed_non_unique` client. After it runs, n inserts must leave
   **every** bucket count unchanged.
Key:
  - `s.get<0>().reserve(n); s.get<2>().reserve(n);`
  - `s.reserve(n)` alone fails: it only reserves index 0, so index 2 still
    rehashes. The ordered index has no buckets.
AcceptIf: check(checks/l3-reserve.sh)
Concepts: M3C3
Source: checks/tpl/l3-reserve.cpp (right answer and `s.reserve(n)` wrong answer run this session)

### CARD M3-Q3  [type: apply]  [topic: lookup]  [box: 1]
Q: A smart-order-router keeps 3 million live child orders and looks each one
   up by id on every fill. Today it uses an `ordered_unique` id index "because
   ids are sorted". Estimate the dependent node visits per lookup, name a
   better index or structure, and say what you would measure to prove it.
Key:
  - About log2(3M) ≈ 21–22 dependent node visits. At that size most are cache
    misses (the 4M ordered row measured about 1,000 ns per lookup here).
  - Use `hashed_unique` (about 2 misses), `boost::unordered_flat_map`, or a
    dense id → slot array if you control the ids (one load).
  - Measure dependent-lookup latency p50/p99/p99.9 at the production working
    set, on the production box. Do not use an average from a small,
    cache-resident test.
Concepts: M3C4
Source: CONCEPT M3C4 (bench/bench_lookup.cpp)

### CARD M3-Q4  [type: apply]  [topic: stability]  [box: 1]
Q: A strategy wants a "list of my working orders in submission order, plus
   O(1) access to the k-th one", and it stores raw `const Order*` in its own
   structs. Pick the third index type, and say what you gain and give up
   compared with keeping a `std::vector<Order>`.
Key:
  - `random_access<>`: it gives `operator[]` and keeps submission order.
  - Gained: pointers, references and iterators stay valid through growth and
    through other inserts and erases. A vector moves elements when it grows,
    which would dangle those `const Order*`.
  - Given up: contiguity (elements are separate nodes, no `data()`), plus
    O(n) insert or erase away from the back, because the pointer array shifts
    (50,000 push_front took about 867 ms here).
Concepts: M3C2
Source: CONCEPT M3C2

### CARD M3-Q5  [type: recall]  [topic: scatter]  [box: 1]
Q: Same container, same 2M orders. A best-N walk costs about 6 ns per node in one
   run and about 80 ns per node in another. What differs between the runs? Does
   swapping in a pmr bump arena fix the slow one?
Key:
  - Allocation order differs. In the fast run orders arrived in price order,
    so consecutive nodes sit next to each other. In the slow run arrival
    prices were random, so every step in price order is a cache miss.
  - An arena does not fix it, because it packs nodes in allocation order too
    (measured about 81 ns per node). It removes malloc calls and page-fault
    spikes, not traversal misses. Fixing the walk needs a layout that is
    contiguous in price order, such as a price-level array.
Concepts: M3C5
Source: CONCEPT M3C5

---

## Integrate (day close)

### CARD M3-INT  [type: synthesize]  [topic: all]
Q: Your store must survive the open: 500k inserts in the first second. List
   the three things you do before the session starts and the one thing you
   measure afterwards, each tied to a mechanism from today.
Key:
  - `reserve()` on every hashed index, not just index 0, so no insert
    rehashes everything (M3C3).
  - Use hashed indices (or flat or dense structures) for id lookup and keep
    ordered indices for range queries, to cut dependent misses (M3C4).
  - Supply an allocator over pre-touched memory (the allocator parameter), so
    node allocation is not malloc plus first-touch page faults. Keep in mind
    it does not fix traversal order (M3C3, M3C5).
  - Measure per-op p99, p99.9 and max, not the mean, on the production box.
Concepts: M3C1, M3C3, M3C4, M3C5
Source: CONCEPT M3C1–M3C5

Next session: Lesson 04. When NOT to use multi_index, and the capstone.
