# Study guide

Time: use short sessions; stop after any step and resume at the next unchecked item.
For each pretest, write a guess, check its key at the end, then return to the next step.
For other questions, write an answer before opening the answer section.
Before checking ANY key, mark your confidence on the Confidence line: sure, unsure or a %.
A miss you marked sure (or >= 70%) is a high-confidence miss: star it (*) in your
tracking sheet, re-test it tomorrow BEFORE any other due card, and keep the star until
you get it right on a later day (confident errors are corrected but fade unless re-tested).
After a core answer and its key, try each optional stretch before viewing its key;
a skipped or attempted stretch does not change the four-box schedule.
Manual review: four boxes; after a hit review in 1, 2, 7, then 21 days.
A miss returns to box 1 and is due tomorrow. Record the date and box on paper.
To redo everything, first date and copy your tracking sheet and attempt log,
then start a blank sheet and return to the first pretest below. To redo one
lesson, keep that dated copy and all old attempts, clear only that lesson's
pretest marks, checked teaching and practice steps, boxes, due dates and
unfinished answer, then restart at that lesson's first pretest. Leave every
other lesson's marks and due dates as they are.

## lesson-01.md

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

### 01-P1 [pretest]
Cold guess. A loop allocates 200,000 objects of 256 B with `new`, writes one byte into each,
   and keeps them all alive. Page size is 16 KiB. Roughly what fraction of the allocations are
   slow (take more than half a microsecond, ten times the typical one), and why?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-P2 [pretest]
Cold guess. An order book `std::map<long, Order>` does 100,000 "cancel oldest, add newest"
   operations. You change the one line `using Book = std::map<long, Order>;` to
   `using Book = hft::map<long, Order>;`. How many global `operator new` calls happen in the hot
   loop before and after?

Confidence (mark before checking the key): sure / unsure / ___%

### 01-P3 [pretest]
Cold guess. After warm-up you call `arena.seal()`. In the hot loop, a `hft::flat_map` that was
   never `reserve()`d has to grow. What happens: nothing, a slow malloc, or something else?

Confidence (mark before checking the key): sure / unsure / ___%

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
Source: https://man7.org/linux/man-pages/man3/malloc.3.html (NOTES: arenas) ; labs/l1_tail_histogram.cpp ; bench/results_r2/RESULTS.md (2026-11-14)

**L1C1 Elaborate:** In your own words, why does the MALLOC run take almost exactly one slow sample per 64 allocations?

**L1C1 HandsOn:** Run `./labs/build/l1_tail_histogram`. Write down the MALLOC "page faults in loop" count and its p99.

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
Source: labs/hft_mem/hft_mem.hpp ; labs/l1_one_line.cpp, labs/l1_every_alias.cpp runs (_evidence/r2/labs-raw-rc.txt, 2026-11-14)

**L1C2 Elaborate:** Why does the hot loop make zero `operator new` calls even though it inserts 100,000 times?

**L1C2 HandsOn:** Run `./labs/build/l1_one_line`. Edit the single `using Book` line in `labs/l1_one_line.cpp` to `hft::map<long, Order>`, run `bash labs/build.sh`, and run it again. Then run `./labs/build/l1_every_alias`: every alias (and `hft::scratch`) in one program.

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
Source: bench/b1_containers.cpp ; bench/results_r2/RESULTS.md (2026-11-14) ; labs/l1_tail_histogram.cpp modes nowarm / nocap ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html (reserve/rehash)

**L1C3 Elaborate:** The no-reserve unordered_flat_map has a mean of 36 ns and a max near 1 ms. How can both be true?

### CONCEPT L1C4  The tripwire: make a hidden allocation fail in test
Two switches, neither touches call sites. **`arena.seal()`** after warm-up: the pool can still
recycle blocks it owns, but any request for NEW memory (a pool refill, a flat container growing)
throws `std::bad_alloc`. **`-DHFT_MEM_TRIPWIRE=1`** at build time: when the region is full, throw
instead of silently falling back to malloc. Under the hood both are `std::pmr::null_memory_resource()`,
whose allocate "always throws std::bad_alloc".
Figure: warm-up (allocations allowed) → seal() → hot path (recycling allowed, new memory throws).
Source: labs/l1_tripwire.cpp run 2026-11-13 ; https://en.cppreference.com/w/cpp/memory/null_memory_resource

**L1C4 Elaborate:** Why is a throwing tripwire better in a test than a counter you check at the end?

**L1C4 HandsOn:** Run `./labs/build/l1_tripwire` and then `./labs/build/l1_tripwire forget-reserve`.

---

## Completion cards

### 01-C1 [completion]
Fill the gap. Warm-up is done; the hot path starts on the next line. Write the ONE statement
   (the arena variable is `arena`) that makes any later request for new memory throw.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-C2 [completion]
Fill the gap with one word. "A pool recycles nodes, but a flat container grows by allocating
   a bigger block, so during warm-up I call ______ on it."

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 01-Q1 [recall]
From memory: name the three mechanisms by which a general-purpose allocator in the hot path
   hurts tail latency, and say which one `l1_tail_histogram` makes visible.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q2 [apply]
Code. Here is the hot path of a book: `hft::thread_arena arena; Book book;` then
   "erase oldest, emplace newest" in a loop. Type the right-hand side of `using Book = ...;` so
   it is a NODE-based ordered map from `long` to `Order` (an order keeps the same address while
   other orders come and go) and the loop makes ZERO `operator new` calls.
   (Graded by compiling, counting `operator new`, and checking one order's address.)

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q3 [recall]
What does a sealed arena still allow, and what does it forbid? Name the build flag that makes
   a FULL region throw instead of falling back to malloc.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q4 [recall]
Write the two lines that move a `std::vector<Quote>` in a feed-handler thread onto the
   pooled hierarchy, plus the one extra step this container needs that a `map` can skip.

Confidence (mark before checking the key): sure / unsure / ___%

### 01-Q5 [apply]
A colleague's bench of a `boost::unordered_flat_map` filled from empty prints
   `mean=36 p99=42 p99.9=83 max=1033000 ns`. They say "36 ns, ship it". In one or two sentences:
   what produced the max, and what is the fix?

Confidence (mark before checking the key): sure / unsure / ___%

Stretch (optional): The same container with `reserve()` but on `hft::` showed p99.9 near 1 µs with zero page faults, in every recorded run, while the default-allocator `reserve` row stayed near the timer step. What do zero faults rule out, and what would you do before naming a cause?

---

## Integrate (day close — one synthesis question)

### 01-INT [synthesize]
Your system has an order book (`std::map`), a price-level table (`boost::container::flat_map`)
   and a per-symbol quote cache (`boost::unordered_flat_map`). Describe, in four steps, how you
   move all three onto `hft_mem` and prove in a test that the hot path never asks for new memory.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-02.md

# Lesson 02 — Open the box: the std::pmr layers (~25 min)

Lesson 01 used `hft::thread_arena` as a black box. Inside it are three standard parts you can
use on their own, plus one you write yourself (lesson 03). Today you meet the standard parts,
one at a time, with a tracer that prints every request a layer passes down. First, run it:

```
./labs/build/l2_layers        # predict each numbered section before you read its output
```

Everything here compiles with Apple clang 21 / libc++ at `-std=c++23` (`__cpp_lib_memory_resource`
reports 201603). All labs also compiled with GCC 13.3 and clang 18 (libstdc++, Boost 1.83)
in a Linux container on this Mac, and `l2_layers` ran there.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 02-P1 [pretest]
Cold guess. With a `std::pmr::monotonic_buffer_resource mono;` you do
   `void* a = mono.allocate(64); mono.deallocate(a, 64); void* b = mono.allocate(64);`.
   Is `b == a`?

Confidence (mark before checking the key): sure / unsure / ___%

### 02-P2 [pretest]
Cold guess. The main thread calls `std::pmr::set_default_resource(&tracer)`. A different
   thread then default-constructs a `std::pmr::vector<int>` and pushes one element. Does the
   tracer see that allocation?

Confidence (mark before checking the key): sure / unsure / ___%

### 02-P3 [pretest]
Cold guess. Does this compile?
   `std::pmr::monotonic_buffer_resource mono; boost::container::pmr::flat_map<int,double> m(&mono);`

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L2C1  memory_resource and polymorphic_allocator
A `std::pmr::memory_resource` is an object with three private virtual functions you override:
`do_allocate(bytes, alignment)`, `do_deallocate(p, bytes, alignment)` and
`do_is_equal(other)`. The public `allocate`/`deallocate` call them. A
`std::pmr::polymorphic_allocator<T>` is a tiny allocator that holds a `memory_resource*` and
forwards to it. It converts implicitly from `memory_resource*`, so `std::pmr::map<K,V> m(&pool);`
works; default-constructed, it uses `std::pmr::get_default_resource()`. The pointer costs bytes:
`sizeof(std::pmr::vector<long>)` is 32 here versus 24 for `std::vector<long>` and `hft::vector<long>`.
Figure: container → polymorphic_allocator (one pointer) → memory_resource (virtual do_allocate).
Source: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator/polymorphic_allocator ; https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate ; bench/b2_alloc_flavors.cpp sizeof lines

**L2C1 Elaborate:** Why can one `std::pmr::map<int,int>` type sit on a pool in one place and on a stack buffer in another, with no change to the type?

### CONCEPT L2C2  monotonic_buffer_resource: bump, never free
It hands out memory by bumping a pointer through a buffer. `deallocate` is a no-op; memory is
released "only when the resource is destroyed" or by `release()`. When a buffer runs out it asks
its upstream for another, sizes growing "in a geometric progression". It "is not thread-safe".
Perfect for per-message scratch that dies all at once: `hft::scratch<8192>` is a fixed buffer +
monotonic + null upstream; call `reset()` (= `release()`) after each message. Measured per message
(32-int vector + 100-char string): default allocator p50 208 ns, scratch p50 125 ns, the same p50
in all 5 runs (`bench/results_r2/RESULTS.md`), so scratch is about 40% cheaper here.
Wrong for churn: an order book on a bare monotonic never reuses a freed node, so it grows forever
(`(b) pmr::map monotonic` took 1,274 page faults in the churn bench).
Figure: one buffer, a pointer moving right; freed blocks stay grey until release().
Source: https://en.cppreference.com/w/cpp/memory/monotonic_buffer_resource ; bench/RESULTS.md (2026-11-13) ; bench/results_r2/RESULTS.md (2026-11-14)

**L2C2 Elaborate:** Why is a monotonic resource ideal under a per-message parser but wrong under an order book?

### CONCEPT L2C3  The pool resources: recycle by size class
`unsynchronized_pool_resource` "consists of a collection of pools that serves requests for
different block sizes"; each pool carves chunks into equal blocks, and a freed block is reused by
the next same-size request (l2_layers section 2 prints the same address). Requests above the
largest block size go "from the upstream allocator directly". It "is not thread-safe";
`synchronized_pool_resource` is the thread-safe one. One measured trap: a refill (carving a new
chunk) can stall. In `labs/l1_tail_histogram nowarm nocap` (the library's own chunk limit) one
refill inside the loop was the 365 µs max [273–403 µs over 5 runs]; with `hft_mem`'s cap
(`pool_options{max_blocks_per_chunk = 256}`, mode `nowarm`) the max fell to 8.7 µs [7.6–11 µs]
but refills came often enough to lift the p99.9 to about 3 µs. Only that one option changed
between the two runs. So `hft_mem` caps the chunk AND offers `warm()` to do refills at start-up.
Figure: pool → size classes 8, 16, 32, ... → each a free list of equal blocks → chunks from upstream.
Source: https://en.cppreference.com/w/cpp/memory/unsynchronized_pool_resource ; https://en.cppreference.com/w/cpp/memory/synchronized_pool_resource ; https://en.cppreference.com/w/cpp/memory/pool_options ; bench/results_r2/RESULTS.md (l1_tail_histogram nowarm / nowarm nocap, 5 runs each, 2026-11-14)

**L2C3 Elaborate:** The pool reuses freed blocks, so why can it still spike in a hot loop?

### CONCEPT L2C4  null, new_delete, the default, and chaining
`std::pmr::null_memory_resource()`: allocate "always throws std::bad_alloc", deallocate "has no
effect". `std::pmr::new_delete_resource()`: plain `operator new`/`delete`. `set_default_resource(r)`
replaces the program's one default pointer and returns the old one; the initial default is
`new_delete_resource()`. Every resource takes an **upstream**, so you build chains. The one inside
`hft::thread_arena`, top to bottom:

```
container ──hft::allocator──► pool_resource (unsynchronized_pool_resource; recycles node blocks)
                                 │ refills / big blocks
                                 ▼
                              gate_resource (pass-through; seal() makes it throw)
                                 │
                                 ▼
                              monotonic_buffer_resource (bump; frees nothing early)
                                 │ geometric buffers
                                 ▼
                              region_resource (one pre-faulted mmap; Linux: huge pages, NUMA node, mlock)
                                 │ only when the region is full
                                 ▼
                              new_delete_resource()   or, with -DHFT_MEM_TRIPWIRE=1, null_memory_resource()
```
Figure: the diagram above.
Source: https://en.cppreference.com/w/cpp/memory/null_memory_resource ; https://en.cppreference.com/w/cpp/memory/set_default_resource ; labs/l2_layers.cpp run 2026-11-13

**L2C4 Elaborate:** Why is `set_default_resource(null_memory_resource())` a dangerous tripwire in a multithreaded test binary?

**L2C4 HandsOn:** In `./labs/build/l2_layers` section 5, a vector of 4,000 longs on pool → monotonic over an 8 KiB stack buffer throws early. Note at what size, and count the "upstream allocate" lines.

### CONCEPT L2C5  Boost.Container's pmr, and the no-allocator option
Boost.Container has its own `boost::container::pmr` (for pre-C++17 compilers). It is a separate
class hierarchy: a std resource cannot feed a `boost::container::pmr::flat_map` (pretest 02-P3).
Most of it needs the compiled library: without `-lboost_container` the link fails with
"Undefined symbols for architecture arm64: boost::container::pmr::monotonic_buffer_resource::monotonic_buffer_resource(...)"
(recorded in `_evidence/r2/link-error.txt`).
The easy path is to give Boost containers a **std** allocator: `boost::container::flat_map<K, V,
std::less<K>, std::pmr::polymorphic_allocator<std::pair<K, V>>>` works (note `pair<K,V>`, not
`pair<const K,V>`), and that is what `hft::flat_map` does. For small, bounded sizes, skip
allocators entirely: `small_vector<T,N>` keeps N elements inline, then uses its allocator;
`static_vector<T,N>` never allocates and throws on overflow. Careful: it throws
`boost::container::bad_alloc`, which `catch (const std::bad_alloc&)` did NOT catch here.
Figure: std::pmr resource → std::pmr::polymorphic_allocator → boost::container::flat_map (works); std resource → boost::container::pmr::flat_map (does not compile).
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/cpp_conformance.html ; labs/l2_boost_pmr.cpp run 2026-11-13 ; _evidence/r2/link-error.txt (build without -lboost_container, 2026-11-14)

**L2C5 Elaborate:** When is `static_vector<Level, 16>` a better answer than any allocator?

**L2C5 HandsOn:** Run `./labs/build/l2_boost_pmr` and read section 4.

### CONCEPT L2C6  The older Boost.Pool, and when pmr wins
Boost.Pool predates pmr. `boost::pool<>` is one block size, an object you own, no lock: 2 ns per
allocate+free pair here, the fastest row in `bench/b2_alloc_flavors`. `object_pool<T>` adds
construct/destroy and destroys leftovers at its end. `pool_allocator` / `fast_pool_allocator` are
standard allocators over a **singleton** pool, and the docs warn: "Exiting the function does NOT
free the system memory allocated by the pool allocator" (l2_boost_pool section 3: memory was still
held after the vector died, `release_memory()` freed it). Their default `Mutex` is
`details::pool::default_mutex`, which is `std::mutex` on this toolchain: 25 ns per pair versus 7 ns
for a pmr pool. The docs recommend `fast_pool_allocator` for node containers like `std::list`;
`pool_allocator` uses `ordered_malloc`/`ordered_free`, and "freeing an object can be O(N) in the
number of currently free blocks". Prefer pmr when you want per-thread ownership, a hierarchy, a
tripwire, or many block sizes; keep `boost::pool<>` for one hot fixed-size object you own.
Figure: singleton pool shared by every container of that size, behind one mutex.
Source: https://www.boost.org/doc/libs/1_90_0/libs/pool/doc/html/boost_pool/pool/interfaces.html ; https://www.boost.org/doc/libs/1_90_0/libs/pool/doc/html/boost/fast_pool_allocator.html ; /opt/homebrew/include/boost/pool/detail/mutex.hpp (default_mutex) ; /opt/homebrew/include/boost/pool/pool_alloc.hpp lines 247, 272 ; bench/RESULTS.md

**L2C6 Elaborate:** Why does a singleton pool fight the "one arena per pinned thread" design?

---

## Completion cards

### 02-C1 [completion]
Fill both gaps (two words): "On a monotonic_buffer_resource, deallocate is a ______; the
   memory comes back only on ______() or destruction."

Confidence (mark before checking the key): sure / unsure / ___%

### 02-C2 [completion]
Fill the gap. `std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, ______);` should throw
   `std::bad_alloc` the moment `buf` runs out. Type the upstream expression.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-C3 [completion]
Fill the gap with the third function name: "A `std::pmr::memory_resource` subclass overrides
   three private virtuals: `do_allocate(bytes, alignment)`, `do_deallocate(p, bytes, alignment)`
   and ______; the public `allocate`/`deallocate` call them."

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 02-Q1 [recall]
From memory, for each of these say in a few words what allocate and deallocate do:
   monotonic_buffer_resource, unsynchronized_pool_resource, null_memory_resource(),
   new_delete_resource().

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q3 [recall]
Give two facts about `boost::container::pmr` that bite people who already know `std::pmr`,
   and the easy way to put a Boost container on a std resource.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q2 [apply]
Code (one line, two declarations). Given `alignas(64) static std::byte buf[1 << 16];`, declare
   `mono` and `pool` so that a `std::pmr::map<int,int> m(&pool)` gets its nodes from `buf` and
   throws `std::bad_alloc` once `buf` is used up.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q5 [apply]
A test fixture calls `std::pmr::set_default_resource(std::pmr::null_memory_resource())` to
   catch hidden allocations, and the test runner runs fixtures on several threads. A different
   test, which never used pmr directly, starts failing with `std::bad_alloc`. Explain, and give
   a tripwire that avoids this problem.

Confidence (mark before checking the key): sure / unsure / ___%

### 02-Q4 [apply]
A team uses `std::list<Order, boost::fast_pool_allocator<Order>>` in two threads, one list per
   thread, and frees the lists at end of day. Name two costs they pay that a per-thread
   `std::pmr::unsynchronized_pool_resource` (or `hft::thread_arena`) would avoid.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 02-INT [synthesize]
Draw (in words) the hierarchy inside `hft::thread_arena` from the container down, and for each
   layer say what it would cost you to delete it.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-03.md

# Lesson 03 — The region and the thread (~30 min)

Two layers are still black boxes: the bottom one (the region that owns real memory) and the
trick that lets a default-constructed allocator find "its" arena. Today you write the first and
take apart the second. Start by watching the kernel work:

```
./labs/build/l3_first_touch
```

**Linux-only material is labelled.** The huge-page and NUMA calls below were read at man7.org
and kernel.org. They compile on this Mac behind `#ifdef __linux__` and did NOT run here (macOS
has no NUMA API). `labs/l3_numa_linux.cpp` and the Linux branch of `hft_mem.hpp` were also
compiled with GCC 13.3 and clang 18 in a Linux (aarch64) Docker container on this Mac and run
there. That container exposed no NUMA topology (`numa_node_of_cpu` returned -1, no
`/proc/self/numa_maps`), so **NUMA placement itself was not verified anywhere**. Run it on
your server.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 03-P1 [pretest]
Cold guess. You `mmap` 64 MiB (16 KiB pages = 4,096 pages) and write one byte per page, twice.
   How many page faults in pass 1, and in pass 2?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-P2 [pretest]
Cold guess (Linux, two sockets). The main thread `mmap`s a region and writes every page. Then it
   starts a worker thread pinned to a CPU on the OTHER socket and hands it the region. On which
   node is the memory?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-P3 [pretest]
Cold guess. Which costs more per 64-byte allocate+free pair on this Mac: a stateful pmr
   allocator (virtual call through `memory_resource*`) or `hft::allocator` (no pointer, direct
   call, but a `thread_local` lookup)?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L3C1  The ~40-line memory_resource that owns a region
`labs/l3_region.cpp` is the whole idea in about 35 lines:

```cpp
void* do_allocate(std::size_t bytes, std::size_t align) override {
    std::size_t start = (off_ + align - 1) & ~(align - 1);   // round up: align is a power of 2
    if (start + bytes > cap_) throw std::bad_alloc();       // full: must throw, never return null
    off_ = start + bytes;
    return base_ + start;
}
void do_deallocate(void*, std::size_t, std::size_t) override {}  // freed when the region dies
bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
    return this == &other;  // memory from one region can only be freed by that same region
}
```

The rules come from the standard: `do_allocate` returns storage "aligned to the specified
alignment" and "throws an exception if storage of the requested size and alignment cannot be
obtained". Two resources compare equal only if "memory allocated from one can be deallocated
from the other and vice versa"; for a region, that is only itself. The constructor `mmap`s the
region and pre-faults it. `hft::region_resource` adds an overflow upstream, `owns(p)` and the
Linux options.
Figure: a region as one long bar; off_ moves right; each request first rounds off_ up to its alignment.
Source: https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate ; https://en.cppreference.com/w/cpp/memory/memory_resource/do_is_equal ; labs/l3_region.cpp run 2026-11-13

**L3C1 Elaborate:** Why would `return true;` in do_is_equal be a memory-corruption bug?

**L3C1 HandsOn:** Run `./labs/build/l3_region`. Then delete the alignment line (`start = off_`), rebuild with `bash labs/build.sh`, and run again.

### CONCEPT L3C2  First touch, pre-fault and mlock
`mmap(MAP_ANONYMOUS)` returns zero-filled address space. Physical pages appear on the first write
to each page, one fault per page: measured 4,096 faults and about 1 µs per page on pass 1
(1.0–1.1 µs in 5 runs; slower when the machine is busy), 0 faults on pass 2. The region pays that at construction by writing one byte per page. On Linux
you can also ask the kernel: `MAP_POPULATE` (mmap(2): the call "doesn't fail if the mapping cannot
be populated") or `madvise(MADV_POPULATE_WRITE)` ("since Linux 5.14", "populate (prefault) page
tables writable"). `mlock(2)` then keeps the pages "resident in RAM" so they cannot be paged out
later. It is limited by `RLIMIT_MEMLOCK` unless the process has `CAP_IPC_LOCK`. `mlock` of 64 MiB
returned 0 here on macOS.
Figure: pass 1: 4,096 page-fault arrows into the kernel; pass 2: none.
Source: https://man7.org/linux/man-pages/man2/mmap.2.html ; https://man7.org/linux/man-pages/man2/madvise.2.html ; https://man7.org/linux/man-pages/man2/mlock.2.html ; labs/l3_first_touch.cpp ; bench/results_r2/RESULTS.md (5 runs, 2026-11-14)

**L3C2 Elaborate:** Pre-faulting removed the faults. Name one cost it does NOT remove.

**L3C2 HandsOn:** Run `./labs/build/l3_first_touch` twice and record pass 1 and pass 2 ns per page.

### CONCEPT L3C3  Huge pages (Linux; NOT executed on the authoring machine)
Fewer, bigger pages mean fewer TLB misses and fewer faults. Two routes. **Explicit hugetlb:**
`mmap(..., MAP_HUGETLB)`, which "allocate[s] the mapping using huge pages" from a pool a root user
reserves via `/proc/sys/vm/nr_hugepages`. Check `HugePages_Total` / `HugePages_Free` in
`/proc/meminfo`. `MAP_HUGE_2MB` (= `21 << MAP_HUGE_SHIFT`) picks the size. **Transparent huge
pages:** `madvise(MADV_HUGEPAGE)` on a normal mapping. It needs `CONFIG_TRANSPARENT_HUGEPAGE`, and
memory "may only be replaced by huge pages on hugepage-aligned boundaries". If
`/sys/kernel/mm/transparent_hugepage/enabled` is `madvise`, THP is used only in regions marked this
way. Verify with the `AnonHugePages` fields of `/proc/PID/smaps`. `hft::region_resource` does the
madvise route by default (`arena_config::huge_pages`); `labs/l3_numa_linux.cpp` tries MAP_HUGETLB
and falls back. Page sizes: 16 KiB here; 4 KiB base with 2 MiB / 1 GiB huge pages on x86-64 Linux.
Figure: one 2 MiB huge page replacing 512 separate 4 KiB pages (one TLB entry instead of 512).
Source: https://man7.org/linux/man-pages/man2/mmap.2.html ; https://man7.org/linux/man-pages/man2/madvise.2.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html

**L3C3 Elaborate:** `l3_numa_linux.cpp` checks the result of the MAP_HUGETLB mmap and falls back, but calls madvise(MADV_HUGEPAGE) with no fallback. Why the difference?

### CONCEPT L3C4  NUMA: pin, bind, touch, verify (Linux; NOT executed on the authoring machine)
On a multi-socket box, memory on the other socket is slower to reach. The default policy is
"local allocation, that is, allocate memory on the node of the CPU that triggered the allocation".
So the order is: **pin** the thread (`pthread_setaffinity_np`, or `hft::pin_this_thread(cpu)`),
**mmap**, **mbind** the range to the node (`MPOL_BIND`, `<numaif.h>`, `-lnuma`; it "has an effect
only for new allocations"; the nodemask holds "up to maxnode bits", so size the mask and
`maxnode` together and never shift `1UL` by 64 or more), **touch** every page from that pinned thread, then run the hot path.
The libnuma shortcut is `numa_alloc_onnode(size, node)`, "rounded up to a multiple of the system
page size", "relatively slow compared to the malloc(3) family", "must be freed with numa_free()",
so use it once at start-up, never in the hot path. Verify three ways: `numastat -p <pid>`
("per-node memory allocation information"), `/proc/<pid>/numa_maps` (`N<node>=<pages>`), and
`move_pages(pid, n, pages, NULL, status, 0)`, which with `nodes == NULL` moves nothing and returns
"the node where each page currently resides". In `hft_mem`, NUMA binding is opt-in:
`-DHFT_MEM_NUMA=1 -lnuma` and `arena_config::numa_node`; `region().numa_bound()` tells you whether
mbind succeeded (in the Linux container it failed with EPERM, so check it). **One arena per
pinned thread; never share a pool across nodes.**
Figure: two sockets; the worker pinned on node 1 touches its region first, so the region's pages sit on node 1.
Source: https://man7.org/linux/man-pages/man2/set_mempolicy.2.html ; https://man7.org/linux/man-pages/man2/mbind.2.html ; https://man7.org/linux/man-pages/man3/numa.3.html ; https://man7.org/linux/man-pages/man2/move_pages.2.html ; https://man7.org/linux/man-pages/man7/numa.7.html ; https://man7.org/linux/man-pages/man8/numastat.8.html ; https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html

**L3C4 Elaborate:** Why must the arena be constructed ON the worker thread, after pinning, rather than by main() and passed in?

**L3C4 HandsOn:** Read `labs/l3_numa_linux.cpp` top to bottom and number its five steps. On a Linux NUMA box: `clang++ -std=c++23 -O2 labs/l3_numa_linux.cpp -lnuma -o numa && ./numa 0`.

### CONCEPT L3C5  Behind the scenes: thread_local arena + stateless allocator
`thread_arena`'s constructor stores `this` in a `static inline thread_local thread_arena* current_`.
`hft::allocator<T>` is an empty class (`sizeof` 1; containers using it stay at 24 B) that reads
that pointer on every allocate and calls the pool with a **qualified** call,
`unsynchronized_pool_resource::do_allocate(...)`, which is never virtually dispatched. Check it
yourself: `bash bench/asm_alloc.sh` disassembles one function per path. Here (Apple clang 21,
libc++, arm64) the stateful path is a load from the vtable and an indirect `br x3`; the stateless
path reads the thread_local through a descriptor call (`blr x8`) and then makes a direct `b` to
`std::__1::pmr::unsynchronized_pool_resource::do_allocate` (`std::__1` is libc++'s namespace;
libstdc++ and x86 will print different names and instructions). With no arena on the thread, allocate throws
`std::logic_error("hft_mem: no thread_arena on this thread")` instead of guessing. Measured per
allocate+free pair: stateful pmr 7 ns, stateless 8 ns, malloc 20 ns. You buy zero bytes per
container and no pointer to pass around. You pay by tying memory to the thread (lesson 04).
Figure: thread 1 → its thread_local pointer → its arena; thread 2 → its own. Same container type, two different regions.
Source: labs/hft_mem/hft_mem.hpp ; labs/l3_behind_the_scenes.cpp run 2026-11-13 ; bench/asm_alloc.sh + bench/asm_alloc_paths.cpp (output: _evidence/r2/asm-alloc.txt, 2026-11-14) ; bench/RESULTS.md

**L3C5 Elaborate:** The same `hft::vector<hft::string>` type, used in two threads, lands in two different regions. What makes that happen, and what does it imply if one thread hands its vector to the other?

**L3C5 HandsOn:** Run `./labs/build/l3_behind_the_scenes`.

---

## Completion cards

### 03-C1 [completion]
Fill in the body of `do_is_equal(const std::pmr::memory_resource& other) const noexcept` for a
   resource that owns its own region (one statement).

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 03-Q1 [apply]
Code (one line is fine). Write the body of `do_allocate(std::size_t bytes, std::size_t align)`
   for a bump region with members `std::byte* base_; std::size_t cap_; std::size_t off_;`. It must
   honour `align` and throw `std::bad_alloc` when the region is full. (Graded by compiling and
   testing alignment, overlap and exhaustion.)

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q3 [recall]
What does pre-faulting buy you, what are two Linux ways to ask the kernel to do it, and what
   does mlock add?

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q4 [apply]
Order these five start-up steps for a NUMA-local hot-path region (answer with the letters,
   e.g. "X Y Z ..."): P = pin the thread to its CPU, M = mmap the region, B = mbind the region to
   the node, T = touch (pre-fault) every page, H = start the hot path.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q2 [recall]
From memory: the three functions a custom memory_resource overrides, and the one rule each must
   keep.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q6 [apply]
Your team must choose between `std::pmr::map<K,V> m(arena.resource())` (stateful) and
   `hft::map<K,V>` (stateless) for 2,000 small per-symbol maps owned by one pinned thread. Using
   this lesson's measurements, give one argument for each side and say which you would pick.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q5 [recall]
Name three ways to verify on Linux which NUMA node your region's pages are on.

Confidence (mark before checking the key): sure / unsure / ___%

### 03-Q7 [apply]
Your Linux box shows `/sys/kernel/mm/transparent_hugepage/enabled` = `madvise` and
   `HugePages_Total: 0` in `/proc/meminfo`, and you have no root. Which huge-page route can your
   region use, which one cannot, and how do you check afterwards that it got huge pages?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 03-INT [synthesize]
Write the start-up sequence of one hot-path worker thread on a two-socket Linux box, from thread
   start to the first market-data message, naming which hft_mem call or Linux call does each step
   and why the order matters.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-04.md

# Lesson 04 — Traps, locality, and your own layer (~30 min)

"Behind the scenes" is convenient because you stop seeing the memory. It is also dangerous for
the same reason. Today you break the library on purpose, see where nodes actually land, and
then rebuild one layer yourself. Start by crashing something:

```
./labs/build/l4_traps outlive ; echo "exit=$?"
```

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 04-P1 [pretest]
Cold guess. An `hft::map` is created inside a block that also owns the `hft::thread_arena`,
   but the map is held in a `std::optional` declared OUTSIDE the block. The block ends (arena
   destroyed), then the map is destroyed. What happens?

Confidence (mark before checking the key): sure / unsure / ___%

### 04-P2 [pretest]
Cold guess. `sysctl hw.cachelinesize` on this Mac says 128. What does
   `std::hardware_destructive_interference_size` report with Apple clang 21 / libc++?

Confidence (mark before checking the key): sure / unsure / ___%

### 04-P3 [pretest]
Cold guess. `std::pmr::vector<long> a(&mono); std::pmr::vector<long> b(a);` Which resource
   does `b` use: `&mono` or something else?

Confidence (mark before checking the key): sure / unsure / ___%

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
Source: labs/l4_traps.cpp runs 2026-11-13, raw exit codes re-recorded in _evidence/r2/labs-raw-rc.txt (2026-11-14) ; labs/hft_mem/hft_mem.hpp

**L4C1 Elaborate:** Why does `thread` abort cleanly but `outlive` crashes without a message?

**L4C1 HandsOn:** Run `./labs/build/l4_traps static` and `./labs/build/l4_traps thread; echo $?`.

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
Source: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/multi_index_container.html ; labs/l4_traps.cpp runs 2026-11-13

**L4C2 Elaborate:** Why does `hft::allocator` not need uses-allocator construction for `hft::vector<hft::string>` to stay in the arena?

**L4C2 HandsOn:** Run `./labs/build/l4_traps pmr-nested`, then `pmr-move`.

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
Source: labs/l4_node_size.cpp, labs/l4_hot_cold.cpp runs 2026-11-13 ; bench/results_r2/RESULTS.md (l4_hot_cold, 5 runs, 2026-11-14) ; bench/RESULTS.md

**L4C3 Elaborate:** When does a per-thread shared pool beat a pool per container?

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
Source: https://en.cppreference.com/w/cpp/thread/hardware_destructive_interference_size ; bench/b3_locality.cpp ; bench/RESULTS.md

**L4C4 Elaborate:** Why is `alignas(64)` correct on your x86 server but not enough on this Mac?

### CONCEPT L4C5  Which allocator for which container
| container on the hot path | memory supply | why |
|---|---|---|
| node: `multi_index`, `map`, `unordered_map`, `unordered_node_map` | the **pool** (`hft::` alias) | one node per element; freed nodes are recycled |
| flat: `flat_map`, `vector`, `unordered_flat_map` | `hft::` alias + `reserve()` at warm-up (big block via the monotonic/region layer) | growth is a full copy; a pool cannot prevent it |
| small, hard upper bound | `static_vector<T,N>` / `small_vector<T,N>`, no allocator | no allocation at all |
| per-message scratch | `hft::scratch<N>` (monotonic) + `reset()` per message | everything dies together |
| shared across threads | not hft (single-thread); a `synchronized_pool_resource` or redesign to one owner | unsynchronized means one thread |
Figure: the table above.
Source: bench/RESULTS.md ; CONCEPT lesson-01.md#L1C3

**L4C5 Elaborate:** Why does the pool help an `unordered_node_map` (churn mean 81 → 37 ns here) but cannot help an `unordered_flat_map` that is never reserved?

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
Source: labs/l4_rebuild_freelist.cpp, labs/l4_capstone.cpp runs 2026-11-13 ; `tiny` runs with raw exit codes in _evidence/r2/labs-raw-rc.txt (2026-11-14)

**L4C6 Elaborate:** Why is the free list's `next` pointer stored inside the freed block rather than in a separate array?

**L4C6 HandsOn:** Run `./labs/build/l4_capstone tiny` and `./labs/build/l4_capstone_tripwire tiny; echo $?`.

---

## Completion cards

### 04-C1 [completion]
Fill the gap with one word: "In a function that owns both, declare the `hft::thread_arena`
   ______ the containers, so it is destroyed after them."

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 04-Q1 [apply]
Code. Given `struct Order { long id; long px; };` and an `Indices` type (hashed by id + ordered by
   px), write the type for an order store whose nodes come from the current thread's hft arena.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q2 [recall]
From memory: three lifetime or thread traps of a "behind the scenes" thread-local allocator,
   and what each one looks like when it fires in hft_mem.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q3 [apply]
Code. `std::pmr::vector<X> v(&mono);` holds 20 long strings. Type `X` so that the string bytes
   ALSO come from `mono` (graded by counting global operator new calls).

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q4 [recall]
From memory, fill the decision table: which memory supply for (a) a multi_index order store,
   (b) a flat_map of price levels, (c) a parser's temporaries for one message, (d) a list of at
   most 8 legs per order?

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q5 [apply]
Code (one line). Define `struct Counters { ... a; ... b; };` with two `std::atomic<long>`
   counters, each written by a different thread, that cannot false-share on the machine you are
   on. (Graded: they must be at least one real cache line apart here.)

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q6 [apply]
Code. In the free list (`struct node { node* next; }; node* head_;`), write the body of
   `do_deallocate(void* p, std::size_t bytes, std::size_t align)` for the fast path: the freed
   block must become the new head.

Confidence (mark before checking the key): sure / unsure / ___%

### 04-Q7 [apply]
Imagine a risk thread that keeps 5,000 tiny per-account `hft::map`s it rarely walks, and a
   matching thread that keeps one 1,000,000-order `hft::multi_index` it walks by price all day.
   Per-thread shared pool or one pool per container, for each thread? Why?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 04-INT [synthesize]
Review this design and name every problem: "main() creates one `hft::thread_arena`, builds a
   global `hft::multi_index` order store and a `std::pmr::vector<std::string>` of symbols on it,
   then starts two pinned worker threads that both insert into the order store."

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-05.md

# Lesson 05 — Membership is a pointer test: intrusive lists over a pool (~30 min)

Your order sits in several lists at once: its price level, its client, "to be sent to venue X",
"waiting for an ack". The hot-path questions are "is THIS order in the pending-send list?" and
"take it out of everything, now". Lessons 01–04 gave containers a pooled memory supply. This
lesson flips the design: the **pool owns the orders**, the order **carries the links**, and the
lists allocate nothing. Cancel an order in O(1) before you read anything (1 minute):

```
bash labs/build.sh                 # once (about 35 s here); skip if you already built
./labs/build/l5_cancel
```

Numbers below: Apple M1 Max, macOS, Apple clang 21, Boost 1.90, `-std=c++23 -O2` (benches add
`-DNDEBUG`), measured 2026-11-14 on a **shared** Mac with a Time Machine backup running; median of
5 runs, each row in its own process (`bench/results_intrusive/RESULTS.md`). The timer step is
about 41 ns, and an empty timed region already costs about 21 ns on average, so single-operation
rows near 0–42 ns are at the floor. Re-run on your x86 box (64 B lines): the mechanisms carry
over, the numbers do not.

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 05-P1 [pretest]
Cold guess. You hold an `Order&`. Rank these answers to "is it in the pending-send list?" from
   cheapest to dearest per query: (a) `pending_ids.contains(o.id)` on a `std::unordered_set<long>`,
   (b) `std::find` in a `std::list<Order*>` of 50,000, (c) `o.send_hook.is_linked()`,
   (d) `o.state == State::PendingSend`.

Confidence (mark before checking the key): sure / unsure / ___%

### 05-P2 [pretest]
Cold guess. Four per-venue send lists all link orders through the SAME member hook
   `send_hook`. For an order queued for venue 1, what does `o.send_hook.is_linked()` say when you
   ask "is it queued for venue 3?"

Confidence (mark before checking the key): sure / unsure / ___%

### 05-P3 [pretest]
Cold guess. With `link_mode<normal_link>` (the fastest mode), what does `hook.is_linked()` do?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L5C1  Use it: cancel in O(1) with no allocation
`labs/l5_cancel.cpp` is the whole idea in 60 lines. Storage: `hft::slab<Order> orders(N)`, carved
ONCE from the thread arena (`labs/hft_mem/hft_slab.hpp`, lesson 06 opens it). Each `Order`
carries two `boost::intrusive::list_member_hook<>`s, `level_hook` and `client_hook`, so one order
sits in its price-level FIFO and its client list at the same time. An id index
(`unordered_flat_map<long, Order*>`, reserved) finds it. **Cancel** = id lookup →
`level.erase(level.iterator_to(*o))` → the same for the client list → `ids.erase(id)` →
`orders.destroy(o)`. `iterator_to` builds an iterator FROM the object, in constant time, so no
list is searched. The run prints `in level list? 1 → 0`, `level 18 size=156 → 155`, and
`operator new calls during the cancel: 0`.
Figure: one Order box in the slab, two hook boxes inside it; arrows from the level list and from the client list point INTO the order's hooks; the id map points at the order.
Source: labs/l5_cancel.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/obtaining_iterators_from_values.html

**L5C1 Elaborate:** Why can `level.erase(level.iterator_to(*o))` be O(1) when `std::list::remove` is O(n)?

**L5C1 HandsOn:** Run `./labs/build/l5_cancel` and read the four lines.

### CONCEPT L5C2  Membership is a hook test: one hook per list, one source of truth
Give the order **one hook per list it can join** (level, client, active, pending-send,
pending-ack). Then "is it in pending-send?" is `o.send_hook.is_linked()`: it reads the order's own
cache line and nothing else.

**Speed of the yes/no question** (`bench/b4_membership`, 100,000 orders, half pending, p50 per
batch of 64 random orders):
- enum compare 83 ns, bitmask 84, **hook 125**;
- `multi_index` find(id)→state 584, `unordered_set<id>` 750, `flat_set<id>` 3,541;
- one `std::find` in a 50,000-entry `std::list<Order*>`: 66,083 ns.

So the hook is **not** faster than a flag (why it is slower, instruction count or where in the
object it sits, is **not isolated**). *Checkpoint: if not speed, what could a hook win on?*

**It wins on two other things.**
1. **It cannot disagree with the list.** A hand-kept enum can: `l5_membership` section 3 queues
   an order and forgets the enum, and prints `state says PendingSend? 0  send hook says linked? 1`.
2. **Moving between states is O(1) and allocates nothing**: send → ack (unlink + push_back, batch
   of 16) p50 42 ns, against 459 ns for erase + insert in two `unordered_set`s (a node allocation
   per insert) and 33,833 ns for one find + erase + push in `std::list<Order*>`.
Figure: the order's bytes: [payload 32 B][level 16][client 16][active 16][send 16][ack 16] = 112 B; is_linked reads one 8-byte field inside it.
Source: bench/b4_membership.cpp ; bench/results_intrusive/RESULTS.md ; labs/l5_membership.cpp run 2026-11-14 ; _evidence/r3/b4-offsets.txt

**L5C2 Elaborate:** The enum is as fast as the hook for the yes/no question. Why keep the hook?

**L5C2 HandsOn:** Run `./labs/build/l5_membership` and find the line where the two sources of truth disagree.

### CONCEPT L5C3  Which link modes make is_linked() trustworthy
Boost.Intrusive has three link modes; the default is **safe_link**. `is_linked()` has the
precondition "link_mode must be safe_link or auto_unlink" and returns "true, if the node belongs to
a container" (list_hook.hpp). **safe_link**: the hook starts unlinked, and erase "puts the erased
object in the well-known default state" (null links), so `is_linked()` goes 1 → 0 (`l5_link_modes`).
In builds without `NDEBUG` it also asserts on misuse (lesson 06). **normal_link**: nothing resets
the hook, `is_linked()` is a compile error (pretest 05-P3); a list destroyed while holding objects
leaves their hooks stale. **auto_unlink**: as safe_link, plus the hook's destructor unlinks itself
and `hook.unlink()` needs no list. It is a compile error with the default
`constant_time_size<true>` ("static assertion", docs and `-DBREAK_AUTO_UNLINK_CTS`), because the
list cannot keep a count while elements leave on their own; `size()` then walks the list.
Figure: three columns: safe_link (reset on erase, is_linked ok, asserts in debug), normal_link (no reset, no is_linked), auto_unlink (reset + self-unlink, no constant-time size).
Source: /opt/homebrew/include/boost/intrusive/list_hook.hpp lines 144–151 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/auto_unlink_hooks.html ; labs/l5_link_modes.cpp run 2026-11-14

**L5C3 Elaborate:** In one sentence, what does `is_linked() == true` tell you, and what does it NOT?

**L5C3 HandsOn:** Compile `clang++ -std=c++23 -I/opt/homebrew/include -DBREAK_AUTO_UNLINK_CTS labs/l5_link_modes.cpp` and read the error.

### CONCEPT L5C4  The Venue X trap, and the hook as a state machine
One hook shared by N per-venue send lists answers "in SOME venue list". Asked "queued for venue
3?" about an order queued for venue 1, `is_linked()` says 1 (`l5_membership` section 4). Three
fixes, with their costs: **(a)** store the venue in the order (1 byte) and test
`is_linked() && o.venue == X`: O(1), but now two fields must agree; **(b)** one hook per venue:
exact, 16 B × N per order (4 venues = 64 B more); **(c)** walk from the node to the list header
and compare with each list's `end()`: no extra bytes, O(position): 100,001 steps took 205–280 µs
over 5 runs (one more run, on a busy machine, took 1,258 µs).
Sharing a hook is also a **design tool**: send and ack through ONE hook means an order can never
be in both at once (inserting a linked safe_link hook asserts: `l5_membership twice`, exit 134;
the `-DNDEBUG` build `l5_membership_ndebug twice` printed `survived the second push_back:
active.size()=2`, then crashed in `active.clear()`, exit 139). Separate hooks allow both at once. A status enum allows anything. The
**bitmask** (1 bit per list, `member_bits & 1`, 84 ns per 64) is the cheapest yes/no, but gives no
next/prev and no O(1) unlink.
Figure: four venue lists threading through the same send_hook slot of different orders; the order shows "linked" but not which thread.
Source: labs/l5_membership.cpp runs 2026-11-14 (asserts on and -DNDEBUG: _evidence/r3/labs-output-l5-l7.txt, walk: _evidence/r3/lab-repeats.txt) ; /opt/homebrew/include/boost/intrusive/detail/list_iterator.hpp line 118

**L5C4 Elaborate:** Why does sharing ONE hook between the send and ack lists make an illegal state unrepresentable?

**L5C4 HandsOn:** Run `./labs/build/l5_membership twice; echo $?`.

### CONCEPT L5C5  Three models, counted
Same workload in `labs/l5_models` (10,000 inserts, 1,000 cancels). Columns: `operator new` per
insert / `operator delete` per cancel / bytes per order including setup and storage.
**VALUE** (container owns the Order): `std::map<id,Order>` 1 / 1 / 72 B; `std::list<Order>` per
level 1 / 1 / 48 B; reserved `flat_map<id,Order>` 0 / 0 / 40 B; `multi_index` (hashed id +
ordered px: ONE node with two index headers) 1 / 1 / 82 B. A value container gives one structure;
a second structure must hold a key or pointer. **POINTER** (`list<Order*>` + `unordered_map<id,it>`):
2 / 2 / 109 B, and every "next" is two dependent loads (node → Order). **INTRUSIVE** (slab +
2 hook lists + flat id map): 0 / 0 / 94 B, all allocated once at start; "next" is one load,
because the link sits inside the order. The cost is bytes **in the object**: 16 B per list it can
join. 5 hooks make a 32 B order 112 B: 1.14 orders per 128 B line instead of 4. Scanning 1,024
consecutive orders: 875 ns hooked vs 375 ns lean (p50).
Figure: three rows of boxes: VALUE (node contains order), POINTER (node → separate order), INTRUSIVE (order contains the links, containers are just headers).
Source: labs/l5_models.cpp run 2026-11-14 ; bench/b4_membership.cpp scan rows ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html

**L5C5 Elaborate:** Why does the pointer model pay TWO dependent loads per "next" and the intrusive model one?

**L5C5 HandsOn:** Run `./labs/build/l5_models` and read the new/ins and B/total columns.

### CONCEPT L5C6  Exactly what is O(1), and what is not
**O(1) given the object:** `iterator_to` / static `s_iterator_to` (list, slist, set, multiset;
"Constant time" in list.hpp), `erase` from a list, next/prev, `is_linked()`, slab release.

**Not O(1):**
- lookup **by key**: `set::find` O(log n), 84 ns p50 for 50,000 ids in `bench/b6_handroll`;
  `unordered_set::find` average O(1), 41 ns, and only as good as the bucket array you supply
  (lesson 06);
- `unordered_set::erase(it)` too is "Average case O(1), worst case O(this->size())": its buckets
  are singly linked, so erase must find the predecessor in the chain;
- `slist::erase(it)` is "Linear to the elements before i" (91 ns mean vs 19 for `list` in b6);
- `size()` is O(n) with `constant_time_size<false>`; "which list am I in?" with a shared hook is
  O(position) (L5C4); destroying a safe_link list is O(n) (it unlinks every element).

*Checkpoint: cover the list above and name two things that are not O(1).*

**One catch:** erase on a Boost `list` needs the **list object** (to keep its size counter), so
you must know which list the order is in (`o.px` tells you the level). "Intrusive = O(1)
everything" is false: intrusive makes *membership operations given the object* O(1); keyed
lookup still needs an index.
Figure: two columns: given the object (O(1)) | given a key or a position (log n, average 1, or n).
Source: /opt/homebrew/include/boost/intrusive/list.hpp lines 249–257, 1301–1309 ; /opt/homebrew/include/boost/intrusive/slist.hpp line 987 ; /opt/homebrew/include/boost/intrusive/hashtable.hpp line 2898 ; bench/results_intrusive/RESULTS.md ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html

**L5C6 Elaborate:** Why can a hand-rolled circular list unlink with only the node, while `bi::list::erase` needs the list?

---

## Completion cards

### 05-C1 [completion]
Fill the gap with one link mode: "`is_linked()` compiles with `auto_unlink` or with ______,
   which is also the default."

Confidence (mark before checking the key): sure / unsure / ___%

### 05-C2 [completion]
Complete `cancel(Book& b, long id)`. `Book` holds `hft::slab<Order> orders`, `Level levels[64]`,
   `Client clients[8]` (the two intrusive list types) and `ids` (id → `Order*`); the order has
   `px` and `client`. Write the ONE missing statement:
   ```
   Order* o = b.ids.find(id)->second;
   b.levels[o->px].erase(Level::s_iterator_to(*o));
   ______                                    // take it out of its client list, O(1)
   b.ids.erase(id);
   b.orders.destroy(o);
   ```
   (Graded by compiling the completed body with asserts on.)

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 05-Q1 [apply]
Code. `struct Order { long id; State state; bi::list_member_hook<> send_hook; bi::list_member_hook<> ack_hook; };`
   Write the body of `bool in_pending_send(const Order& o)`. Warning: some code paths forget
   to update `state`. (Graded by compiling and running it.)

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q2 [apply]
Code. `struct Book { hft::slab<Order> orders; Level levels[64]; Client clients[8];
   boost::unordered_flat_map<long, Order*> ids; };` with `Order { long id; int px; int client;
   long qty; level_hook; client_hook; }`. Write the body of `void cancel(Book& b, long id)`:
   out of every structure, slot back to the slab, nothing allocated, O(1): no loop, no search.
   (Compiled with asserts on; levels hold several orders.)

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q3 [recall]
From memory: name three operations an intrusive design makes O(1) given the object, and three
   that stay O(log n), average O(1) or O(n).

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q4 [apply]
Code. All per-venue send lists share `send_hook`; the order has `std::uint8_t venue`. Write the
   body of `bool queued_for(const Order& o, int v)`.

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q5 [recall]
From memory: per insert, how many heap allocations do (a) `multi_index` with two indices,
   (b) `std::list<Order*>` + `unordered_map<id, iterator>`, (c) intrusive lists over a slab make,
   and what does (c) pay instead?

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q6 [apply]
Transfer. A risk engine keeps 200,000 accounts. Two questions per tick: (1) "is account A in
   the breached set?" (yes/no, millions of times), (2) every second, walk all breached accounts
   and clear the ones back under limit. Hook, bitmask, or both? Why?

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q7 [apply]
Transfer. A market-data fan-out has 64 gateways; each symbol may be subscribed by any subset.
   Someone proposes "one intrusive list hook per gateway inside each Symbol". Cost it, and propose
   the cheaper membership answer.

Confidence (mark before checking the key): sure / unsure / ___%

### 05-Q8 [recall]
From memory: which link modes make `is_linked()` usable, what does `true` tell you, and which
   question about the order does it leave open?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 05-INT [synthesize]
An order can be in: its price level, its client list, the pending-send list of exactly one
   venue (of 4), and the pending-ack list. Design the Order's hooks and fields so that every
   membership question is O(1) and correct, "pending-send and pending-ack at once" is impossible,
   and say what it costs per order in bytes.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-06.md

# Lesson 06 — Inside the hooks and the pool: building it right (~30 min)

Lesson 05 used intrusive lists over `hft::slab`. Today you open both: what a slab is, what each
hook costs here, how the hashed and tree containers want to be fed, and five ways to corrupt the
whole thing, each one broken on purpose and fixed. Start by breaking it (1 minute):

```
./labs/build/l6_traps linked-destroy ; echo "exit=$?"
./labs/build/l6_traps_ndebug linked-destroy ; echo "exit=$?"
```

Same machine and caveats as lesson 05 (M1 Max, Apple clang 21, Boost 1.90, shared Mac, median of
5 runs in `bench/results_intrusive/RESULTS.md`, timer floor about 21 ns mean).

## Pretest — commit your guesses FIRST (no fair reading ahead)

### 06-P1 [pretest]
Cold guess. On this 64-bit machine, how many bytes is `bi::set_member_hook<>` (red-black tree
   hook), and with `optimize_size<true>`?

Confidence (mark before checking the key): sure / unsure / ___%

### 06-P2 [pretest]
Cold guess. A Boost.Intrusive `unordered_set` gets a 64-bucket array, then 50,000 orders are
   inserted. What happens to the bucket count, and to `find(id)`?

Confidence (mark before checking the key): sure / unsure / ___%

### 06-P3 [pretest]
Cold guess. You `slab.destroy()` an order that is still in a price level, then the next
   `create()` reuses its slot. What happens in a debug build, and in a `-DNDEBUG` build?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Concept cards

### CONCEPT L6C1  The pool: hft::slab<T>
`labs/hft_mem/hft_slab.hpp` (205 lines, added to the library). Construction carves ONE block,
`capacity` slots of `sizeof(T)` bytes then a 4-byte generation per slot, from the current thread's
arena (bulk layer → monotonic → pre-faulted region; no `operator new`, `l6_slab` prints 0 calls).
A **free list is threaded through the free slots** (a free slot's first bytes hold `next`), so
`create()` is a pop and `destroy()` a push: O(1), no search, no lock, single thread. It is
threaded front to back, so the first objects are adjacent (`b - a = 64 B`). **LIFO**: `destroy(a)`
then `create()` returns a's slot. **Exhaustion** never falls back to malloc: `try_create()`
returns nullptr (reject the order), `create()` throws `std::bad_alloc`; a throwing constructor
leaves the slot free. The generation is bumped by `create()` AND `destroy()` (odd = live), so a
checked build aborts on a double `destroy()` and no handle ever matches a free slot. The slab must
die before its arena and aborts (checked builds) if destroyed with live objects. LIFO vs a FIFO
ring on a 64 MB slab half full, 6 interleaved runs: 12.2–12.8 vs 11.7–12.2 ns per
destroy+create+write. Isolated in the same runs: the same churn without create's generation store
took 10.4–10.6 ns (the pre-fix slab 10.3–10.5), so that one store costs about 1.7 ns here, the
price of the double-destroy check. The FIFO ring does none of the slab's bookkeeping, so LIFO vs
FIFO itself is still **not isolated**.
Figure: slot array [A][free][B][free][free]; head → free → free → null threaded through the free slots; generations below each slot.
Source: labs/hft_mem/hft_slab.hpp ; labs/l6_slab.cpp runs 2026-11-14 (_evidence/r3/lab-repeats.txt) ; tests/run.sh modes slab-*

**L6C1 Elaborate:** Why does the slab need no memory of its own for the free list?

**L6C1 HandsOn:** Run `./labs/build/l6_slab`.

### CONCEPT L6C2  Hooks: base or member, tags, and their bytes here
Four chunks; pause after each.

**1. Two ways to attach a hook.**
- **Base hook**: `struct Order : bi::list_base_hook<bi::tag<by_level>>, bi::list_base_hook<bi::tag<by_client>>`;
  two base hooks of one kind "must" have different tags; the list names the hook with
  `bi::base_hook<...>`.
- **Member hook**: `bi::list_member_hook<> level;` and the list names it with
  `bi::member_hook<Order, Hook, &Order::level>`: no inheritance, no tags. This course uses member hooks.

*Checkpoint: which of the two needs tags, and when?*

**2. The rule both share.** **The type must name every container it can join** (coupling), and
an object cannot be in one container twice through one hook.

**3. Bytes per hook here** (`l6_hooks`), smallest first:
- slist 8, unordered_set 8 (16 with `store_hash` or `optimize_multikey`).
- list 16.
- set 32 (24 with `optimize_size<true>`), avl_set 32 (24), bs_set 24 (splay / scapegoat / treap
  trees default to it).

*Checkpoint: cover the list and say the list, set and unordered_set hook sizes.*

**4. Bytes per container header:** list 24 B (16 with `constant_time_size<false>`), slist 16 (24
with `cache_last<true>`, needed for push_back), set 40. And the trees Boost offers that you would
not want to write: rbtree (set), AVL, splay, scapegoat, treap.
Figure: Order layout with hooks laid out after the payload; a list header pointing into the hook, not into the object start.
Source: labs/l6_hooks.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/set_multiset.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html ; /opt/homebrew/include/boost/intrusive/bstree.hpp line 66

**L6C2 Elaborate:** Base or member hook: which keeps your Order an aggregate-free plain struct, and why might you still pick base?

**L6C2 HandsOn:** Run `./labs/build/l6_hooks` and write down the list, set and unordered_set hook sizes.

### CONCEPT L6C3  Lookup by key: buckets you own, trees, and the id map next door
An intrusive `unordered_set` takes **your bucket array** (`bucket_traits(ptr, n)`); "the contents
... are not rehashed to maintain a load factor"; rehash happens only when you call
`rehash(new_traits)`, and "the bucket array must be destroyed after the container". With
`power_2_buckets<true>` it masks instead of dividing (you guarantee the power of two). A too-small
array never throws and never allocates: it just gets slow (mean find 2,696–2,743 ns vs 1.5–1.6 ns,
64 vs 65,536 buckets, dense ids, 6 runs). `key_of_value<id_of>` lets you call `find(42)` with a
plain id; for a different key type pass `find(key, hasher, equal)`. A `set` by id is O(log n):
57–62 ns mean here (56.6–61.5 over the same 6 runs).
The **id → Order*** index is often a `boost::unordered_flat_map`. Trap, isolated in
`l6_flat_drift`: reserved for exactly the 50,000 live ids, steady erase+insert at constant size
still allocated a new 1 MiB table 9 times in 200,000 operations (bucket count unchanged), because
`max_load` "may decrease on erasure under high-load conditions". p99.9 stayed 125 ns, **max was
531 µs**. Reserved 2× (100,000): 0 allocations, max 1,750 ns. `hft_book.hpp` first reserved 1×
capacity and hit exactly this: filled to capacity under `seal()`, churn threw `bad_alloc` at op
21,508; it now reserves 2× and the same run makes 200,000 cancel+add with the bucket count
unchanged (`bash tests/run.sh`, mode `book-churn-full`).
Figure: bucket array (yours, fixed) → chains threaded through each order's 8-byte hook.
Source: labs/l6_buckets.cpp, labs/l6_flat_drift.cpp runs 2026-11-14 (_evidence/r3/lab-repeats.txt) ; _evidence/r3/book-churn.txt ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/unordered_set_unordered_multiset.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/map_multimap.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/advanced_lookups_insertions.html ; https://www.boost.org/doc/libs/1_90_0/libs/unordered/doc/html/unordered/reference/unordered_flat_map.html ; bench/results_intrusive/RESULTS.md

**L6C3 Elaborate:** Why can't an intrusive unordered_set grow on its own?

**L6C3 HandsOn:** Run `./labs/build/l6_flat_drift`.

### CONCEPT L6C4  Disposers and who owns what
The container **never owns** anything: `erase` only unlinks. To unlink AND release, pass a
disposer: `ids.erase_and_dispose(it, [&](Order* o){ pool.destroy(o); })`,
`lvl.clear_and_dispose(...)`, `lvl.remove_and_dispose_if(pred, disposer)`; the disposer runs
after the element is removed. Teardown order: the slab (owner) must be declared **before** the
containers that link its objects, so it dies after them; unlink everything, then destroy. A
safe_link (or auto_unlink) list's destructor unlinks every element, "Linear to the number of
elements": 1,426–1,458 µs for 1,000,000 orders here (12 runs, both builds); normal_link: 0.0–1.7
µs, but every hook is left holding links into a dead list. Boost containers are move-only
(`BOOST_MOVABLE_BUT_NOT_COPYABLE`, list.hpp line 1472), and objects need no copy constructor to
be stored; copying a hooked object copies the payload, not the membership (the hook's copy
constructor leaves the copy unlinked; `hr::dlink`/`hr::hlink` now do the same).
Figure: slab (owns) ← orders ← hooks ← lists (link only); arrows for destruction order: lists first, slab last.
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/erasing_and_disposing.html ; /opt/homebrew/include/boost/intrusive/list.hpp lines 249–257, 1472 ; _evidence/r3/lab-repeats.txt ; _evidence/r3/labs-output-l5-l7.txt ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/intrusive_vs_nontrusive.html ; labs/l6_traps.cpp run 2026-11-14

**L6C4 Elaborate:** Why must the disposer, not the list, call `pool.destroy`?

**L6C4 HandsOn:** Run `./labs/build/l6_traps container-dies`.

### CONCEPT L6C5  Five traps, broken and fixed (labs/l6_traps)
| trap | debug build (asserts) | `-DNDEBUG` build | fix |
|---|---|---|---|
| `linked-destroy`: destroy while linked | assert in `~hook`, exit 134 | slot reused, crash later, exit 139 | unlink first (`erase_and_dispose`) |
| `order-dies-first`: object declared after its list | assert, exit 134 | exit 0 (silent; the list's destructor then clears a dead object's hook, list.hpp 251–256) | declare the owner first |
| `autounlink`: auto_unlink surprises | copy is NOT linked; a temporary left the list "by itself" (size 2 → 1) | same | prefer safe_link; never link temporaries |
| `aba`: raw pointer after reuse | old pointer now reads id=8 (another order) | same | keep a `slab::handle` {index, generation}; `get()` → nullptr |
| `iterate`: erase inside range-for | segfault, exit 139 | segfault, exit 139 | `it = l.erase(it)` or `remove_and_dispose_if` |

Note the release column: **safe_link's checks are `BOOST_ASSERT`, which compiles to nothing with
`NDEBUG`**; in release, safe_link still resets hooks on erase (so `is_linked()` works) but no
longer catches misuse. Test with asserts on. Erase-inside-range-for crashes because erase resets
the hook to null links (safe mode, `circular_list_algorithms::init`) and the loop's `++` then
follows a null `next`.
Figure: the table above.
Source: labs/l6_traps.cpp runs 2026-11-14 (both builds) ; /opt/homebrew/include/boost/intrusive/detail/generic_hook.hpp lines 43–48 ; /opt/homebrew/include/boost/assert.hpp ; /opt/homebrew/include/boost/intrusive/circular_list_algorithms.hpp lines 70–75 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html

**L6C5 Elaborate:** Why does the generation in the handle catch the ABA case when the pointer cannot?

**L6C5 HandsOn:** Run `./labs/build/l6_traps aba` and `./labs/build/l6_traps iterate-fixed`.

### CONCEPT L6C6  Index links: 32-bit slot numbers instead of pointers
If every object lives in ONE slab, a link can be a 32-bit **slot number**: `struct ilink {
uint32_t prev, next; }`, 8 B instead of 16 (`labs/hft_mem/hft_handroll.hpp`, `hr::idx_list`).
With three hooks an order shrinks from 80 B to 56 B: 2.29 orders per 128 B line instead of 1.60.
It is **relocatable**: memcpy the slab elsewhere (snapshot, shared memory, file) and the links
still hold (`l6_index_list`); a pointer-linked copy still points into the old array. Boost's own
relocatable option is `void_pointer<offset_ptr<void>>` hooks (also intact after memcpy here, but
16 B). The price: slot × stride arithmetic on every step and explicit nil checks. Generated code:
`idx_erase` 22 instructions, 5 branches vs `hand_erase` 5 / 1 (`bench/results_intrusive/asm.md`);
walking a list of about 49 was 209 vs 167 ns p50 (`b6_handroll`, every run). The 4 B id fits the
slab, not the process: at most 2^32 − 2 slots.
Figure: slab with slots 0..7; links drawn as small integers (1 → 3 → nil) instead of arrows.
Source: labs/l6_index_list.cpp run 2026-11-14 ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html ; bench/results_intrusive/asm.md ; bench/results_intrusive/RESULTS.md

**L6C6 Elaborate:** Why does relocation break a pointer list but not an index list?

**L6C6 HandsOn:** Run `./labs/build/l6_index_list`.

---

## Completion cards

### 06-C1 [completion]
Fill the gap with one word: "Call `slab.destroy(o)` only after `o` is ______ from every
   container it is in."

Confidence (mark before checking the key): sure / unsure / ___%

### 06-C2 [completion]
Complete the index-linked list's `void erase(std::uint32_t i)` (members `T* base_;
   std::uint32_t head_, tail_;`, link of slot i is `base_[i].*M`, `nil` = no neighbour). The
   line for the PREV neighbour is given; write the ONE missing line for the NEXT neighbour:
   ```
   ilink& l = base_[i].*M;
   if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;
   ______
   l.prev = l.next = nil;
   ```

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 06-Q1 [recall]
From memory: bytes per hook here for list, slist, set (default and optimize_size) and
   unordered_set, and the one rule for two BASE hooks of the same kind.

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q6 [apply]
Code. Write the body of the index-linked list's `void erase(std::uint32_t i)`. Members:
   `T* base_; std::uint32_t head_, tail_;`, slot i's link is `base_[i].*M` with `.prev/.next`,
   `nil` = no neighbour; leave slot i's links nil.

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q2 [recall]
From memory: three rules for the bucket array of an intrusive `unordered_set`, and what a
   too-small array does to latency.

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q5 [apply]
Code. A cancel request carries `hft::slab<Order>::handle h`, taken when the order was created.
   Write the statement(s) for `cancel_by_handle(pool, h)` that cancel it only if that same order
   is still alive (its slot may now hold another order).

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q3 [apply]
Code (one statement). Empty the price level `lvl` AND hand every order back to `pool`
   (`hft::slab<Order>`).

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q7 [apply]
Transfer. Your matching engine must publish a consistent snapshot of the whole book to a risk
   process through shared memory every 100 ms, by copying the order slab. Pointer hooks, offset_ptr
   hooks, or index links? Name the cost of your choice.

Confidence (mark before checking the key): sure / unsure / ___%

### 06-Q4 [recall]
From memory: the five traps of intrusive-over-a-pool, and which ones safe_link catches in a
   debug build.

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 06-INT [synthesize]
Review: "Orders come from `hft::slab<Order>`; levels are `bi::list` with normal_link for speed;
   cancel does `slab.destroy(o)` then `level.erase(level.iterator_to(*o))`; the id index is a
   `boost::unordered_flat_map` reserved for the expected peak of live orders; clients keep raw
   `Order*` to cancel later." Name every problem and its fix.

Confidence (mark before checking the key): sure / unsure / ___%

## lesson-07.md

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

### 07-P1 [pretest]
Cold guess. Same order-book workload. Mean cost of one ADD: intrusive lists over a slab vs
   `boost::multi_index` (hashed id + ordered price) on the pooled `hft::allocator`. Ratio?

Confidence (mark before checking the key): sure / unsure / ___%

### 07-P2 [pretest]
Cold guess. 50,000 live 64 B objects; destroy a random one, create one, 400,000 times.
   `boost::object_pool` vs a plain free list: what is object_pool's p99?

Confidence (mark before checking the key): sure / unsure / ___%

### 07-P3 [pretest]
Cold guess. Static instruction count of `erase` for a Boost `list` (default options) and for a
   hand-rolled circular doubly linked list, at -O2 -DNDEBUG?

Confidence (mark before checking the key): sure / unsure / ___%

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
Source: labs/l7_order_book.cpp run 2026-11-14 ; bench/b5_book.cpp ; bench/results_intrusive_r2/RESULTS.md ; _evidence/r3/b5-A-isolation.txt ; https://www.boost.org/doc/libs/1_90_0/libs/multi_index/doc/reference/ord_indices.html

**L7C1 Elaborate:** Why did the pooled allocator barely change multi_index's add time?

**L7C1 HandsOn:** Run `./labs/build/l7_order_book`.

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
Source: labs/l7_patterns.cpp run 2026-11-14

**L7C2 Elaborate:** Why does the timer cancel need no search even though the wheel has 64 buckets?

**L7C2 HandsOn:** Run `./labs/build/l7_patterns`.

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
Source: bench/b6_handroll.cpp ; bench/results_intrusive/RESULTS.md ; bench/results_intrusive_r2/RESULTS.md ; bench/results_intrusive/floor.txt ; bench/results_intrusive/asm.md ; _evidence/r3/lab-repeats.txt (l7_ordered_free x10) ; /opt/homebrew/include/boost/pool/pool.hpp line 480 ; /opt/homebrew/include/boost/intrusive/hashtable.hpp line 2898

**L7C3 Elaborate:** The lists' timings are all the same here but the instruction counts are not. What does that tell you to do?

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
Source: bench/results_intrusive_r2/compile_time.txt ; /opt/homebrew/include/boost/intrusive/options.hpp ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/safe_hook.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/node_algorithms.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/using_smart_pointers.html ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/presenting_containers.html

**L7C4 Elaborate:** Name one thing you must test yourself if you hand-roll that Boost's safe mode would have caught.

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
Source: bench/results_intrusive/RESULTS.md ; labs/l5_models.cpp ; https://www.boost.org/doc/libs/1_90_0/doc/html/intrusive/usage_when.html ; CONCEPT L7C3 ; CONCEPT L7C4

**L7C5 Elaborate:** Give the single strongest reason to pick multi_index over intrusive + slab for an order store.

---

## Completion cards

### 07-C1 [completion]
Fill the gap with one word: "`boost::object_pool::destroy` calls ______ `free`, which walks the
   free list and can cost O(N)."

Confidence (mark before checking the key): sure / unsure / ___%

### 07-C2 [completion]
Complete `void mark_sent(Order& o)`: the order leaves `send` (a `SendQueue` through
   `send_hook`) and joins `ack` (an `AckQueue` through `ack_hook`). Write the ONE missing
   statement, O(1), no search:
   ```
   ______                // leave the pending-send queue
   ack.push_back(o);
   ```

Confidence (mark before checking the key): sure / unsure / ___%

---

## Question cards

### 07-Q1 [recall]
From memory: the steps of an O(1) cancel in the intrusive book, and what each design (intrusive,
   multi_index, pointer model) allocates per ADD.

Confidence (mark before checking the key): sure / unsure / ___%

### 07-Q3 [recall]
From memory: is Boost.Intrusive good enough here? Name what to take from Boost, what to
   hand-roll, and the one measured result that argues against `boost::object_pool`.

Confidence (mark before checking the key): sure / unsure / ___%

### 07-Q2 [apply]
Code. Write the body of `void mark_sent(Order& o)`: the order leaves `send` (a `SendQueue`
   through `send_hook`) and joins `ack` (an `AckQueue` through `ack_hook`), O(1), no allocation.

Confidence (mark before checking the key): sure / unsure / ___%

### 07-Q4 [apply]
Transfer. A quote cache keeps, per symbol, at most 20 quotes sorted by price, read by
   scanning; quotes are never in any other structure, and updates replace a quote by price. Intrusive
   + slab, multi_index, or a flat container? Why?

Confidence (mark before checking the key): sure / unsure / ___%

### 07-Q5 [apply]
Transfer. A risk check must expire 10,000 per-order "throttle" timers at 1 ms granularity, and
   90% are cancelled before they fire. Sketch the structure and the cost of schedule, cancel and
   one tick.

Confidence (mark before checking the key): sure / unsure / ___%

### 07-Q6 [apply]
Transfer. A colleague wants to hand-roll every list "because Boost's erase is 8 instructions and
   ours is 5". What do you check first, and what do you suggest instead?

Confidence (mark before checking the key): sure / unsure / ___%

---

## Integrate (day close — one synthesis question)

### 07-INT [synthesize]
Write the one-page design note for a new order gateway: storage, hooks and lists, id index,
   state machine, pool, what Boost provides and what you hand-roll, and two risks with their
   mitigations. Use numbers from this machine only as ratios.

Confidence (mark before checking the key): sure / unsure / ___%

# Answer keys
Compare only after writing an attempt and marking its confidence.
Sure (or >= 70%) and missed? Star it: re-test it first tomorrow.

## 01-P1
- About 1 in 64 (≈1.5%). 16384 / 256 = 64, so every 64th object lands on a page nobody has
    touched yet, and that first write takes a page fault. Measured: 3,138 page faults inside the
    loop and 3,126–3,128 samples ≥ 0.5 µs, in each of 15 runs (about two thirds of them ≥ 1 µs).
    If you guessed "none, malloc is fast", that surprise is the lesson: the cost is the kernel's,
    not malloc's. CONCEPT L1C1 explains it.

## 01-P2
- Before: 100,000 (one node per insert). After: 0. The node is taken from a pool that the freed
    node was just returned to. CONCEPT L1C2.

## 01-P3
- It throws `std::bad_alloc`, on purpose: the sealed arena refuses any request for new memory.
    In a test, that turns a hidden hot-path allocation into a loud failure. CONCEPT L1C4.

## L1C1 Elaborate
- Each 16 KiB page holds 64 objects of 256 B. The first write to a new page takes a page fault,
    so one allocation in 64 pays for it.

## L1C1 HandsOn
- About 3,100 faults (≈ 51 MB / 16 KiB) and a p99 near 1 µs; the ARENA run shows 0 faults and a
    p99 near 42 ns (one timer step). Your exact numbers will differ.

## L1C2 Elaborate
- Each insert follows an erase of the same node size. The erase returns the node to the pool's
    free list and the insert takes it back, so no new memory is ever requested.

## L1C2 HandsOn
- First run prints "hot-path operator new calls: 100000"; after the edit it prints 0.
- `l1_every_alias` prints one row per alias, each "in region? yes  operator new in sealed
    churn: 0", and exits 0.

## L1C3 Elaborate
- Almost every insert is cheap. A handful of inserts trigger a rehash that allocates a bigger
    table and moves every element; those few are huge, and averaging hides them.

## L1C4 Elaborate
- It fails at the exact line that allocated, with a stack you can inspect, instead of a number
    at the end that says "something, somewhere, allocated".

## L1C4 HandsOn
- The first prints "clean: 300 levels, 1000 orders, ..." and exits 0. The second prints
    "TRIPWIRE: the hot path asked for new memory" and exits 3.

## 01-C1
- `arena.seal();`

## 01-C2
- `reserve` (pre-size it to the most elements it will ever hold).

## 01-Q1
- First-touch page faults on fresh memory (the one the lab shows: ~3,100 faults, one per 64
    allocations, and a p99 about 20× the p50).
- Locks / arenas: threads synchronise inside a shared allocator.
- Scattered nodes: nodes spread over the heap cost cache misses when you walk them.

## 01-Q2
- `hft::map<long, Order>` (spelling out `std::map<long, Order, std::less<long>,
    hft::allocator<std::pair<const long, Order>>>` also passes). `hft::flat_map` makes zero
    `operator new` calls here too, but erasing the oldest shifts every element, so it fails the
    address test: a flat map is not a node container.

## 01-Q3
- Allowed: recycling blocks the pool already owns (erase then insert of the same size).
- Forbidden: any request for new memory below the pool (a refill, or a big block such as flat
    container growth): it throws `std::bad_alloc`.
- Flag: `-DHFT_MEM_TRIPWIRE=1` (overflow upstream becomes `null_memory_resource()`).

## 01-Q4
- `hft::thread_arena arena;` at the top of the thread, then `hft::vector<Quote> quotes;`.
- Extra step: `quotes.reserve(max_expected)` during warm-up, because a vector grows by
    reallocating, which a pool does not prevent.

## 01-Q5
- A rehash: the table grew, allocated a bigger array and moved every element in one insert.
    (Not a page fault or a lock: one insert paid for copying the whole table.)
- Fix: `reserve(n)` for the maximum size during warm-up (here the max fell by more than
    1,000×).
- Self-graded against this Key: a free-text "what and why" cannot be graded honestly by regex.

## 01-Q5 stretch
- Zero faults rules out first-touch page faults inside the loop. Nothing else was isolated:
    the cause of that tail is unknown. Before naming one, change one thing at a time (for
    example `-DHFT_MEM_CHECKED=0`, a smaller or larger region, a profiler on that row) on an
    isolated core, and re-run several times. Do not blame "the machine" for a gap that shows
    up in every run of one row and no run of the other.

## 01-INT
- Create `hft::thread_arena` at the top of the owning thread, before any container.
- Switch the types to `hft::map`, `hft::flat_map`, `hft::unordered_flat_map`.
- Warm up: `reserve()` the two flat containers (and optionally `arena.warm`), build the book.
- `arena.seal()` before the hot path (and build tests with `-DHFT_MEM_TRIPWIRE=1`): any
    allocation of new memory now throws in the test.

## 02-P1
- No. Deallocate on a monotonic resource does nothing; the 64 bytes are never reused until
    `release()` or destruction. (l2_layers section 1.) CONCEPT L2C2.

## 02-P2
- Yes. There is one default-resource pointer for the whole program, not one per thread.
    (l2_layers section 4 prints the other thread's 4-byte allocation.) CONCEPT L2C4.

## 02-P3
- No: "no matching constructor". `boost::container::pmr::memory_resource` and
    `std::pmr::memory_resource` are different classes. CONCEPT L2C5.

## L2C1 Elaborate
- The resource is chosen at run time through the pointer inside polymorphic_allocator, so the
    container type does not encode where memory comes from.

## L2C2 Elaborate
- Per-message data all dies together, so release() frees it in one step. An order book frees
    single nodes all the time, and monotonic never reuses them, so memory only grows.

## L2C3 Elaborate
- When a size class runs out it must fetch and carve a new chunk from upstream; that refill is
    slow and grows with chunk size, unless chunks are capped and the pool is warmed beforehand.

## L2C4 Elaborate
- It is process-wide: every other thread's default-constructed pmr containers start throwing too,
    not just the thread under test.

## L2C4 HandsOn
- It throws at v.size() = 16: each new vector size lands in a new size class, and the pool grabs
    a ~1 KiB chunk per class, so 8 KiB is gone after six requests. Pools spend memory per size class.

## L2C5 Elaborate
- When the size has a hard, small upper bound: the elements live inside the object, so there is
    no allocation, no pointer chase and no allocator to configure.

## L2C5 HandsOn
- "9th push_back threw a std::exception that is NOT std::bad_alloc: boost::container::bad_alloc thrown".

## L2C6 Elaborate
- It is one process-wide pool per block size behind a lock: threads share it, contend on it,
    and its memory is not owned by (or local to) any one thread.

## 02-C1
- no-op ; release

## 02-C2
- `std::pmr::null_memory_resource()`

## 02-C3
- `do_is_equal` (takes the other resource: `do_is_equal(other)`). Lesson 03 writes all three.

## 02-Q1
- monotonic: bump-allocate from a buffer, ask upstream for a bigger buffer when out; deallocate does nothing.
- unsynchronized pool: per-size-class free lists; deallocate returns the block for reuse; big
    requests go straight upstream; single-thread only.
- null: allocate always throws std::bad_alloc; deallocate has no effect.
- new_delete: plain operator new / operator delete.

## 02-Q3
- It is a separate class hierarchy: std resources cannot be passed to boost::container::pmr containers.
- Its resources and polymorphic_allocator need the compiled library (link -lboost_container).
- Easy way: pass `std::pmr::polymorphic_allocator<...>` as the Boost container's allocator argument
    (for flat_map the value type is `std::pair<K, V>`).

## 02-Q2
- `std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource()); std::pmr::unsynchronized_pool_resource pool(&mono);`

## 02-Q5
- The default resource is process-wide, so the other test's default-constructed pmr containers
    (or any code using get_default_resource) now get the null resource.
- Use a per-container or per-arena tripwire instead: a chain whose upstream is
    null_memory_resource(), or hft's `arena.seal()` / `-DHFT_MEM_TRIPWIRE=1`, scoped to one thread.

## 02-Q4
- Both threads share one singleton pool per block size behind a lock (default Mutex = std::mutex
    here): more than 3× the cost per allocate+free pair of a pmr pool, measured single-threaded,
    before any contention.
- The memory is not returned when the lists die (singleton keeps it until release_memory/purge
    or exit), and it is not owned by either thread (no per-thread or per-node locality).

## 02-INT
- Pool: without it, freed nodes are never reused (monotonic only grows) → memory blow-up under churn.
- Gate: without it, no seal(); a hidden hot-path allocation would succeed silently.
- Monotonic: without it, every pool refill and flat-container block would go straight to the
    region with no cheap carving layer for big blocks (the region would have to do it).
- Region: without it, memory comes from malloc: first-touch faults in the hot path, no NUMA or
    huge-page control.
- Overflow upstream: new_delete keeps production alive when the region is full; null makes tests fail.

## 03-P1
- 4,096, then 0. mmap gave address space only; the first write to each page faults
    (≈1 µs per page here). That is why the region pre-faults at start-up. CONCEPT L3C2.

## 03-P2
- On the main thread's node. The default policy allocates "on the node of the CPU that
    triggered the allocation", which is whoever touched first. Pin first, then touch. CONCEPT L3C4.

## 03-P3
- Roughly a tie: the two medians were within about 15% of each other, and the stateless one
    was NOT the faster (bench/b2_alloc_flavors). Removing the virtual call did not pay here,
    because the thread_local lookup costs about as much. Measure, do not assume. CONCEPT L3C5.

## L3C1 Elaborate
- Containers use is_equal to decide whether memory from one allocator may be freed through
    another (move, swap). "Always equal" lets region A be asked to free region B's blocks.

## L3C1 HandsOn
- Original prints "b % 64 = 0". Without the rounding, b lands right after the 1-byte block and
    prints "b % 64 = 1": a misaligned pointer. (Surprise found while authoring: a plain `b % 64`
    test printed 0 even for the broken code, because the optimiser folded it; the lab reads the
    pointer through a volatile. Tests of allocators need the same care.)

## L3C2 Elaborate
- Cold caches and TLB entries for memory you have not used recently; pages can also be
    reclaimed later unless locked (mlock). Zero faults is not zero tail.

## L3C2 HandsOn
- Pass 1 about 1 µs per page with 4,096 faults; pass 2 with 0 faults and far less time
    (7–137 ns per page over 5 runs here; it varies a lot). mlock returned 0.

## L3C3 Elaborate
- MAP_HUGETLB depends on setup outside the program: pages a root user reserved in advance
    (nr_hugepages) and permission (mmap(2) lists EPERM without CAP_IPC_LOCK or the
    hugetlb_shm_group group). madvise(MADV_HUGEPAGE) is advice on a mapping that already
    exists and works with normal pages, so the program can carry on whatever it returns.

## L3C4 Elaborate
- The arena's constructor pre-faults the region, and the first touch decides the node.
    Constructed by main(), the pages land on main's node; the thread_local registration would
    also point at main's thread, not the worker's.

## L3C4 HandsOn
- Steps: pin, map (hugetlb or madvise), mbind, touch + mlock, verify (move_pages + numa_maps).
    On this Mac the program only prints that it is Linux-only.

## L3C5 Elaborate
- Each thread's arena registers itself in a thread_local pointer that the allocator reads on
    every call. A vector handed to the other thread would be freed into that thread's arena, which
    does not own the memory: the checked build aborts.

## L3C5 HandsOn
- Both threads print "vector buffer in MY region? yes  string buffer in MY region? yes" with
    different arena addresses; main prints "no thread_arena on this thread".

## 03-C1
- `return this == &other;`

## 03-Q1
- `std::size_t s = (off_ + align - 1) & ~(align - 1); if (s + bytes > cap_) throw std::bad_alloc(); off_ = s + bytes; return base_ + s;`

## 03-Q3
- It moves every page fault (about a microsecond each here) to start-up, out of the hot path.
- Linux: mmap with MAP_POPULATE, or madvise(MADV_POPULATE_WRITE) (Linux 5.14+). Or write a byte per page.
- mlock keeps the pages resident in RAM (no later paging), limited by RLIMIT_MEMLOCK unless CAP_IPC_LOCK.

## 03-Q4
- P M B T H (or M P B T H). Required: M before B (bind needs the range), B before T (the policy
    affects only new allocations), P before T (first touch picks the node), T before H.

## 03-Q2
- do_allocate: return storage of at least `bytes` aligned to `alignment`, or throw (never return null).
- do_deallocate: accept exactly what this resource handed out (a region may do nothing).
- do_is_equal: true only if memory from one can be freed by the other (a region: only itself).

## 03-Q6
- Stateless: 8 bytes smaller per container (24 vs 32 B; 16 KB over 2,000 maps), no resource
    pointer to pass, nested hft types pick up the arena automatically.
- Stateful: the container is not tied to the current thread (another thread could free it if it
    uses the same resource, with external synchronisation), and the call cost measured about the
    same, so there is no speed reason to avoid it.
- Pick: either is defensible; with one owning pinned thread, stateless is simpler. A good
    answer bases the choice on ownership and size, not on "virtual calls are slow".

## 03-Q5
- `numastat -p <pid>` (per-node memory of a process).
- `/proc/<pid>/numa_maps`: the `N<node>=<pages>` fields of the region's line.
- `move_pages(pid, count, pages, NULL, status, 0)`: with nodes NULL it returns each page's node in status.

## 03-Q7
- Usable: transparent huge pages via `madvise(MADV_HUGEPAGE)` on the normal mapping (with
    `enabled` = `madvise`, THP is used only in regions marked this way; only hugepage-aligned
    parts can be replaced). This is `hft::region_resource`'s default route.
- Not usable: `mmap(..., MAP_HUGETLB)`: it takes pages from a pool a root user reserves via
    `/proc/sys/vm/nr_hugepages`, and the pool is empty (HugePages_Total 0).
- Verify: the `AnonHugePages` fields of `/proc/PID/smaps` for the region.

## 03-INT
- Pin first: `hft::pin_this_thread(cpu)` (pthread_setaffinity_np), so first touch happens on that node.
- Construct `hft::thread_arena` on that thread with `numa_node` set (built with -DHFT_MEM_NUMA=1):
    mmap → madvise(MADV_HUGEPAGE) → mbind → pre-fault (the touch) → optional mlock.
- Create the containers after the arena (aliases), reserve/warm, then `arena.seal()`.
- Verify placement once (numastat -p / numa_maps / move_pages), then start the hot path.

## 04-P1
- It crashes with SIGSEGV (exit 139 here). The arena's region was unmapped, and the map's
    destructor walks its nodes, which lived in that region. The library's "no arena" check never
    gets a chance. CONCEPT L4C1.

## 04-P2
- 256 (and `hardware_constructive_interference_size` 64). GCC 13 on aarch64 Linux also
    reported 256; clang 18 with libstdc++ did not define the constant at all. Neither is "the
    cache line size". CONCEPT L4C4.

## 04-P3
- Something else: `get_default_resource()`. Copy construction calls
    `select_on_container_copy_construction`, which returns "a default-constructed
    polymorphic_allocator". Your copy silently left the arena. CONCEPT L4C2.

## L4C1 Elaborate
- In `thread` the region still exists, so the checked deallocate runs and sees a pointer outside
    the current thread's region. In `outlive` the region is already unmapped; the destructor reads
    a node there and faults before deallocate is ever called.

## L4C1 HandsOn
- static: "static container: hft_mem: no thread_arena on this thread". thread: the abort message
    above and exit status 134.

## L4C2 Elaborate
- It has no state to pass down: every hft::allocator, including the string's own, finds the same
    thread arena through the thread_local pointer.

## L4C2 HandsOn
- pmr-nested: std::string version 1 operator new call, pmr::string version 0. pmr-move: across
    resources "buffer stolen? no"; same resource "buffer stolen? yes".

## L4C3 Elaborate
- When you have many small containers (or ones you never walk): one pool keeps fewer partly-used
    chunks and less memory overall. Per-container wins for a few big containers you iterate.

## L4C4 Elaborate
- The x86 line is 64 B; here it is 128 B, so two 64-B-aligned counters can still share one line.

## L4C5 Elaborate
- The node map allocates one node per element, which the pool recycles. The flat map allocates
    one table and grows it by rehashing into a bigger one; only pre-sizing removes that copy.

## L4C6 Elaborate
- A free block holds no live data, so its bytes can hold the link. The list costs no extra
    memory, and there is nothing to allocate when you free.

## L4C6 HandsOn
- Production build: completes and prints "overflow allocations=2" (the 1 MiB region was too
    small; memory silently came from malloc). Tripwire build: "TRIPWIRE: region exhausted ..." and
    exit status 3.

## 04-C1
- before

## 04-Q1
- `hft::multi_index<Order, Indices>` (or `boost::multi_index_container<Order, Indices, hft::allocator<Order>>`).

## 04-Q2
- Container outlives its arena: the destructor touches unmapped memory → crash (SIGSEGV).
- Static/global container built before the arena exists → `std::logic_error` "no thread_arena on this thread".
- Container destroyed (or moved and then freed) on another thread → checked abort "not from this
    thread's arena"; unchecked, corruption of the wrong pool.

## 04-Q3
- `std::pmr::string`

## 04-Q4
- (a) node container → the pool (`hft::multi_index`).
- (b) flat container → `hft::flat_map` + `reserve()` at warm-up (big block from the monotonic/region layer).
- (c) per-message scratch → monotonic (`hft::scratch`) + `reset()` after each message.
- (d) hard small bound → `static_vector<Leg, 8>` (or `small_vector`), no allocator.

## 04-Q5
- `struct Counters { alignas(128) std::atomic<long> a; alignas(128) std::atomic<long> b; };`
- Portable spelling, with the guard from CONCEPT L4C4 (clang 18 + libstdc++ has no constant):
    `#ifdef __cpp_lib_hardware_interference_size` → `alignas(std::hardware_destructive_interference_size)`
    (256 here), `#else` → `alignas(128)`.
- Explicit padding (`std::atomic<long> a; std::byte pad[128]; std::atomic<long> b;`) also passes:
    a and b are always more than one line apart. `alignas(64)` fails on this Mac (128 B lines)
    though it is right on x86.

## 04-Q6
- `auto* n = static_cast<node*>(p); n->next = head_; head_ = n;`

## 04-Q7
- Risk thread: shared per-thread pool. Many small containers would waste partly-used chunks
    with one pool each, and nobody walks them.
- Matching thread: a pool for that one big container, so its nodes stay adjacent and price walks
    touch less memory (per-container pool about half the time per node of a shared pool on random
    keys here).

## 04-INT
- One arena shared by two threads: hft is unsynchronized and thread-local, so the workers have
    no arena (logic_error), or with one they would free into the wrong pool. Use one arena and one
    store per pinned worker, built on that worker.
- The arena lives on main's NUMA node (first touch by main), not the workers' nodes.
- A global store built before or apart from its arena: lifetime coupling (static init / outlive).
- `pmr::vector<std::string>`: string bytes escape to malloc; use `pmr::string` or `hft::string`.

## 05-P1
- (d) ≈ (c) < (a) < (b). Measured per batch of 64 random orders (p50): enum 83 ns, hook 125 ns,
    unordered_set 750 ns; ONE list find 66,083 ns. The hook is not faster than a flag: it reads
    the order's own bytes, like the flag. Why it measured slower (two compares, or its offset 80
    in the 112 B order vs the enum's 29) is not isolated. Its win is elsewhere (L5C2).

## 05-P2
- `true`, and that is the wrong answer. A hook knows it is linked into SOME list through that
    hook, never which one. `labs/l5_membership` section 4 prints exactly this. CONCEPT L5C4.

## 05-P3
- It does not compile: a static assertion in `generic_hook.hpp` requires safe_link or
    auto_unlink. With normal_link nothing records "unlinked", so membership needs another source
    (a state field, a bit). CONCEPT L5C3.

## L5C1 Elaborate
- The links live inside the order, so the order's address IS the node's address: the iterator is
    computed from it with no search. `std::list::remove` must walk to find the node holding the value.

## L5C1 HandsOn
- before: in both lists, level size 156. after: in neither, size 155. 0 operator new calls.
    Live orders 9999 of 10000, then 0 after the tidy shutdown.

## L5C2 Elaborate
- The hook IS the list membership, so it cannot drift from the list; and the same hook gives O(1)
    unlink and next/prev for the transition, which a flag cannot.

## L5C2 HandsOn
- Section 3: `order 105: state says PendingSend? 0   send hook says linked? 1`.

## L5C3 Elaborate
- That this hook is currently in some container; not which container, and not which of several
    containers sharing the hook.

## L5C3 HandsOn
- `list.hpp:125: error: static assertion failed ... !(constant_time_size && ... auto_unlink)`.

## L5C4 Elaborate
- A hook can be in one container at a time, so the order cannot be "pending send" and
    "pending ack" together; to move it you must unlink first, which is the transition.

## L5C4 HandsOn
- `Assertion failed: (!safemode_or_autounlink || node_algorithms::inited(to_insert)), function push_back`, exit 134.

## L5C5 Elaborate
- Pointer model: load the list node's next, then load the Order through the node's pointer.
    Intrusive: the next link already points at the next order's hook, inside the order.

## L5C5 HandsOn
- std::map 1.00 / 72.0; flat_map 0.00 / 40.0; multi_index 1.00 / 81.9; POINTER 2.00 / 108.7; INTRUSIVE 0.00 / 94.4.

## L5C6 Elaborate
- With no size counter, unlinking touches only the neighbours. Boost's default
    `constant_time_size<true>` must decrement the list's count, so it needs the list object.

## 05-C1
- safe_link

## 05-C2
- `b.clients[o->client].erase(Client::s_iterator_to(*o));`
- Skip it, or unlink from the wrong list, and `destroy` asserts: the order is still linked.

## 05-Q1
- `return o.send_hook.is_linked();`

## 05-Q2
- `Order* o = b.ids.find(id)->second;`
    `b.levels[o->px].erase(Level::s_iterator_to(*o));`
    `b.clients[o->client].erase(Client::s_iterator_to(*o));`
    `b.ids.erase(id); b.orders.destroy(o);`
- Forget one unlink and `destroy` asserts (safe_link): the order is still in a list.

## 05-Q3
- O(1) given the object: unlink/erase from a list (via iterator_to / s_iterator_to), next/prev,
    is_linked() (safe/auto mode), moving between lists, slab release.
- Not O(1): find by key in an intrusive set (log n) or unordered_set (average 1, depends on your
    buckets); slist erase (linear in position); size() without constant_time_size; "which list"
    with a shared hook; destroying a safe-mode list (n).

## 05-Q4
- `return o.send_hook.is_linked() && o.venue == v;`
- `is_linked()` alone says "some venue", which is the trap.

## 05-Q5
- (a) 1 (one node holding both index headers); (b) 2 (list node + hash node; 3 if the Order is
    also `new`ed); (c) 0.
- (c) pays bytes inside the object: 16 B per list it can join (l5_models: 0.00 new/ins, 94 B/order total).

## 05-Q6
- Both, or a hook alone: (2) needs to walk only the breached accounts and unlink each in O(1),
    which is a hook's job; (1) can be the hook's `is_linked()` (measured 125 ns per 64 here) or a
    bit if you want the cheapest test. A bit alone forces a scan of 200,000 accounts for (2).

## 05-Q7
- 64 hooks × 16 B = 1,024 B per symbol, eight 128 B lines of links per symbol, most of them idle.
- A `std::uint64_t` subscription bitmask answers "is gateway g subscribed?" in O(1) with 8 B; keep
    per-gateway iteration in a per-gateway array of symbol ids (or hooks only for the few hot gateways).

## 05-Q8
- safe_link (the default) and auto_unlink; with normal_link it does not compile.
- `true` = this hook is currently in some container. It does not say which container, nor which
    of several lists sharing the hook (the Venue X trap).

## 05-INT
- level_hook + client_hook (2 × 16 B); ONE state hook shared by the 4 venue send lists AND the
    ack list (16 B), so the order is in at most one of them; a 1-byte venue field.
- Membership: level/client/state via is_linked() (safe_link); "queued for venue X" =
    state-hook linked && venue == X && not in ack: so also keep a 1-byte "which state list" tag
    (or give ack its own hook and accept that both-at-once is then representable).
- Cost: 48 B of hooks + 2 B of tags on top of the payload; all transitions O(1), no allocation.

## 06-P1
- 32 B (4 pointers) and 24 B (3 pointers): optimize_size packs the colour bit into the parent
    pointer. A list hook is 16 B, an unordered_set hook 8 B, an slist hook 8 B. CONCEPT L6C2.

## 06-P2
- Nothing grows: "not rehashed to maintain a load factor". bucket_count stays 64 (load factor
    781) and mean find(id) was about 2,710 ns; after `rehash()` onto a 65,536-bucket array you supply,
    1.5–1.6 ns. CONCEPT L6C3.

## 06-P3
- Debug: the safe_link hook's destructor asserts (`!hook.is_linked()`), exit 134.
- `-DNDEBUG`: no check; the new order reused the slot ("yes") and the program then crashed
    (exit 139) while the level was walked or cleared. CONCEPT L6C5.

## L6C1 Elaborate
- A free slot holds no object, so its first bytes can hold the "next free" pointer; the list
    costs nothing and freeing needs nothing.

## L6C1 HandsOn
- 0 operator new; slots adjacent (64 B); LIFO yes; full → try_create nullptr, create → bad_alloc;
    then the LIFO/FIFO line.

## L6C2 Elaborate
- Member hooks leave the class hierarchy alone and need no tags. Base hooks make the hook part of
    the type's identity (useful when one hook type per class is natural, e.g. one list only).

## L6C2 HandsOn
- 16, 32 (24 with optimize_size), 8.

## L6C3 Elaborate
- It allocates nothing: the bucket array belongs to you, so only you can supply a bigger one.

## L6C3 HandsOn
- `reserve(50000): ... operator new in churn: 9 (each 1048576 B, first at op 20686)`;
    `reserve(100000): ... 0`.

## L6C4 Elaborate
- The list does not know where the object came from (slab, stack, vector); only you do, so you
    hand that knowledge in as the disposer.

## L6C4 HandsOn
- `~list with 1,000,000 orders: safe_link ~1400 us, normal_link ~0 us`, and the safe list left all
    orders unlinked (live 1000000) so the slab could take them back.

## L6C5 Elaborate
- The slot (address) is the same for the old and the new order; only the generation changed on
    destroy(), so comparing it tells "same object" from "same address".

## L6C5 HandsOn
- aba: `raw pointer to order 7 now reads id=8 ... handle ... get() = nullptr`. iterate-fixed: `level: 1 3 5`.

## L6C6 Elaborate
- Pointers hold absolute addresses of the old copy; indices are offsets from whatever base the
    slab now has.

## L6C6 HandsOn
- 16 vs 8 B per hook, 80 vs 56 B per order; the COPY walks `100 102 104 106` with indices; the
    pointer copy says "next is in the OLD array"; the offset_ptr copy walks fine at 16 B per hook.

## 06-C1
- unlinked (or erased / removed / detached)

## 06-C2
- `if (l.next != nil) (base_[l.next].*M).prev = l.prev; else tail_ = l.prev;`
- The mirror image of the given line: forget the `else tail_` and erasing the last slot leaves
    the tail pointing at a freed slot.

## 06-Q1
- list 16, slist 8, set 32 / 24 with optimize_size, unordered_set 8.
- Two base hooks of one kind need different `tag<>`s (member hooks need none).

## 06-Q6
- `ilink& l = base_[i].*M;`
    `if (l.prev != nil) (base_[l.prev].*M).next = l.next; else head_ = l.next;`
    `if (l.next != nil) (base_[l.next].*M).prev = l.prev; else tail_ = l.prev;`
    `l.prev = l.next = nil;`

## 06-Q2
- You supply it (bucket_traits); it is never rehashed for you (call `rehash()` with a new array);
    it must outlive the container (and is not shared between containers); power_2_buckets needs a
    power of two.
- Too small: no error, no allocation, long chains: mean find about 2,710 ns with 64 buckets vs 1.5–1.6 ns with 65,536 here.

## 06-Q5
- `if (Order* o = pool.get(h)) pool.destroy(o);`

## 06-Q3
- `lvl.clear_and_dispose([&](Order* o) { pool.destroy(o); });`

## 06-Q7
- Index links (or offset_ptr hooks): a memcpy'd slab keeps working at the new address; raw
    pointer hooks point into the writer's memory.
- Index links: 8 B per hook and relocatable, but every step is base + i × stride and needs nil
    checks (22 vs 5 instructions for erase here). offset_ptr: Boost does it for you, 16 B per hook.

## 06-Q4
- Destroy while linked (caught: assert); object dies before its list (caught: assert);
    auto_unlink surprises (not a bug Boost can see); stale pointer after slot reuse (not caught:
    use a generation handle); erase inside a range-for (not caught: crash).
- With -DNDEBUG none is caught.

## 06-INT
- Destroy before unlink: in debug (safe_link) an assert; with normal_link no check at all, and
    after slot reuse the level contains a different order. Unlink first, then destroy (or erase_and_dispose).
- normal_link: no is_linked(), no reset, no debug checks; the measured speed difference vs
    safe_link was below the timer here (one extra store in the code): keep safe_link.
- Flat map reserved for exactly the peak: under churn it rehashes (1 MiB, max 531 µs here): reserve with headroom (2×).
- Raw `Order*` held by clients: ABA after slot reuse; hand out `slab::handle` and use `get()`.

## 07-P1
- About 8×: 27 ns vs 208 ns mean (p50 41 vs 208). The pool did not close the gap: the default
    allocator version was 194 ns. The difference is the work, not the allocator. CONCEPT L7C1.

## 07-P2
- In half the processes about 32,500 ns (p99; 5 of 10 runs) against 42 for the free list; in
    the others object_pool was as fast as the free list. `destroy()` is `ordered_free`, O(length of
    the free list); the cost depends on where the free blocks sit in memory. CONCEPT L7C3.

## 07-P3
- Boost default (safe_link + constant-time size): 8; hand-rolled: 5; Boost with
    `normal_link` + `constant_time_size<false>`: 4. The extra Boost instructions are the size
    counter (3) and the safe-mode reset (1 `stp` of zeros). CONCEPT L7C3.

## L7C1 Elaborate
- The allocation was never the main cost of that add: hashing, the ordered (tree) insert and
    rebalancing are, and they are the same with any allocator.

## L7C1 HandsOn
- pending-send 1 → 0 and pending-ack 0 → 1 after mark_sent; level qty 4900 → 4840 after
    reduce; mass cancel of client 3: 3125 orders; 0 operator new in the hot section; the stale
    handle id → "false (stale id rejected)".

## L7C2 Elaborate
- The timer stores its due tick, so its bucket is `due & 63`, and `s_iterator_to` gives the
    iterator from the timer itself.

## L7C2 HandsOn
- Timers 2 (due 5) and 3 (due 70) fire, 0 live after 80 ticks; orders 100 and 101 escalate, 2
    still waiting; LRU evicts 2, 3, ends `1 4 5`.

## L7C3 Elaborate
- The difference (a size counter, one reset store) is smaller than this timer can see; it may
    matter in a tight loop on your box, so measure there with a batched bench or perf counters
    before hand-rolling for it, or simply turn the Boost options off.

## L7C4 Elaborate
- Destroying or reusing an object that is still linked (and inserting a linked object twice):
    Boost asserts in debug builds; your hand-rolled list silently corrupts.

## L7C5 Elaborate
- It owns its elements: no destroy-while-linked, no dangling pointer, no forgotten unlink; you pay
    about 7–8× on add and 2–4× on cancel here.

## 07-C1
- ordered

## 07-C2
- `send.erase(SendQueue::s_iterator_to(o));` (or `send.erase(send.iterator_to(o));`)
- Without it the order sits in both queues. `o.send_hook.unlink()` does not compile here: it is
    "only allowed if link_mode is auto_unlink" (list_hook.hpp), and this hook is safe_link.

## 07-Q1
- Find by id (hash probe, or the id is the slab handle), unlink from every list it is in
    (`is_linked()` tells which state lists), release the slot to the slab.
- Allocations per add: intrusive 0; multi_index 1 node; pointer model 4 (Order, 2 list nodes, hash node).

## 07-Q3
- Take Boost's lists / unordered_set / trees with chosen options (member hooks, safe_link,
    constant_time_size<false> where size() is unused, your own buckets); speed within the timer
    floor of hand-rolled, plus debug checks and trees.
- Hand-roll the pool (O(1) LIFO free list), generation handles, and index links if you need 8 B
    hooks or relocation.
- object_pool's destroy is ordered_free: p99 ≈ 32.5 µs in 5 of 10 runs here (3 of 5, then 2 of
    5); a plain free list never left the floor.

## 07-Q2
- `send.erase(SendQueue::s_iterator_to(o)); ack.push_back(o);`

## 07-Q4
- A flat container (`static_vector` / `flat_map` of ≤ 20, or a sorted small array): one structure,
    no cross-membership, scans dominate, and contiguous storage is smaller and faster to scan
    (lean 375 vs hooked 875 ns per 1,024 records here). Intrusive's strength (membership in many
    structures, O(1) unlink by object) is unused.

## 07-Q5
- Timer wheel: power-of-two array of intrusive lists (no size counter), timers in a slab, each
    timer stores its due tick. Schedule: slab create + push_back into `due & mask`, O(1).
    Cancel: `erase(s_iterator_to(t))` + destroy, O(1), no search. Tick: walk one bucket; delays
    longer than the wheel wrap, so compare `due` before firing.

## 07-Q6
- Check whether the difference is visible on the real workload at all (here every list variant was
    at the timer floor; use a batched bench or perf counters on the target box).
- Suggest Boost with `constant_time_size<false>` (5 instructions, the same as the hand-roll here)
    and keep safe_link for its checks; hand-roll only what Boost cannot express that you need
    (index links, generations).

## 07-INT
- Storage: one `hft::slab<Order>` per pinned thread, carved from its arena; LIFO free list;
    try_create → reject on exhaustion; handles {index, generation} as the client order id.
- Hooks (Boost member hooks, safe_link): price level, client, one state hook for
    send/ack (or one each), membership by `is_linked()`.
- Id index: the handle (no map) if you assign ids; otherwise a flat map reserved with headroom.
- Boost: lists, unordered_set/trees, disposers, debug checks. Hand-roll: slab, handles, index links if needed.
- Ratios: add ≈ 7–8× cheaper than multi_index, cancel 2–4× cheaper, 0 allocations vs 4 for the pointer model.
- Risks: destroy-while-linked / ABA (debug asserts in CI, handles, unlink-then-dispose); cross-thread
    use (single owner thread, queues).
