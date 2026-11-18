# Lesson 04 — When NOT to use it, what to use instead, and the capstone

**Time: about 30 min. Environment for every number in this lesson:** Apple M1 Max,
Apple clang 21, `clang++ -std=c++23 -O2`, Boost 1.90, `steady_clock` with a
41.67 ns tick (41/42/83/84 ns are one or two ticks, so read them as "at or
below a couple of ticks"). The capstone and each bench were run 5 times, on a
machine shared with other jobs. Tables show the **median of 5 runs**, with
min..max where the spread is large (up to 2x for modify_price, wider for p99).
Raw runs: `bench/results/`. Learn the ratios and the mechanism, not the
nanoseconds. Re-run on your production box before you believe any of them.

Same workload, two order stores, same answers (the capstone's `selftest`
checks the checksums match). One store's `modify_price` has a p50 of
**about 10 µs** (9.7 to 17.3 µs over 5 runs). The other's is **one or two
timer ticks, 42 to 83 ns**. Neither is buggy. By the end of this lesson you
will know which data-structure decision made the difference of more than
100×, and you will remove a factor of 26 to 42 of it with one word.

**Do this now (1 min):** `./build/capstone selftest`, then
`./build/capstone mi | cut -c1-80`.

## Pretest — commit your guesses FIRST

### CARD M4-P1  [type: pretest]  [topic: ladder]  [box: 1]
Q: Cold guess. Prices on an exchange are integer ticks within a known band.
   What structure gives O(1) "best price" and "add at price p", with no tree
   at all?
Key:
  - An array of price levels indexed by `price - band_low`. Each level is a
    FIFO queue of orders (time priority), and you track the best non-empty
    index. This is the "price ladder". CONCEPT M4C2.
Concepts: M4C2
Source: CONCEPT M4C2; labs/capstone.cpp FastStore

### CARD M4-P2  [type: pretest]  [topic: capstone]  [box: 1]
Q: Cold guess. In the capstone's MIStore, `modify_price` uses `modify()`.
   What one-word change brings its p50 from about 10 µs to well under 1 µs?
Key:
  - `replace` instead of `modify`. The client index is hashed_non_unique with
    about 1,560 orders per client, so modify walks that group every time
    (Lesson 02, M2C5). Measured over 5 runs: modify p50 9.7 to 17.3 µs against replace p50
    375 to 416 ns, a factor of 26x to 42x in every run.
Concepts: M4C5
Source: labs/capstone.cpp (run 5 times this session; bench/results/); lessons/lesson-02.md#M2C5

---

## Concept cards

### CONCEPT M4C1  Honest pros and cons, and when NOT to use multi_index
| | Boost.MultiIndex |
|---|---|
| + | One node per element (1 allocation vs N), measured smaller than the hand-rolled store (127.5 vs 176.5 B/elem) |
| + | Consistency by construction: one erase, const elements, all-or-nothing insert (strong guarantee) |
| + | Stable iterators and pointers in every index; `project<>` between views |
| + | Allocator template parameter: pools and arenas plug in |
| − | Still a **node container**: every lookup is a pointer chase, and traversal locality depends on allocation order (about 81 vs 6.6 ns per node) |
| − | Ordered lookups cost about log2 n dependent misses (about 1,000 ns at 4M) |
| − | `modify` traps: erase on collision, O(group) on hashed_non_unique |
| − | Heavy templates: long compile times and error messages |

**Do not use it** on the hot path of a book or an id lookup when you control
the key space (ticks, your own ids). Arrays beat nodes there. Do not use it
for a single access path (a plain map does that). **Do use it** for the warm
side: a reference-data table, a risk or position view, a drop-copy store, or
anything with 2+ lookups where correctness matters more than nanoseconds.
Elaborate: Name one table in your own stack that fits "use it" and one that
fits "don't".
ElaborateKey:
  - "Use it": a table with several access paths, moderate update rate and
    correctness-critical consistency (for example instruments by symbol and
    by exchange id, or orders for a risk or drop-copy view).
  - "Don't": the hot-path book or the id→order map on the matching or
    trading thread, where the key space is dense and known and arrays or flat
    tables win.
Source: lessons 01–03 measurements; labs/capstone.cpp (run this session); https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html (insert: "Exception safety: Strong")

### CONCEPT M4C2  The price ladder: the book as an array of levels
`std::array<LevelList, kTicks> level` per side. Index = price tick, and each
level is a FIFO list of orders. "Add at p" is `level[p].push_back(o)`, which
costs O(1) with no comparisons. "Best" is a tracked index. When the best level
empties, walk to the next non-empty one, which is O(gap) but short near the
touch. Best-N walks adjacent array slots instead of tree nodes. Trade-offs: you
must know the price band (out-of-band prices need a fallback path), and memory
is proportional to the band, not to the order count.
Measured in the capstone (best_5, ns, median of 5 runs): MIStore p50 about
167 / p99 250 to 333, FastStore p50 41 / p99 84. FastStore's 41 is one timer
tick, so its true value is at or below that.
HandsOn: In `labs/capstone.cpp`, read `FastStore::unlink`. What happens when
the last order at the best level is cancelled?
HandsOnKey:
  - `s.best` steps away from the touch (−1 for bids, +1 for asks) until it
    finds a non-empty level. If the side is now empty, `best` becomes −1.
Source: labs/capstone.cpp (run this session)

### CONCEPT M4C3  Intrusive containers + a pre-allocated pool
`boost::intrusive::list` stores **no nodes of its own**: the links live inside
your object, through a `list_base_hook`. "Operating with intrusive containers
doesn't invoke any memory management at all". The same object can sit in
several intrusive containers at once, for example its price level and its
client list. Put the objects in a `std::vector<Node>` sized at startup with a
free-slot stack, and add, cancel and modify become link and unlink operations
with **zero allocations**. The cost: **you** own lifetime. Unlink before you
reuse a slot, and never let the pool vector reallocate, because the hooks
hold raw addresses.
HandsOn: In `FastStore`, find where a cancelled order's slot goes, and the one
line that would break everything if the pool were a growing `vector`.
HandsOnKey:
  - The slot index goes back on `free_` (`free_.push_back(it->second)`).
  - The pool is `std::vector<Node> pool_(capacity)`, created once and never
    resized. A `push_back` that reallocates would move every Node and leave
    every intrusive link dangling.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html ; labs/capstone.cpp (run this session)

### CONCEPT M4C4  Id lookup: `unordered_flat_map` vs a dense slot array
`boost::unordered_flat_map` is open addressing: elements are "held directly
in the bucket array". That means fewer misses (p50 about 145 vs 237 ns per lookup at 4M
orders against the MI hashed index, medians of 5 runs), but "pointer stability is not kept under
rehashing". So store a **slot index** in it, not the `Order` you link into
lists. If **you** issue the ids (internal order ids, or a client-order-id you
assign), make the id the slot: `slots[id]`, one load, about 92 ns at 4M. Trade-off:
dense ids need recycling and a bounded range. Exchange-assigned ids still
need a hash map. Reserve the flat map too: unreserved, its max insert was
about 13 ms (Lesson 03).
Elaborate: Why does FastStore keep `unordered_flat_map<id, uint32_t slot>`
rather than `unordered_flat_map<id, Node>`?
ElaborateKey:
  - Flat-map elements move when the table rehashes, so intrusive hooks inside
    them would dangle. The Nodes live in a pool that never moves, and the map
    only stores their slot numbers.
Source: https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; bench/bench_lookup.cpp, bench/bench_rehash.cpp (run this session)

### CONCEPT M4C5  Capstone: read the histogram, fix the store, swap the allocator
`./build/capstone` replays the same 1,000,000 deterministic operations (45%
add, 45% cancel, 7% modify price, 2% best-5, 1% client query) against 100k
resting orders. It prints p50/p99/p99.9/max and power-of-two buckets per
operation. Median of 5 runs, p50 / p99 (ns); brackets show the min..max of
the p50 where it moved by more than 15%:

| op | MI modify() | MI replace() | Fast |
|---|---|---|---|
| add | 250 / 583 | 250 / 417 | 41 / 84 |
| cancel | 167 / 2,625 (p99 ranged 750 to 2,667) | 167 / 417 | 42 / 167 |
| modify_price | 10,875 [9,667..17,292] / 48,917 | 375 / 750 | 42 [42..83] / 167 |
| best_5 | 167 / 333 | 166 / 250 | 41 / 84 |
| client_qty | 24,167 [20,500..45,500] / 72,584 | 27,250 / 65,792 | 18,125 [14,833..22,333] / 56,041 |

Read it like this. `modify→replace` is the cheap fix (Lesson 02). Arrays,
intrusive lists and a pool are the structural fix. `client_qty` is slow
**everywhere**, because summing about 1,560 scattered orders is a scan. The
fix there is a running per-client total updated on add and cancel, not a
better index.
`MIStore` takes its allocator as a template parameter, so changing where
nodes live is **one line**:
`MIStore<std::pmr::polymorphic_allocator<Order>> s(cap, true, &arena);`
HandsOn: Run `./build/capstone mi`, then `./build/capstone mi-replace`, then
`./build/capstone fast`. Write down the modify_price p99 for each.
HandsOnKey:
  - Modify p99 is in the tens of µs, replace p99 under 1 µs, and FastStore
    p99 a couple of hundred ns. Your box will differ, but the order should not.
Source: labs/capstone.cpp (run 5 times this session; bench/results/; `selftest` confirms all four variants produce the same checksum and size)

---

## Completion cards

### CARD M4-C1  [type: completion]  [topic: ladder]  [box: 1]
Q: Complete: in a price ladder, "add an order at price p" is
   `level[____].push_back(o)`, and time priority within a level comes from
   ______ order of the list.
Key:
  - `level[p]` (or `p - band_low`)
  - FIFO / insertion order (push_back at the tail, best at the front)
AcceptIf: regex(/\b(p|px|price|tick|idx)\b.*(?<!not )(?<!no )(?<!never )\b(fifo|first[- ]in|queue|insertion|arrival|append)/is)
RejectIf: regex(/(?<!not )(?<!no )(?<!never )\b(sorted|sort|lifo|reverse)\b/i)
Concepts: M4C2
Source: CONCEPT M4C2

### CARD M4-C2  [type: completion]  [topic: idmap]  [box: 1]
Q: Fill the blank: FastStore's id map is `boost::unordered_flat_map<std::uint64_t, ______>`
   because flat-map elements ______ on rehash.
Key:
  - a slot index (`std::uint32_t`), not the Node or Order
  - move (no pointer stability)
AcceptIf: regex(/(uint32|uint64|size_t|slot|index|\bint\b).*\b(mov|reloc|invalidat)/is)
RejectIf: regex(/^\s*(Node|Order)\b|\bdo not move|\bdon'?t move|never move|\b(are|is|stay|remain)\s+(pointer-)?stable\b/i)
Concepts: M4C4
Source: CONCEPT M4C4

---

## Question cards

### CARD M4-Q1  [type: recall]  [topic: ladder]  [box: 1]
Q: Write `int next_best_bid(const std::array<std::uint32_t, kTicks>& qty, int from)`.
   It returns the highest tick `<= from` with non-zero quantity, or -1 if
   there is none. This is the walk a bid ladder does when its best level
   empties.
Key:
  - `for (int px = from; px >= 0; --px) if (qty[px]) return px; return -1;`
  - Tick 0 is a valid level: a loop with `px > 0` misses it.
AcceptIf: check(checks/l4-ladder.sh)
Concepts: M4C2
Source: checks/tpl/l4-ladder.cpp (right answer and the `px > 0` wrong answer run this session)

### CARD M4-Q2  [type: apply]  [topic: capstone]  [box: 1]
Q: Allocator swap, new container. Given
   `template <class Alloc = std::allocator<Order>> using Book = multi_index_container<Order, indexed_by<ordered_non_unique<key<&Order::price>>>, Alloc>;`
   and `std::pmr::monotonic_buffer_resource arena(1 << 20);` already declared,
   write the **one statement** that declares a `Book` named `book` whose nodes
   come from `arena`.
Key:
  - `Book<std::pmr::polymorphic_allocator<Order>> book(&arena);`
  - The check confirms `book.get_allocator().resource() == &arena` and inserts
    1,000 orders. `Book<std::pmr::polymorphic_allocator<>> book(&arena);` (the
    default `std::byte` argument) is equally right: the container rebinds the
    allocator to its node type, and the check accepts it.
AcceptIf: check(checks/l4-alloc.sh)
Concepts: M4C5
Source: checks/tpl/l4-alloc.cpp (right answers `polymorphic_allocator<Order>` and `polymorphic_allocator<>`, and the `Book<> book;` wrong answer, run this session; _probes/pmr_default_arg.cpp)

### CARD M4-Q3  [type: recall]  [topic: when-not]  [box: 1]
Q: Give two situations where you should avoid Boost.MultiIndex in a
   trading system, and one where it is the right tool. Give a mechanism for
   each.
Key:
  - Not for the hot-path book when prices are bounded ticks: a node tree
    costs dependent misses and allocation-order-dependent traversal, and a
    ladder array is O(1) and contiguous.
  - Not for hot id lookup when you control the ids (dense array, one load) or
    can use an open-addressing map with slot indices.
  - Not for a single access path (plain map or flat map).
  - Right tool: multiple access paths with consistency requirements off the
    nanosecond path, because one node, const elements and one erase remove
    the hand-sync bug class.
Concepts: M4C1
Source: CONCEPT M4C1

### CARD M4-Q4  [type: apply]  [topic: idmap]  [box: 1]
Q: Different domain: a **market-data feed handler** for one venue keeps
   per-instrument state, found by the venue's 32-bit instrument id (sparse
   values up to 4 billion, about 20,000 live). Which id → state structure would
   you pick: MI hashed, `unordered_flat_map`, or a dense array? Where does the
   state itself live?
Key:
  - Not a dense array keyed by raw venue id (sparse 32-bit range, too big).
    Pick `boost::unordered_flat_map<uint32_t, uint32_t slot>`, reserved at
    startup, or remap venue ids to dense internal slots once at subscribe
    time and index an array from then on.
  - The state lives in a pre-sized vector or pool, so pointers to it stay
    valid. The flat map stores the slot, because its elements move on rehash.
Concepts: M4C4
Source: CONCEPT M4C4

### CARD M4-Q5  [type: apply]  [topic: capstone]  [box: 1]
Q: The capstone shows `client_qty` at tens of µs p50 in every variant (about
   18 to 38 µs across the four variants here), even FastStore. A risk check calls it on every new order. What do you change,
   and why does no index choice fix it?
Key:
  - Keep a running per-client aggregate (open qty, notional) and update it in
    add, cancel and modify. The query becomes one load.
  - Every index still has to visit about 1,560 scattered orders, so the cost
    is the scan and its cache misses, not the lookup.
Concepts: M4C5
Source: CONCEPT M4C5 (labs/capstone.cpp, run this session)

---

## Integrate (course close)

### CARD M4-INT  [type: synthesize]  [topic: all]
Q: Your team lead asks: "Should the new order gateway use Boost.MultiIndex?"
   Give your answer in five bullets: decision, the consistency argument, the
   two latency mechanisms that matter, and the one-line next step for memory.
Key:
  - Decision: yes for the gateway's warm order store (id / client /
    session lookups). No for the hot book and id map, which should be a
    ladder plus a dense or flat id map plus an intrusive pool.
  - Consistency: one node per order, const elements, erase-once, and an
    all-or-nothing insert remove the hand-sync bug class. Use replace or a
    rollback, never a bare modify, on unique keys.
  - Latency 1: reserve every hashed index (rehash spike: ms max behind a
    mean of tens of ns).
  - Latency 2: node containers pay dependent cache misses, and traversal
    speed depends on allocation order (about 81 vs 6.6 ns per node).
  - Memory next step: the allocator is a template parameter, so swap it in
    one line for the pooled, pre-touched allocators from `../advanced_memory`.
Concepts: M4C1, M4C2, M4C4, M4C5
Source: lessons 01–04

## Next step — one line to pooled memory
The sibling tutorial `../advanced_memory` builds drop-in allocators that give
containers pooled, pre-touched memory. Because `MIStore` takes its allocator
as a template parameter, adopting one is a one-line change in
`labs/capstone.cpp`:

```cpp
MIStore<std::pmr::polymorphic_allocator<Order>> s(cap, true, &arena);   // today: pmr arena (runs here)
// MIStore<YourPoolAllocator<Order>> s(cap, true);                      // after ../advanced_memory
```
Re-run `./build/capstone` and compare the add and cancel p99.9 and max.
