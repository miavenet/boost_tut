# Lesson 01 — Use it first: one line from malloc to pooled (~25 min)

Imagine a market-data handler that is fast on average and still loses the race now and then.
The profiler shows nothing hot. The culprit hides in one call you never wrote: the `malloc`
inside `map::emplace`. In this lesson you do not build anything clever. You USE a small
library, `labs/hft_mem/hft_mem.hpp`, change one line, and watch the tail go away. Lessons 02–04
open the box.

**This will feel slower and harder than reading, and that is the point.** Guessing before you
are taught, and recalling without looking, feel worse in the moment than re-reading; re-reading
feels smooth because familiarity passes for mastery (the fluency illusion). The effort is what
makes it stay. Wrong pretest guesses are expected and cost nothing.

Setup, once (2 minutes, from the tutorial directory):

```
bash labs/build.sh          # builds every lab into labs/build/ and every bench into bench/build/
```

Every number below was measured on the authoring machine: Apple M1 Max, macOS, Apple clang 21,
`-std=c++23 -O2`, page size 16 KiB, timer step about 41 ns. The Mac was shared with other jobs
(not an isolated core), so each number is a median of 5 runs with its [min–max] range
(`bench/results_r2/RESULTS.md`; raw runs beside it). A tuned x86-64 Linux server with 4 KiB
pages will show different numbers. Trust the mechanisms and the ratios; re-run on your own box.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### CARD 01-P1  [type: pretest]  [topic: tail]  [box: 1]
Q: Cold guess. A loop allocates 200,000 objects of 256 B with `new`, writes one byte into each,
   and keeps them all alive. Page size is 16 KiB. Roughly what fraction of the allocations are
   slow (take more than half a microsecond, ten times the typical one), and why?
Key:
  - About 1 in 64 (≈1.5%). 16384 / 256 = 64, so every 64th object lands on a page nobody has
    touched yet, and that first write takes a page fault. Measured: 3,138 page faults inside the
    loop and 3,126–3,128 samples ≥ 0.5 µs, in each of 15 runs (about two thirds of them ≥ 1 µs).
    If you guessed "none, malloc is fast", that surprise is the lesson: the cost is the kernel's,
    not malloc's. CONCEPT L1C1 explains it.
Concepts: L1C1
Source: labs/l1_tail_histogram.cpp ; bench/results_r2/l1_tail_*_run*.txt (15 runs, 2026-11-14)

### CARD 01-P2  [type: pretest]  [topic: use]  [box: 1]
Q: Cold guess. An order book `std::map<long, Order>` does 100,000 "cancel oldest, add newest"
   operations. You change the one line `using Book = std::map<long, Order>;` to
   `using Book = hft::map<long, Order>;`. How many global `operator new` calls happen in the hot
   loop before and after?
Key:
  - Before: 100,000 (one node per insert). After: 0. The node is taken from a pool that the freed
    node was just returned to. CONCEPT L1C2.
Concepts: L1C2
Source: labs/l1_one_line.cpp (run 2026-11-13)

### CARD 01-P3  [type: pretest]  [topic: tripwire]  [box: 1]
Q: Cold guess. After warm-up you call `arena.seal()`. In the hot loop, a `hft::flat_map` that was
   never `reserve()`d has to grow. What happens: nothing, a slow malloc, or something else?
Key:
  - It throws `std::bad_alloc`, on purpose: the sealed arena refuses any request for new memory.
    In a test, that turns a hidden hot-path allocation into a loud failure. CONCEPT L1C4.
Concepts: L1C4
Source: labs/l1_tripwire.cpp (run 2026-11-13)

---

## Concept cards

### CONCEPT L1C1  Why the general-purpose allocator is a tail-latency problem
The average `malloc` is fast. The tail is not, for three reasons. (1) **First touch:** fresh heap
memory is only address space; the first write to each page takes a page fault. (2) **Locks and
arenas:** a shared allocator must synchronise threads; glibc's malloc(3) says it "creates
additional memory allocation arenas if mutex contention is detected". (3) **Scattered nodes:**
node containers end up spread over the heap, which costs cache misses on every walk (lesson 04).
Measured with `labs/l1_tail_histogram` (200,000 × 256 B; median [min–max], MALLOC over 15 runs,
ARENA over 5):

```
MALLOC (new in the hot path)   page faults in loop: 3138 (every run)
  mean=45 [44–46] ns  p50=41  p99=1000 [958–1042]  p99.9=1417 [1292–1583]  max=19083 [7000–49625] ns
ARENA  (hft::thread_arena, pre-faulted, warmed)   page faults in loop: 0
  mean=22 ns  p50=41  p99=42  p99.9=83 [42–84]  max=3584 [209–6417] ns
```

The mean says "45 ns, fine". The p99 says one request in a hundred waits about a microsecond.
The max moves a lot between runs (a shared machine); the fault count and the p99 do not.
Figure: two histograms. MALLOC has a tall bar under 50 ns and a second cluster of about 3,100 samples between 0.5 and 20 µs. ARENA has only the tall bar.
Elaborate: In your own words, why does the MALLOC run take almost exactly one slow sample per 64 allocations?
ElaborateKey:
  - Each 16 KiB page holds 64 objects of 256 B. The first write to a new page takes a page fault,
    so one allocation in 64 pays for it.
HandsOn: Run `./labs/build/l1_tail_histogram`. Write down the MALLOC "page faults in loop" count and its p99.
HandsOnKey:
  - About 3,100 faults (≈ 51 MB / 16 KiB) and a p99 near 1 µs; the ARENA run shows 0 faults and a
    p99 near 42 ns (one timer step). Your exact numbers will differ.
Source: https://man7.org/linux/man-pages/man3/malloc.3.html (NOTES: arenas) ; labs/l1_tail_histogram.cpp ; bench/results_r2/RESULTS.md (2026-11-14)

### CONCEPT L1C2  The library in two lines
`hft_mem.hpp` gives every thread one memory hierarchy (region → monotonic → pool) and aliases
that use it with no resource pointer anywhere:

```cpp
hft::thread_arena arena;            // 1. once, at the top of the thread that owns the data
hft::map<long, Order> book;         // 2. any alias: map, unordered_map, unordered_node_map,
                                    //    flat_map, unordered_flat_map, vector, string, multi_index
```

The alias plugs `hft::allocator<T>` into the container's allocator slot. That allocator is an
empty class: it finds the current thread's arena through a `thread_local` pointer set by the
`thread_arena` constructor. A freed node goes back to the pool; the next insert reuses it.
Figure: the order book erases a node; the node goes back to the pool's free list; the next emplace pops the same node. malloc is never called.
Elaborate: Why does the hot loop make zero `operator new` calls even though it inserts 100,000 times?
ElaborateKey:
  - Each insert follows an erase of the same node size. The erase returns the node to the pool's
    free list and the insert takes it back, so no new memory is ever requested.
HandsOn: Run `./labs/build/l1_one_line`. Edit the single `using Book` line in `labs/l1_one_line.cpp` to `hft::map<long, Order>`, run `bash labs/build.sh`, and run it again. Then run `./labs/build/l1_every_alias`: every alias (and `hft::scratch`) in one program.
HandsOnKey:
  - First run prints "hot-path operator new calls: 100000"; after the edit it prints 0.
  - `l1_every_alias` prints one row per alias, each "in region? yes  operator new in sealed
    churn: 0", and exits 0.
Source: labs/hft_mem/hft_mem.hpp ; labs/l1_one_line.cpp, labs/l1_every_alias.cpp runs (_evidence/r2/labs-raw-rc.txt, 2026-11-14)

### CONCEPT L1C3  Node containers recycle; flat containers must be pre-sized
Node containers (`map`, `unordered_map`, `unordered_node_map`, `multi_index`) allocate one node per
element, so a pool recycles them for free. Flat containers (`vector`, `flat_map`,
`unordered_flat_map`) allocate one big block and **grow** by allocating a bigger one and moving
everything: that one insert is a huge spike even though the mean looks fine. A pool cannot fix
growth; `reserve()` during warm-up can. Measured in `bench/b1_containers` (N = 100,000 inserts;
median of 5 runs, the max with its [min–max]; `bench/results_r2/RESULTS.md`):

```
unordered_flat_map default, no reserve   mean=36  p99=42  p99.9=42   max=1024917 [922125–1228459] ns
unordered_flat_map default, reserve      mean=21  p99=42  p99.9=42   max=6167 [167–14375] ns
flat_map default, no reserve             mean=57  p99=84  p99.9=1209 max=399584 [382000–428584] ns
hft::flat_map, reserve                   mean=44  p99=84  p99.9=917  max=6625 [2625–10709] ns
```

Read the max column as orders of magnitude: about 1 ms and 0.4 ms of one growth copy without
`reserve()`, at most about 14 µs with it, in every run.

The library also has `arena.warm(bytes, count)`, which pre-carves pool blocks at start-up so even
the pool's own refills stay out of the hot path. `labs/l1_tail_histogram` changes one flag at a
time (ARENA section, 5 runs each): warmed, p99.9 = 83 ns [42–84]; `nowarm`, the pool refills
inside the loop, at least once every 256 allocations (hft_mem caps a chunk at 256 blocks), and
p99.9 rises to 2.75 µs [1.0–3.9 µs]; `nowarm nocap` (the library's own chunk limit) leaves p99.9 at 83 ns but the max becomes 365 µs [273–403 µs]. Only that flag
changed, so the refill is the cause: capped, refills are small but frequent enough to reach the
p99.9; uncapped, they are rare but one of them is huge. warm() removes both.
Figure: a flat container's capacity doubling: the insert that crosses capacity copies the whole table into a new block.
Elaborate: The no-reserve unordered_flat_map has a mean of 36 ns and a max near 1 ms. How can both be true?
ElaborateKey:
  - Almost every insert is cheap. A handful of inserts trigger a rehash that allocates a bigger
    table and moves every element; those few are huge, and averaging hides them.
Source: bench/b1_containers.cpp ; bench/results_r2/RESULTS.md (2026-11-14) ; labs/l1_tail_histogram.cpp modes nowarm / nocap ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html (reserve/rehash)

### CONCEPT L1C4  The tripwire: make a hidden allocation fail in test
Two switches, neither touches call sites. **`arena.seal()`** after warm-up: the pool can still
recycle blocks it owns, but any request for NEW memory (a pool refill, a flat container growing)
throws `std::bad_alloc`. **`-DHFT_MEM_TRIPWIRE=1`** at build time: when the region is full, throw
instead of silently falling back to malloc. Under the hood both are `std::pmr::null_memory_resource()`,
whose allocate "always throws std::bad_alloc".
Figure: warm-up (allocations allowed) → seal() → hot path (recycling allowed, new memory throws).
Elaborate: Why is a throwing tripwire better in a test than a counter you check at the end?
ElaborateKey:
  - It fails at the exact line that allocated, with a stack you can inspect, instead of a number
    at the end that says "something, somewhere, allocated".
HandsOn: Run `./labs/build/l1_tripwire` and then `./labs/build/l1_tripwire forget-reserve`.
HandsOnKey:
  - The first prints "clean: 300 levels, 1000 orders, ..." and exits 0. The second prints
    "TRIPWIRE: the hot path asked for new memory" and exits 3.
Source: labs/l1_tripwire.cpp run 2026-11-13 ; https://en.cppreference.com/w/cpp/memory/null_memory_resource

---

## Completion cards

### CARD 01-C1  [type: completion]  [topic: tripwire]  [box: 1]
Q: Fill the gap. Warm-up is done; the hot path starts on the next line. Write the ONE statement
   (the arena variable is `arena`) that makes any later request for new memory throw.
Key:
  - `arena.seal();`
AcceptIf: check(checks/l1_seal.sh)
Concepts: L1C4
Source: CONCEPT L1C4

### CARD 01-C2  [type: completion]  [topic: use]  [box: 1]
Q: Fill the gap with one word. "A pool recycles nodes, but a flat container grows by allocating
   a bigger block, so during warm-up I call ______ on it."
Key:
  - `reserve` (pre-size it to the most elements it will ever hold).
AcceptIf: regex(/^\s*`?(\w+(\.|->))?reserve(\(\s*([A-Za-z_]\w*|[2-9]\d*|1\d+)?\s*\))?`?\s*\.?\s*$/i)
RejectIf: regex(/shrink|clear|resize/i)
Concepts: L1C3
Source: CONCEPT L1C3

---

## Question cards

### CARD 01-Q1  [type: recall]  [topic: tail]  [box: 1]
Q: From memory: name the three mechanisms by which a general-purpose allocator in the hot path
   hurts tail latency, and say which one `l1_tail_histogram` makes visible.
Key:
  - First-touch page faults on fresh memory (the one the lab shows: ~3,100 faults, one per 64
    allocations, and a p99 about 20× the p50).
  - Locks / arenas: threads synchronise inside a shared allocator.
  - Scattered nodes: nodes spread over the heap cost cache misses when you walk them.
Concepts: L1C1
Source: CONCEPT L1C1

### CARD 01-Q2  [type: apply]  [topic: use]  [box: 1]
Q: Code. Here is the hot path of a book: `hft::thread_arena arena; Book book;` then
   "erase oldest, emplace newest" in a loop. Type the right-hand side of `using Book = ...;` so
   it is a NODE-based ordered map from `long` to `Order` (an order keeps the same address while
   other orders come and go) and the loop makes ZERO `operator new` calls.
   (Graded by compiling, counting `operator new`, and checking one order's address.)
Key:
  - `hft::map<long, Order>` (spelling out `std::map<long, Order, std::less<long>,
    hft::allocator<std::pair<const long, Order>>>` also passes). `hft::flat_map` makes zero
    `operator new` calls here too, but erasing the oldest shifts every element, so it fails the
    address test: a flat map is not a node container.
AcceptIf: check(checks/l1_alias.sh)
Concepts: L1C2
Source: labs/l1_one_line.cpp

### CARD 01-Q3  [type: recall]  [topic: tripwire]  [box: 1]
Q: What does a sealed arena still allow, and what does it forbid? Name the build flag that makes
   a FULL region throw instead of falling back to malloc.
Key:
  - Allowed: recycling blocks the pool already owns (erase then insert of the same size).
  - Forbidden: any request for new memory below the pool (a refill, or a big block such as flat
    container growth): it throws `std::bad_alloc`.
  - Flag: `-DHFT_MEM_TRIPWIRE=1` (overflow upstream becomes `null_memory_resource()`).
Concepts: L1C4
Source: labs/hft_mem/hft_mem.hpp (gate_resource, region_resource)

### CARD 01-Q4  [type: recall]  [topic: use]  [box: 1]
Q: Write the two lines that move a `std::vector<Quote>` in a feed-handler thread onto the
   pooled hierarchy, plus the one extra step this container needs that a `map` can skip.
Key:
  - `hft::thread_arena arena;` at the top of the thread, then `hft::vector<Quote> quotes;`.
  - Extra step: `quotes.reserve(max_expected)` during warm-up, because a vector grows by
    reallocating, which a pool does not prevent.
Concepts: L1C2, L1C3
Source: CONCEPT L1C2 ; CONCEPT L1C3

### CARD 01-Q5  [type: apply]  [topic: tail]  [box: 1]
Q: A colleague's bench of a `boost::unordered_flat_map` filled from empty prints
   `mean=36 p99=42 p99.9=83 max=1033000 ns`. They say "36 ns, ship it". In one or two sentences:
   what produced the max, and what is the fix?
Key:
  - A rehash: the table grew, allocated a bigger array and moved every element in one insert.
    (Not a page fault or a lock: one insert paid for copying the whole table.)
  - Fix: `reserve(n)` for the maximum size during warm-up (here the max fell by more than
    1,000×).
  - Self-graded against this Key: a free-text "what and why" cannot be graded honestly by regex.
Stretch: The same container with `reserve()` but on `hft::` showed p99.9 near 1 µs with zero page faults, in every recorded run, while the default-allocator `reserve` row stayed near the timer step. What do zero faults rule out, and what would you do before naming a cause?
StretchKey:
  - Zero faults rules out first-touch page faults inside the loop. Nothing else was isolated:
    the cause of that tail is unknown. Before naming one, change one thing at a time (for
    example `-DHFT_MEM_CHECKED=0`, a smaller or larger region, a profiler on that row) on an
    isolated core, and re-run several times. Do not blame "the machine" for a gap that shows
    up in every run of one row and no run of the other.
Concepts: L1C1, L1C3
Source: bench/RESULTS.md (2026-11-13) ; bench/results_r2/RESULTS.md (2026-11-14: hft rows p99.9 791–1208 ns, default reserve 42 ns, 5/5 runs)

---

## Integrate (day close — one synthesis question)

### CARD 01-INT  [type: synthesize]  [topic: all]
Q: Your system has an order book (`std::map`), a price-level table (`boost::container::flat_map`)
   and a per-symbol quote cache (`boost::unordered_flat_map`). Describe, in four steps, how you
   move all three onto `hft_mem` and prove in a test that the hot path never asks for new memory.
Key:
  - Create `hft::thread_arena` at the top of the owning thread, before any container.
  - Switch the types to `hft::map`, `hft::flat_map`, `hft::unordered_flat_map`.
  - Warm up: `reserve()` the two flat containers (and optionally `arena.warm`), build the book.
  - `arena.seal()` before the hot path (and build tests with `-DHFT_MEM_TRIPWIRE=1`): any
    allocation of new memory now throws in the test.
Concepts: L1C1, L1C2, L1C3, L1C4
Source: CONCEPT L1C2 ; CONCEPT L1C3 ; CONCEPT L1C4
