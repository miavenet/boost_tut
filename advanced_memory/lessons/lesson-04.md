# Lesson 04 — Traps, locality, and your own layer (~30 min)

"Behind the scenes" is convenient because you stop seeing the memory. It is also dangerous for
the same reason. Today you break the library on purpose, see where nodes actually land, and
then rebuild one layer yourself. Start by crashing something:

```
./labs/build/l4_traps outlive ; echo "exit=$?"
```

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 04-P1  [type: pretest]  [topic: lifetime]  [box: 1]
Q: Cold guess. An `hft::map` is created inside a block that also owns the `hft::thread_arena`,
   but the map is held in a `std::optional` declared OUTSIDE the block. The block ends (arena
   destroyed), then the map is destroyed. What happens?
Key:
  - It crashes with SIGSEGV (exit 139 here). The arena's region was unmapped, and the map's
    destructor walks its nodes, which lived in that region. The library's "no arena" check never
    gets a chance. CONCEPT L4C1.
Concepts: L4C1
Source: labs/l4_traps.cpp run 2026-11-13

### CARD 04-P2  [type: pretest]  [topic: sharing]  [box: 1]
Q: Cold guess. `sysctl hw.cachelinesize` on this Mac says 128. What does
   `std::hardware_destructive_interference_size` report with Apple clang 21 / libc++?
Key:
  - 256 (and `hardware_constructive_interference_size` 64). GCC 13 on aarch64 Linux also
    reported 256; clang 18 with libstdc++ did not define the constant at all. Neither is "the
    cache line size". CONCEPT L4C4.
Concepts: L4C4
Source: bench/b3_locality.cpp runs 2026-11-13 (macOS and Linux container)

### CARD 04-P3  [type: pretest]  [topic: propagate]  [box: 1]
Q: Cold guess. `std::pmr::vector<long> a(&mono); std::pmr::vector<long> b(a);` Which resource
   does `b` use: `&mono` or something else?
Key:
  - Something else: `get_default_resource()`. Copy construction calls
    `select_on_container_copy_construction`, which returns "a default-constructed
    polymorphic_allocator". Your copy silently left the arena. CONCEPT L4C2.
Concepts: L4C2
Source: labs/l4_traps.cpp pmr-copy run 2026-11-13 ; https://en.cppreference.com/w/cpp/memory/polymorphic_allocator/select_on_container_copy_construction

---

## Concept cards

### CONCEPT L4C1  Lifetime and thread coupling
The container borrows memory from the arena, so the arena must outlive it. Inside `thread_arena`,
members die in reverse declaration order (pool, gate, monotonic, region), which is the only safe
order. In your code, declare the arena **first**, then the containers. Three ways to get it wrong,
all in `labs/l4_traps`: `outlive` (SIGSEGV, exit 139: the nodes were in the unmapped region);
`static` (a function-local static multi_index built before any arena: its constructor allocates a
header node, so it throws "no thread_arena on this thread"); `thread` (a map destroyed on another
thread that has its own arena: the checked build aborts with "deallocate of memory that is not from
this thread's arena", exit 134). The same rule holds for NUMA: memory freed on another thread
must go back to the **owning** thread, for example through a queue, never into a pool on another node.
Figure: stack frame: arena declared first (destroyed last), containers declared after (destroyed first).
Elaborate: Why does `thread` abort cleanly but `outlive` crashes without a message?
ElaborateKey:
  - In `thread` the region still exists, so the checked deallocate runs and sees a pointer outside
    the current thread's region. In `outlive` the region is already unmapped; the destructor reads
    a node there and faults before deallocate is ever called.
HandsOn: Run `./labs/build/l4_traps static` and `./labs/build/l4_traps thread; echo $?`.
HandsOnKey:
  - static: "static container: hft_mem: no thread_arena on this thread". thread: the abort message
    above and exit status 134.
Source: labs/l4_traps.cpp runs 2026-11-13, raw exit codes re-recorded in _evidence/r2/labs-raw-rc.txt (2026-11-14) ; labs/hft_mem/hft_mem.hpp

### CONCEPT L4C2  Propagation and nested allocator-aware types
`polymorphic_allocator` "does not propagate on container copy assignment, move assignment, or
swap", and copy construction gets a default-constructed one (pretest 04-P3). Measured in
`l4_traps pmr-move`: move-assigning between two different resources did NOT steal the buffer
("elements copied into a new r2 buffer"); with the same resource it did. Swapping two pmr
containers "whose allocators do not compare equal results in undefined behavior". Nested types:
`polymorphic_allocator::construct` "does uses-allocator construction", so `pmr::vector<pmr::string>`
puts the string bytes in the same resource, but `pmr::vector<std::string>` does not (1 hidden
`operator new` per long string, measured). `hft::allocator` sidesteps both: `is_always_equal` and
propagation are true, so move and swap are pointer swaps, and `hft::vector<hft::string>` gets its
strings from the same thread arena with no uses-allocator machinery. Be clear what that `true`
claims: the standard wants equal allocators only when each can free the other's memory, and two
`hft::allocator`s can do that only on the same thread under the same arena. So
`is_always_equal = true` is a single-thread, single-arena assumption; the checked build enforces
it at every deallocate (the `thread` trap above is what breaking it looks like). For
`boost::multi_index_container` the allocator is the **last** template argument; on copy and move
assignment it follows the `propagate_on_container_*` traits.
Figure: pmr::vector<std::string>: vector buffer in the arena, string bytes in malloc. pmr::vector<pmr::string>: both in the arena.
Elaborate: Why does `hft::allocator` not need uses-allocator construction for `hft::vector<hft::string>` to stay in the arena?
ElaborateKey:
  - It has no state to pass down: every hft::allocator, including the string's own, finds the same
    thread arena through the thread_local pointer.
HandsOn: Run `./labs/build/l4_traps pmr-nested`, then `pmr-move`.
HandsOnKey:
  - pmr-nested: std::string version 1 operator new call, pmr::string version 0. pmr-move: across
    resources "buffer stolen? no"; same resource "buffer stolen? yes".
Source: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html ; labs/l4_traps.cpp runs 2026-11-13

### CONCEPT L4C3  Cache locality: node size, pool layout, hot/cold
A node is bigger than your value. `labs/l4_node_size` here (libc++): `std::map<long,Order>` node
64 B for a 32 B value; `std::unordered_map` 48 B; `boost::unordered_node_map` 32 B; a two-index
`multi_index` 64 B for a 24 B `Order`. libstdc++ in the Linux container gave 40 B for
`unordered_map`, so measure on your library. The line is 128 B here and 64 B on typical x86-64.
**Where nodes land** matters as much as their size (`bench/b3_locality`, two maps filled
interleaved, walk map A, 100,000 nodes, p50 per node, median of 3 runs):

```
                      ascending keys        random keys
default malloc        4 ns/node (gap 96 B)  17 ns/node
one SHARED pool       5 ns/node (gap 128 B) 26 ns/node
pool PER CONTAINER    4 ns/node (gap 64 B)  13 ns/node
```

A per-container pool keeps A's nodes next to each other. A per-thread shared pool interleaves A
and B, so A's walk touches twice the memory. `unsynchronized_pool_resource` is already
per-size-class inside, so classes never mix. **Hot/cold split** (`labs/l4_hot_cold`, 1,000,000
orders, p50 over 5 runs): scanning 3 hot fields inside a 128 B struct cost 4.04 ns/order; the same
fields split into a 24 B array cost 0.41 ns/order, about 10× less (9.6–10.3× in every run).
Figure: shared pool: A B A B A B in memory; per-container pool: A A A A | B B B B.
Elaborate: When does a per-thread shared pool beat a pool per container?
ElaborateKey:
  - When you have many small containers (or ones you never walk): one pool keeps fewer partly-used
    chunks and less memory overall. Per-container wins for a few big containers you iterate.
Source: labs/l4_node_size.cpp, labs/l4_hot_cold.cpp runs 2026-11-13 ; bench/results_r2/RESULTS.md (l4_hot_cold, 5 runs, 2026-11-14) ; bench/RESULTS.md

### CONCEPT L4C4  False sharing and alignment
Two threads writing two different variables in one cache line fight over that line. Measured
(`bench/b3_locality`, two threads, 10,000 relaxed increments per sample): counters 8 B apart p50
95,125 ns; counters `alignas(std::hardware_destructive_interference_size)` (256 here, so 256 B
apart) p50 24,583 ns, about 3.9× faster. The constant is "minimum offset between two objects to
avoid false sharing", it is implementation-defined, and it may be missing: guard it with
`#ifdef __cpp_lib_hardware_interference_size`, as cppreference's own example does (fallback 64,
"64 bytes on x86-64"). Use `alignas` on the per-thread counters and the arena's hot fields. Never
let two threads' pools share a cache line or a chunk.
Figure: one 128 B line holding counter a (thread 0) and counter b (thread 1); the line bounces between cores.
Elaborate: Why is `alignas(64)` correct on your x86 server but not enough on this Mac?
ElaborateKey:
  - The x86 line is 64 B; here it is 128 B, so two 64-B-aligned counters can still share one line.
Source: https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size ; bench/b3_locality.cpp ; bench/RESULTS.md

### CONCEPT L4C5  Which allocator for which container
| container on the hot path | memory supply | why |
|---|---|---|
| node: `multi_index`, `map`, `unordered_map`, `unordered_node_map` | the **pool** (`hft::` alias) | one node per element; freed nodes are recycled |
| flat: `flat_map`, `vector`, `unordered_flat_map` | `hft::` alias + `reserve()` at warm-up (big block via the monotonic/region layer) | growth is a full copy; a pool cannot prevent it |
| small, hard upper bound | `static_vector<T,N>` / `small_vector<T,N>`, no allocator | no allocation at all |
| per-message scratch | `hft::scratch<N>` (monotonic) + `reset()` per message | everything dies together |
| shared across threads | not hft (single-thread); a `synchronized_pool_resource` or redesign to one owner | unsynchronized means one thread |
Figure: the table above.
Elaborate: Why does the pool help an `unordered_node_map` (churn mean 81 → 37 ns here) but cannot help an `unordered_flat_map` that is never reserved?
ElaborateKey:
  - The node map allocates one node per element, which the pool recycles. The flat map allocates
    one table and grows it by rehashing into a bigger one; only pre-sizing removes that copy.
Source: bench/RESULTS.md ; CONCEPT lesson-01.md#L1C3

### CONCEPT L4C6  Rebuild one layer: a fixed-size free list
`labs/l4_rebuild_freelist.cpp` replaces the general pool with about 40 lines for one block size.
Freed blocks form a singly linked list threaded THROUGH the free blocks. `deallocate` pushes
(`n->next = head_; head_ = n;`), `allocate` pops, and an empty list refills by carving one chunk
from the upstream monotonic. LIFO reuse hands back the most recently freed block, the one most
likely still in cache. Measured: a 10,000-entry map churned 100,000 times took 40 refills, all
during the first fill. The capstone `labs/l4_capstone.cpp` puts it together: a two-index
`multi_index` order store and an `hft::flat_map` price table, arena first, reserve, seal, hot
loop. The same source builds as `l4_capstone_tripwire` (`-DHFT_MEM_TRIPWIRE=1`).
Figure: head → [free block] → [free block] → null; deallocate pushes on the left, allocate pops from the left.
Elaborate: Why is the free list's `next` pointer stored inside the freed block rather than in a separate array?
ElaborateKey:
  - A free block holds no live data, so its bytes can hold the link. The list costs no extra
    memory, and there is nothing to allocate when you free.
HandsOn: Run `./labs/build/l4_capstone tiny` and `./labs/build/l4_capstone_tripwire tiny; echo $?`.
HandsOnKey:
  - Production build: completes and prints "overflow allocations=2" (the 1 MiB region was too
    small; memory silently came from malloc). Tripwire build: "TRIPWIRE: region exhausted ..." and
    exit status 3.
Source: labs/l4_rebuild_freelist.cpp, labs/l4_capstone.cpp runs 2026-11-13 ; `tiny` runs with raw exit codes in _evidence/r2/labs-raw-rc.txt (2026-11-14)

---

## Completion cards

### CARD 04-C1  [type: completion]  [topic: lifetime]  [box: 1]
Q: Fill the gap with one word: "In a function that owns both, declare the `hft::thread_arena`
   ______ the containers, so it is destroyed after them."
Key:
  - before
AcceptIf: regex(/^\s*before\s*\.?\s*$/i)
RejectIf: regex(/after/i)
Concepts: L4C1
Source: CONCEPT L4C1

---

## Question cards

### CARD 04-Q1  [type: apply]  [topic: propagate]  [box: 1]
Q: Code. Given `struct Order { long id; long px; };` and an `Indices` type (hashed by id + ordered by
   px), write the type for an order store whose nodes come from the current thread's hft arena.
Key:
  - `hft::multi_index<Order, Indices>` (or `boost::multi_index_container<Order, Indices, hft::allocator<Order>>`).
AcceptIf: check(checks/l4_multi_index.sh)
Concepts: L4C2
Source: labs/hft_mem/hft_mem.hpp ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html

### CARD 04-Q2  [type: recall]  [topic: lifetime]  [box: 1]
Q: From memory: three lifetime or thread traps of a "behind the scenes" thread-local allocator,
   and what each one looks like when it fires in hft_mem.
Key:
  - Container outlives its arena: the destructor touches unmapped memory → crash (SIGSEGV).
  - Static/global container built before the arena exists → `std::logic_error` "no thread_arena on this thread".
  - Container destroyed (or moved and then freed) on another thread → checked abort "not from this
    thread's arena"; unchecked, corruption of the wrong pool.
Concepts: L4C1
Source: CONCEPT L4C1

### CARD 04-Q3  [type: apply]  [topic: propagate]  [box: 1]
Q: Code. `std::pmr::vector<X> v(&mono);` holds 20 long strings. Type `X` so that the string bytes
   ALSO come from `mono` (graded by counting global operator new calls).
Key:
  - `std::pmr::string`
AcceptIf: check(checks/l4_nested.sh)
Concepts: L4C2
Source: CONCEPT L4C2

### CARD 04-Q4  [type: recall]  [topic: choose]  [box: 1]
Q: From memory, fill the decision table: which memory supply for (a) a multi_index order store,
   (b) a flat_map of price levels, (c) a parser's temporaries for one message, (d) a list of at
   most 8 legs per order?
Key:
  - (a) node container → the pool (`hft::multi_index`).
  - (b) flat container → `hft::flat_map` + `reserve()` at warm-up (big block from the monotonic/region layer).
  - (c) per-message scratch → monotonic (`hft::scratch`) + `reset()` after each message.
  - (d) hard small bound → `static_vector<Leg, 8>` (or `small_vector`), no allocator.
Concepts: L4C5
Source: CONCEPT L4C5

### CARD 04-Q5  [type: apply]  [topic: sharing]  [box: 1]
Q: Code (one line). Define `struct Counters { ... a; ... b; };` with two `std::atomic<long>`
   counters, each written by a different thread, that cannot false-share on the machine you are
   on. (Graded: they must be at least one real cache line apart here.)
Key:
  - `struct Counters { alignas(128) std::atomic<long> a; alignas(128) std::atomic<long> b; };`
  - Portable spelling, with the guard from CONCEPT L4C4 (clang 18 + libstdc++ has no constant):
    `#ifdef __cpp_lib_hardware_interference_size` → `alignas(std::hardware_destructive_interference_size)`
    (256 here), `#else` → `alignas(128)`.
  - Explicit padding (`std::atomic<long> a; std::byte pad[128]; std::atomic<long> b;`) also passes:
    a and b are always more than one line apart. `alignas(64)` fails on this Mac (128 B lines)
    though it is right on x86.
AcceptIf: check(checks/l4_align.sh)
Concepts: L4C4
Source: CONCEPT L4C4

### CARD 04-Q6  [type: apply]  [topic: rebuild]  [box: 1]
Q: Code. In the free list (`struct node { node* next; }; node* head_;`), write the body of
   `do_deallocate(void* p, std::size_t bytes, std::size_t align)` for the fast path: the freed
   block must become the new head.
Key:
  - `auto* n = static_cast<node*>(p); n->next = head_; head_ = n;`
AcceptIf: check(checks/l4_freelist_push.sh)
Concepts: L4C6
Source: labs/l4_rebuild_freelist.cpp

### CARD 04-Q7  [type: apply]  [topic: choose]  [box: 1]
Q: Imagine a risk thread that keeps 5,000 tiny per-account `hft::map`s it rarely walks, and a
   matching thread that keeps one 1,000,000-order `hft::multi_index` it walks by price all day.
   Per-thread shared pool or one pool per container, for each thread? Why?
Key:
  - Risk thread: shared per-thread pool. Many small containers would waste partly-used chunks
    with one pool each, and nobody walks them.
  - Matching thread: a pool for that one big container, so its nodes stay adjacent and price walks
    touch less memory (per-container pool about half the time per node of a shared pool on random
    keys here).
Concepts: L4C3
Source: CONCEPT L4C3 ; bench/RESULTS.md

---

## Integrate (day close — one synthesis question)

### CARD 04-INT  [type: synthesize]  [topic: all]
Q: Review this design and name every problem: "main() creates one `hft::thread_arena`, builds a
   global `hft::multi_index` order store and a `std::pmr::vector<std::string>` of symbols on it,
   then starts two pinned worker threads that both insert into the order store."
Key:
  - One arena shared by two threads: hft is unsynchronized and thread-local, so the workers have
    no arena (logic_error), or with one they would free into the wrong pool. Use one arena and one
    store per pinned worker, built on that worker.
  - The arena lives on main's NUMA node (first touch by main), not the workers' nodes.
  - A global store built before or apart from its arena: lifetime coupling (static init / outlive).
  - `pmr::vector<std::string>`: string bytes escape to malloc; use `pmr::string` or `hft::string`.
Concepts: L4C1, L4C2, L4C3, L4C5
Source: CONCEPT L4C1 ; CONCEPT L4C2 ; lesson-03.md#L3C4
