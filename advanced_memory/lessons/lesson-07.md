# Lesson 07 — Patterns, Boost vs hand-rolled, and the verdict (~30 min)

You have the parts. Today: the whole order book against `multi_index` and the pointer model on
the same workload, three more HFT patterns, then the question every team asks: is
Boost.Intrusive good enough, or should we hand-roll the lists and pools? Answer with evidence.
First, a pool that is fast in one process and 300× slower in the next (1 minute):

```
for i in 1 2 3 4 5 6; do ./labs/build/l7_ordered_free; done
```

Same machine and caveats as lesson 05: M1 Max, Apple clang 21, Boost 1.90, `-O2 -DNDEBUG`,
shared Mac, median of 5 runs with each row in its own process. The order-book table (L7C1) and the
pool rows (L7C3) are the re-run after the library fixes, `bench/results_intrusive_r2/RESULTS.md`
(5 runs, load average 5.7–6.8, no backup running); the list and id rows are
`bench/results_intrusive/RESULTS.md` (Time Machine copying; those benches did not change).
Timer floor: an empty timed region costs 19–21 ns mean (21 in `floor.txt`, 19 in the r2 floor
row), p50 0, p99 42. **A single-operation row whose mean is inside that band is AT the floor:
neither its p50 nor its mean separates it from another such row; only the generated code can.**

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 07-P1  [type: pretest]  [topic: book]  [box: 1]
Q: Cold guess. Same order-book workload. Mean cost of one ADD: intrusive lists over a slab vs
   `boost::multi_index` (hashed id + ordered price) on the pooled `hft::allocator`. Ratio?
Key:
  - About 8×: 27 ns vs 208 ns mean (p50 41 vs 208). The pool did not close the gap: the default
    allocator version was 194 ns. The difference is the work, not the allocator. CONCEPT L7C1.
Concepts: L7C1
Source: bench/b5_book.cpp ; bench/results_intrusive_r2/RESULTS.md

### CARD 07-P2  [type: pretest]  [topic: pool]  [box: 1]
Q: Cold guess. 50,000 live 64 B objects; destroy a random one, create one, 400,000 times.
   `boost::object_pool` vs a plain free list: what is object_pool's p99?
Key:
  - In half the processes about 32,500 ns (p99; 5 of 10 runs) against 42 for the free list; in
    the others object_pool was as fast as the free list. `destroy()` is `ordered_free`, O(length of
    the free list); the cost depends on where the free blocks sit in memory. CONCEPT L7C3.
Concepts: L7C3
Source: bench/b6_handroll.cpp ; bench/results_intrusive/run1..5.txt ; bench/results_intrusive_r2/run1..5.txt ; labs/l7_ordered_free.cpp ; /opt/homebrew/include/boost/pool/pool.hpp line 480

### CARD 07-P3  [type: pretest]  [topic: handroll]  [box: 1]
Q: Cold guess. Static instruction count of `erase` for a Boost `list` (default options) and for a
   hand-rolled circular doubly linked list, at -O2 -DNDEBUG?
Key:
  - Boost default (safe_link + constant-time size): 8; hand-rolled: 5; Boost with
    `normal_link` + `constant_time_size<false>`: 4. The extra Boost instructions are the size
    counter (3) and the safe-mode reset (1 `stp` of zeros). CONCEPT L7C3.
Concepts: L7C3
Source: bench/results_intrusive/asm.md

---

## Concept cards

### CONCEPT L7C1  The order book, all four designs, one workload
`labs/hft_mem/hft_book.hpp` + `labs/l7_order_book`: orders in `hft::slab`; hooks for price level
(FIFO), client, pending-send, pending-ack; id → `Order*` in a `hft::unordered_flat_map` reserved 2×
(or the id IS the slab handle, no map). Cancel = find + unlink from each list it is in (checked
with `is_linked()`) + destroy. The lab's hot section made 0 `operator new` calls, and a stale
handle id was rejected after its slot was reused. `bench/b5_book`: 50,000 resting orders, 400,000
ops (35% add, 35% cancel, 20% reduce qty, 10% walk one level of about 49); ns, median of 5
(`bench/results_intrusive_r2/RESULTS.md`):

```
                         add mean/p50/p99.9   cancel mean/p50/p99   walk p50   faults
A  intrusive+slab+map      27 /  41 /   125     41 /  42 / 125        458        0
A2 intrusive, id=handle    23 /  41 /    42     22 /  41 /  42        417        0
B  multi_index malloc     194 / 208 /   334     95 /  84 / 167        667        1
C  multi_index hft pool   208 / 208 /   458     87 /  83 / 209        709        0
D  list<Order*>+umap      117 / 125 /   333    132 / 125 / 292        542       98
```

What each timed "add" does: D makes 4 allocations and takes page faults in the loop (98); B/C
make 1 node and insert into a hashed and an ordered index (ordered insertion is `log(n)` per the
multi_index reference); A/A2 make 0 and do **6 hook operations**: `add` links 3 (level, client,
send), then the bench calls `mark_sent` (unlink send, link ack) and `mark_acked` (unlink ack),
leaving 2 linked; cancel unlinks those 2. So A's add carries two state transitions B/C do not do.
The pool did not help multi_index add (194 → 208); cancel was 95 → 87 ns mean, spreads 95..108 vs
78..88; why is **not isolated**. A and A2 differ only in the id index, so their cancel gap (41 vs
22 ns mean) is the flat map's find + erase. The map is now reserved 2× (lesson 06 L6C3); in the same
session the pre-fix 1× map gave A cancel 33 ns, and a 1× build in `_evidence/r3/b5-A-isolation.txt`
gave 47 vs 60 ns (2×) with overlapping spreads: the bigger table is the likely cost, not proven.
Walk: B/C iterate the ordered index, D chases node → Order; why A walks slower than A2 is **not
isolated** (A's walk p99 spread 750..1375 ns overlaps A2's 625..1458).
Figure: the table above as bars per operation.
Elaborate: Why did the pooled allocator barely change multi_index's add time?
ElaborateKey:
  - The allocation was never the main cost of that add: hashing, the ordered (tree) insert and
    rebalancing are, and they are the same with any allocator.
HandsOn: Run `./labs/build/l7_order_book`.
HandsOnKey:
  - pending-send 1 → 0 and pending-ack 0 → 1 after mark_sent; level qty 4900 → 4840 after
    reduce; mass cancel of client 3: 3125 orders; 0 operator new in the hot section; the stale
    handle id → "false (stale id rejected)".
Source: labs/l7_order_book.cpp run 2026-11-14 ; bench/b5_book.cpp ; bench/results_intrusive_r2/RESULTS.md ; _evidence/r3/b5-A-isolation.txt ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html

### CONCEPT L7C2  Three more patterns: timer wheel, pending-ack queue, LRU
`labs/l7_patterns`, each an intrusive list over a slab. **Timer wheel**: 64 buckets, each an
intrusive list (`constant_time_size<false>`, 16 B header); schedule = push into
`(now + delay) & 63`; cancel = `erase(s_iterator_to(t))` in O(1); each tick fires that bucket's
due timers with `remove_and_dispose_if` (a delay of 70 wraps into slot 6 and fires at tick 70, not
6). **Pending-ack queue**: orders join at the back when sent, so the front is always the oldest;
an out-of-order ack unlinks from the middle in O(1); the timeout check looks only at `front()`.
**LRU**: hit = unlink + push_back; miss when full = evict `front()`; the key lookup is an
intrusive `unordered_set` with `key_of_value` on the same object (`1,2,3,1,4,5` evicts 2 then 3,
leaving `1 4 5`). The free list inside `hft::slab` is the fourth.
Figure: wheel of 64 slots, each a short chain of timers; arrow "now" advancing one slot per tick.
Elaborate: Why does the timer cancel need no search even though the wheel has 64 buckets?
ElaborateKey:
  - The timer stores its due tick, so its bucket is `due & 63`, and `s_iterator_to` gives the
    iterator from the timer itself.
HandsOn: Run `./labs/build/l7_patterns`.
HandsOnKey:
  - Timers 2 (due 5) and 3 (due 70) fire, 0 live after 80 ticks; orders 100 and 101 escalate, 2
    still waiting; LRU evicts 2, 3, ends `1 4 5`.
Source: labs/l7_patterns.cpp run 2026-11-14

### CONCEPT L7C3  Boost.Intrusive vs hand-rolled: the speed evidence
Same workloads (`bench/b6_handroll`, one variant per process, 5 runs). Four chunks, one question
each; the one-line answer comes first, the numbers after.

**1. Lists: timing cannot tell them apart.** (50,000 objects, 1,024 lists.)
- push_back and erase are at the timer floor for every variant (mean 19–21 ns, inside the floor
  band): hand dlist, `bi::list` safe/normal × size/no-size, hand index list. A mean of 19 is not
  "faster than the timer", it is the timer.
- Walk of about 49: 167 ns p50 for every pointer list (Boost or hand), 209 for the index list
  (every run), 208 for `bi::slist`. `slist::erase` is linear: 91 ns mean.

*Checkpoint: which list operation here is NOT at the floor, and why?*

**2. Generated code: where the lists DO differ** (`bench/results_intrusive/asm.md`, static counts).
- erase: 8 instructions Boost default, 7 normal_link, 5 safe + no size, 4 normal + no size, 5 hand.
- push_back: 9 Boost default, 5 no size, 5 hand. With asserts on, Boost's safe push_back grows to
  25 instructions (the asserts).

**3. Id index: one real gap.**
- Find: hand hash index and `bi::unordered_set` 25 ns mean each; `bi::set` 94; `unordered_flat_map` 21.
- erase+insert: 28 vs 40 ns mean (spreads 27–28 vs 40–40), so Boost's is reliably about 12 ns
  slower here. The cause is not isolated: one candidate is that Boost's buckets are singly linked,
  so `erase(it)` walks the chain to the predecessor, where the hand index keeps `pprev`.

*Checkpoint: name the one Boost-vs-hand gap the timer could see.*

**4. Pools: fast, except when a process is unlucky** (r2).
- Hand free list, `hft::slab`, `boost::pool<>` malloc/free: 19 ns mean, all at the floor.
- Slow processes (p99 31,958–32,917 ns; the rest p99 42): `pool<>::ordered_free` 4 of 5 runs in
  the first set, 3 of 5 in r2; `boost::object_pool` 3 of 5, then 2 of 5. Each process places its
  chunks independently.
- `l7_ordered_free` isolates it: in 10 runs (`_evidence/r3/lab-repeats.txt`), 7 processes had
  65.3% of live objects above the lowest free block (the free-list walk passes them), mean
  1,429–1,470 ns; 3 had 0.0%, mean 4.0 ns.
Figure: bar pairs Boost vs hand per operation, all touching the "timer floor" line except slist erase, set and object_pool.
Elaborate: The lists' timings are all the same here but the instruction counts are not. What does that tell you to do?
ElaborateKey:
  - The difference (a size counter, one reset store) is smaller than this timer can see; it may
    matter in a tight loop on your box, so measure there with a batched bench or perf counters
    before hand-rolling for it, or simply turn the Boost options off.
Source: bench/b6_handroll.cpp ; bench/results_intrusive/RESULTS.md ; bench/results_intrusive_r2/RESULTS.md ; bench/results_intrusive/floor.txt ; bench/results_intrusive/asm.md ; _evidence/r3/lab-repeats.txt (l7_ordered_free x10) ; /opt/homebrew/include/boost/pool/pool.hpp line 480 ; /opt/homebrew/include/boost/intrusive/hashtable.hpp line 2898

### CONCEPT L7C4  Beyond speed: what you own, and what each side can express
Measured here (`bench/compile_time.sh`, r2): the hand-rolled lists + hash index + pool are **224
lines** (`hft_handroll.hpp`) plus the 205-line slab (both grew by about 50 lines when copy,
exception and double-destroy bugs were fixed in them: that is the ownership cost);
Boost.Intrusive is **40,885 lines** in 98 headers you do not maintain. Compile time of two small
TUs that build the same three structures: 0.495 s Boost vs 0.369 s hand (median of 5), 67,642 vs
52,227 preprocessed lines. What **Boost**
gives you that a hand-roll does not: safe-mode checks (assert on destroy-while-linked, double
insert), `iterator_to` on every container, disposers, `key_of_value` + heterogeneous lookup,
rbtree / AVL / splay / scapegoat / treap balancing you would not want to write, 28 option
macros in `options.hpp` (`constant_time_size`, `link_mode`, `cache_last`, `power_2_buckets`, `store_hash`,
`optimize_size`, `void_pointer` for offset pointers). Costs: template-heavy errors, option
defaults that are not the fastest (safe_link + constant size), a free `container_of` you trust.
What a **hand-roll** expresses easily: 32-bit index links (8 B hooks, relocatable), a generation
in the handle, links packed with other fields, one hook reused as a state machine your way,
custom layout (all hooks in a hot header). Boost can do relocation (offset_ptr hooks, 16 B) and
lets you write your own NodeTraits for its algorithms, but this course did not build an
index-based NodeTraits.
Figure: two columns, "Boost gives" / "hand-roll gives", with the line counts at the top.
Elaborate: Name one thing you must test yourself if you hand-roll that Boost's safe mode would have caught.
ElaborateKey:
  - Destroying or reusing an object that is still linked (and inserting a linked object twice):
    Boost asserts in debug builds; your hand-rolled list silently corrupts.
Source: bench/results_intrusive_r2/compile_time.txt ; /opt/homebrew/include/boost/intrusive/options.hpp ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/node_algorithms.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html

### CONCEPT L7C5  The verdict
**Boost.Intrusive is good enough by default.** On this machine its lists and hashed index are
within the timer floor of the minimum hand-rolled versions (one measurable gap: unordered_set
erase+insert ≈ 12 ns slower), and its debug checks catch the worst traps. Set the options
yourself: member hooks, safe_link in all builds (the release cost was not measurable here),
`constant_time_size<false>` where you never ask `size()`, `power_2_buckets` and your own bucket
array. **Hand-roll** the pool (an O(1) LIFO free list: `hft::slab`, not `boost::object_pool`,
whose ordered free hit p99 32 µs), handles with generations, and index links when you need 8 B
hooks or a relocatable slab. Decision table (this machine; bytes for one Order with 2 lists):

| need | intrusive + slab | multi_index + pool | flat containers | plain std (pointer model) |
|---|---|---|---|---|
| object in several structures | yes, 16 B per list | yes, 1 node, N indices | no (copies or ids) | yes, via pointers |
| cancel given id (mean) | 22–41 ns | 87 ns (pool) / 95 (malloc) | erase shifts elements | 132 ns |
| hot-path allocations | 0 | 1 node per add | 0 if reserved | 4 per add |
| bytes per order (l5_models) | 94 | 82 | 40 (one index) | 109 |
| walk a level of ~49 (p50) | 417–458 ns | 667–709 ns | contiguous scan | 542 ns |
| safety / misuse | worst: lifetimes are yours | best: owns its elements | good | medium |
| coupling | type names its lists | key extractors only | none | none |

**Intrusive is the wrong choice** when an object lives in one structure (a reserved `flat_map`
or vector is smaller and scans faster: 375 vs 875 ns per 1,024 orders lean vs hooked), when lookup
by key dominates and membership changes are rare (multi_index is safer at a measured cost), when
objects cross threads or outlive their lists (nobody owns them), or in a public API (Boost's own
advice).
Figure: the decision table above.
Elaborate: Give the single strongest reason to pick multi_index over intrusive + slab for an order store.
ElaborateKey:
  - It owns its elements: no destroy-while-linked, no dangling pointer, no forgotten unlink; you pay
    about 7–8× on add and 2–4× on cancel here.
Source: bench/results_intrusive/RESULTS.md ; labs/l5_models.cpp ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage_when.html ; CONCEPT L7C3 ; CONCEPT L7C4

---

## Completion cards

### CARD 07-C1  [type: completion]  [topic: pool]  [box: 1]
Q: Fill the gap with one word: "`boost::object_pool::destroy` calls ______ `free`, which walks the
   free list and can cost O(N)."
Key:
  - ordered
AcceptIf: regex(/^\s*`?ordered(_free)?`?\s*\.?\s*$/i)
RejectIf: regex(/unordered/i)
Concepts: L7C3
Source: /opt/homebrew/include/boost/pool/object_pool.hpp line 126 ; /opt/homebrew/include/boost/pool/pool.hpp line 480

### CARD 07-C2  [type: completion]  [topic: transition]  [box: 1]
Q: Complete `void mark_sent(Order& o)`: the order leaves `send` (a `SendQueue` through
   `send_hook`) and joins `ack` (an `AckQueue` through `ack_hook`). Write the ONE missing
   statement, O(1), no search:
   ```
   ______                // leave the pending-send queue
   ack.push_back(o);
   ```
Key:
  - `send.erase(SendQueue::s_iterator_to(o));` (or `send.erase(send.iterator_to(o));`)
  - Without it the order sits in both queues. `o.send_hook.unlink()` does not compile here: it is
    "only allowed if link_mode is auto_unlink" (list_hook.hpp), and this hook is safe_link.
AcceptIf: check(checks/l7_transition_fill.sh)
Concepts: L7C1, L7C2
Source: checks/l7_transition_fill.sh ; checks/l7_transition.sh ; labs/hft_mem/hft_book.hpp ; /opt/homebrew/include/boost/intrusive/list_hook.hpp lines 153–157 ; _evidence/unlink-safe-link.txt

---

## Question cards

### CARD 07-Q1  [type: recall]  [topic: book]  [box: 1]
Q: From memory: the steps of an O(1) cancel in the intrusive book, and what each design (intrusive,
   multi_index, pointer model) allocates per ADD.
Key:
  - Find by id (hash probe, or the id is the slab handle), unlink from every list it is in
    (`is_linked()` tells which state lists), release the slot to the slab.
  - Allocations per add: intrusive 0; multi_index 1 node; pointer model 4 (Order, 2 list nodes, hash node).
Concepts: L7C1
Source: CONCEPT L7C1

### CARD 07-Q3  [type: recall]  [topic: verdict]  [box: 1]
Q: From memory: is Boost.Intrusive good enough here? Name what to take from Boost, what to
   hand-roll, and the one measured result that argues against `boost::object_pool`.
Key:
  - Take Boost's lists / unordered_set / trees with chosen options (member hooks, safe_link,
    constant_time_size<false> where size() is unused, your own buckets); speed within the timer
    floor of hand-rolled, plus debug checks and trees.
  - Hand-roll the pool (O(1) LIFO free list), generation handles, and index links if you need 8 B
    hooks or relocation.
  - object_pool's destroy is ordered_free: p99 ≈ 32.5 µs in 5 of 10 runs here (3 of 5, then 2 of
    5); a plain free list never left the floor.
Concepts: L7C3, L7C4, L7C5
Source: CONCEPT L7C5

### CARD 07-Q2  [type: apply]  [topic: transition]  [box: 1]
Q: Code. Write the body of `void mark_sent(Order& o)`: the order leaves `send` (a `SendQueue`
   through `send_hook`) and joins `ack` (an `AckQueue` through `ack_hook`), O(1), no allocation.
Key:
  - `send.erase(SendQueue::s_iterator_to(o)); ack.push_back(o);`
AcceptIf: check(checks/l7_transition.sh)
Concepts: L7C1, L7C2
Source: checks/l7_transition.sh ; labs/hft_mem/hft_book.hpp

### CARD 07-Q4  [type: apply]  [topic: verdict]  [box: 1]
Q: Transfer. A quote cache keeps, per symbol, at most 20 quotes sorted by price, read by
   scanning; quotes are never in any other structure, and updates replace a quote by price. Intrusive
   + slab, multi_index, or a flat container? Why?
Key:
  - A flat container (`static_vector` / `flat_map` of ≤ 20, or a sorted small array): one structure,
    no cross-membership, scans dominate, and contiguous storage is smaller and faster to scan
    (lean 375 vs hooked 875 ns per 1,024 records here). Intrusive's strength (membership in many
    structures, O(1) unlink by object) is unused.
Concepts: L7C5
Source: CONCEPT L7C5 ; bench/results_intrusive/RESULTS.md

### CARD 07-Q5  [type: apply]  [topic: patterns]  [box: 1]
Q: Transfer. A risk check must expire 10,000 per-order "throttle" timers at 1 ms granularity, and
   90% are cancelled before they fire. Sketch the structure and the cost of schedule, cancel and
   one tick.
Key:
  - Timer wheel: power-of-two array of intrusive lists (no size counter), timers in a slab, each
    timer stores its due tick. Schedule: slab create + push_back into `due & mask`, O(1).
    Cancel: `erase(s_iterator_to(t))` + destroy, O(1), no search. Tick: walk one bucket; delays
    longer than the wheel wrap, so compare `due` before firing.
Concepts: L7C2
Source: labs/l7_patterns.cpp

### CARD 07-Q6  [type: apply]  [topic: handroll]  [box: 1]
Q: Transfer. A colleague wants to hand-roll every list "because Boost's erase is 8 instructions and
   ours is 5". What do you check first, and what do you suggest instead?
Key:
  - Check whether the difference is visible on the real workload at all (here every list variant was
    at the timer floor; use a batched bench or perf counters on the target box).
  - Suggest Boost with `constant_time_size<false>` (5 instructions, the same as the hand-roll here)
    and keep safe_link for its checks; hand-roll only what Boost cannot express that you need
    (index links, generations).
Concepts: L7C3, L7C4
Source: bench/results_intrusive/asm.md ; CONCEPT L7C3

---

## Integrate (day close — one synthesis question)

### CARD 07-INT  [type: synthesize]  [topic: all]
Q: Write the one-page design note for a new order gateway: storage, hooks and lists, id index,
   state machine, pool, what Boost provides and what you hand-roll, and two risks with their
   mitigations. Use numbers from this machine only as ratios.
Key:
  - Storage: one `hft::slab<Order>` per pinned thread, carved from its arena; LIFO free list;
    try_create → reject on exhaustion; handles {index, generation} as the client order id.
  - Hooks (Boost member hooks, safe_link): price level, client, one state hook for
    send/ack (or one each), membership by `is_linked()`.
  - Id index: the handle (no map) if you assign ids; otherwise a flat map reserved with headroom.
  - Boost: lists, unordered_set/trees, disposers, debug checks. Hand-roll: slab, handles, index links if needed.
  - Ratios: add ≈ 7–8× cheaper than multi_index, cancel 2–4× cheaper, 0 allocations vs 4 for the pointer model.
  - Risks: destroy-while-linked / ABA (debug asserts in CI, handles, unlink-then-dispose); cross-thread
    use (single owner thread, queues).
Concepts: L7C1, L7C2, L7C3, L7C4, L7C5
Source: CONCEPT L7C1 ; CONCEPT L7C5 ; lesson-06.md#L6C1
