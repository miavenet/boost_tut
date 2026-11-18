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

### CARD 02-P1  [type: pretest]  [topic: mono]  [box: 1]
Q: Cold guess. With a `std::pmr::monotonic_buffer_resource mono;` you do
   `void* a = mono.allocate(64); mono.deallocate(a, 64); void* b = mono.allocate(64);`.
   Is `b == a`?
Key:
  - No. Deallocate on a monotonic resource does nothing; the 64 bytes are never reused until
    `release()` or destruction. (l2_layers section 1.) CONCEPT L2C2.
Concepts: L2C2
Source: labs/l2_layers.cpp run 2026-11-13

### CARD 02-P2  [type: pretest]  [topic: default]  [box: 1]
Q: Cold guess. The main thread calls `std::pmr::set_default_resource(&tracer)`. A different
   thread then default-constructs a `std::pmr::vector<int>` and pushes one element. Does the
   tracer see that allocation?
Key:
  - Yes. There is one default-resource pointer for the whole program, not one per thread.
    (l2_layers section 4 prints the other thread's 4-byte allocation.) CONCEPT L2C4.
Concepts: L2C4
Source: labs/l2_layers.cpp run 2026-11-13

### CARD 02-P3  [type: pretest]  [topic: boost]  [box: 1]
Q: Cold guess. Does this compile?
   `std::pmr::monotonic_buffer_resource mono; boost::container::pmr::flat_map<int,double> m(&mono);`
Key:
  - No: "no matching constructor". `boost::container::pmr::memory_resource` and
    `std::pmr::memory_resource` are different classes. CONCEPT L2C5.
Concepts: L2C5
Source: compiled 2026-11-13 (Boost 1.90, Apple clang 21)

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
Elaborate: Why can one `std::pmr::map<int,int>` type sit on a pool in one place and on a stack buffer in another, with no change to the type?
ElaborateKey:
  - The resource is chosen at run time through the pointer inside polymorphic_allocator, so the
    container type does not encode where memory comes from.
Source: https://en.cppreference.com/w/cpp/memory/polymorphic_allocator/polymorphic_allocator ; https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate ; bench/b2_alloc_flavors.cpp sizeof lines

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
Elaborate: Why is a monotonic resource ideal under a per-message parser but wrong under an order book?
ElaborateKey:
  - Per-message data all dies together, so release() frees it in one step. An order book frees
    single nodes all the time, and monotonic never reuses them, so memory only grows.
Source: https://en.cppreference.com/w/cpp/memory/monotonic_buffer_resource ; bench/RESULTS.md (2026-11-13) ; bench/results_r2/RESULTS.md (2026-11-14)

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
Elaborate: The pool reuses freed blocks, so why can it still spike in a hot loop?
ElaborateKey:
  - When a size class runs out it must fetch and carve a new chunk from upstream; that refill is
    slow and grows with chunk size, unless chunks are capped and the pool is warmed beforehand.
Source: https://en.cppreference.com/w/cpp/memory/unsynchronized_pool_resource ; https://en.cppreference.com/w/cpp/memory/synchronized_pool_resource ; https://en.cppreference.com/w/cpp/memory/pool_options ; bench/results_r2/RESULTS.md (l1_tail_histogram nowarm / nowarm nocap, 5 runs each, 2026-11-14)

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
FigureText: Five boxes stacked top to bottom: pool (recycles), gate (seal), monotonic (bump), region (pre-faulted mmap, NUMA, huge pages), overflow (new_delete in production, null in tripwire mode). Arrows point down: each asks the one below only when it has nothing left.
Elaborate: Why is `set_default_resource(null_memory_resource())` a dangerous tripwire in a multithreaded test binary?
ElaborateKey:
  - It is process-wide: every other thread's default-constructed pmr containers start throwing too,
    not just the thread under test.
HandsOn: In `./labs/build/l2_layers` section 5, a vector of 4,000 longs on pool → monotonic over an 8 KiB stack buffer throws early. Note at what size, and count the "upstream allocate" lines.
HandsOnKey:
  - It throws at v.size() = 16: each new vector size lands in a new size class, and the pool grabs
    a ~1 KiB chunk per class, so 8 KiB is gone after six requests. Pools spend memory per size class.
Source: https://en.cppreference.com/w/cpp/memory/null_memory_resource ; https://en.cppreference.com/w/cpp/memory/set_default_resource ; labs/l2_layers.cpp run 2026-11-13

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
Elaborate: When is `static_vector<Level, 16>` a better answer than any allocator?
ElaborateKey:
  - When the size has a hard, small upper bound: the elements live inside the object, so there is
    no allocation, no pointer chase and no allocator to configure.
HandsOn: Run `./labs/build/l2_boost_pmr` and read section 4.
HandsOnKey:
  - "9th push_back threw a std::exception that is NOT std::bad_alloc: boost::container::bad_alloc thrown".
Source: https://www.boost.org/doc/libs/1_90_0/doc/html/container/cpp_conformance.html ; labs/l2_boost_pmr.cpp run 2026-11-13 ; _evidence/r2/link-error.txt (build without -lboost_container, 2026-11-14)

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
Elaborate: Why does a singleton pool fight the "one arena per pinned thread" design?
ElaborateKey:
  - It is one process-wide pool per block size behind a lock: threads share it, contend on it,
    and its memory is not owned by (or local to) any one thread.
Source: https://www.boost.org/doc/libs/1_90_0/libs/pool/doc/html/boost_pool/pool/interfaces.html ; https://www.boost.org/doc/libs/1_90_0/libs/pool/doc/html/boost/fast_pool_allocator.html ; /opt/homebrew/include/boost/pool/detail/mutex.hpp (default_mutex) ; /opt/homebrew/include/boost/pool/pool_alloc.hpp lines 247, 272 ; bench/RESULTS.md

---

## Completion cards

### CARD 02-C1  [type: completion]  [topic: mono]  [box: 1]
Q: Fill both gaps (two words): "On a monotonic_buffer_resource, deallocate is a ______; the
   memory comes back only on ______() or destruction."
Key:
  - no-op ; release
AcceptIf: regex(/no.?op|nothing/i) AND regex(/release/i)
RejectIf: regex(/(frees?|returns?|reuses?) (the )?(block|memory) (immediately|at once)|(not|isn'?t) (a )?no.?op/i)
Concepts: L2C2
Source: CONCEPT L2C2

### CARD 02-C2  [type: completion]  [topic: chain]  [box: 1]
Q: Fill the gap. `std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, ______);` should throw
   `std::bad_alloc` the moment `buf` runs out. Type the upstream expression.
Key:
  - `std::pmr::null_memory_resource()`
AcceptIf: check(checks/l2_upstream.sh)
Concepts: L2C4
Source: CONCEPT L2C4

### CARD 02-C3  [type: completion]  [topic: resource]  [box: 1]
Q: Fill the gap with the third function name: "A `std::pmr::memory_resource` subclass overrides
   three private virtuals: `do_allocate(bytes, alignment)`, `do_deallocate(p, bytes, alignment)`
   and ______; the public `allocate`/`deallocate` call them."
Key:
  - `do_is_equal` (takes the other resource: `do_is_equal(other)`). Lesson 03 writes all three.
AcceptIf: regex(/^\s*`?(virtual\s+)?(bool\s+)?do_is_equal\s*(\(\s*(const\s+(std::pmr::)?memory_resource\s*&(\s*\w+)?|\w+)?\s*\))?\s*(const)?\s*(noexcept)?\s*(override)?\s*;?\s*`?\s*\.?\s*$/)
RejectIf: regex(/^\s*`?(is_equal|operator\s*==|do_equal|do_release)/i)
Concepts: L2C1
Source: CONCEPT L2C1 ; https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate

---

## Question cards

### CARD 02-Q1  [type: recall]  [topic: chain]  [box: 1]
Q: From memory, for each of these say in a few words what allocate and deallocate do:
   monotonic_buffer_resource, unsynchronized_pool_resource, null_memory_resource(),
   new_delete_resource().
Key:
  - monotonic: bump-allocate from a buffer, ask upstream for a bigger buffer when out; deallocate does nothing.
  - unsynchronized pool: per-size-class free lists; deallocate returns the block for reuse; big
    requests go straight upstream; single-thread only.
  - null: allocate always throws std::bad_alloc; deallocate has no effect.
  - new_delete: plain operator new / operator delete.
Concepts: L2C2, L2C3, L2C4
Source: CONCEPT L2C2 ; CONCEPT L2C3 ; CONCEPT L2C4

### CARD 02-Q3  [type: recall]  [topic: boost]  [box: 1]
Q: Give two facts about `boost::container::pmr` that bite people who already know `std::pmr`,
   and the easy way to put a Boost container on a std resource.
Key:
  - It is a separate class hierarchy: std resources cannot be passed to boost::container::pmr containers.
  - Its resources and polymorphic_allocator need the compiled library (link -lboost_container).
  - Easy way: pass `std::pmr::polymorphic_allocator<...>` as the Boost container's allocator argument
    (for flat_map the value type is `std::pair<K, V>`).
Concepts: L2C5
Source: CONCEPT L2C5

### CARD 02-Q2  [type: apply]  [topic: chain]  [box: 1]
Q: Code (one line, two declarations). Given `alignas(64) static std::byte buf[1 << 16];`, declare
   `mono` and `pool` so that a `std::pmr::map<int,int> m(&pool)` gets its nodes from `buf` and
   throws `std::bad_alloc` once `buf` is used up.
Key:
  - `std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource()); std::pmr::unsynchronized_pool_resource pool(&mono);`
AcceptIf: check(checks/l2_chain.sh)
Concepts: L2C3, L2C4
Source: labs/l2_layers.cpp section 5

### CARD 02-Q5  [type: apply]  [topic: default]  [box: 1]
Q: A test fixture calls `std::pmr::set_default_resource(std::pmr::null_memory_resource())` to
   catch hidden allocations, and the test runner runs fixtures on several threads. A different
   test, which never used pmr directly, starts failing with `std::bad_alloc`. Explain, and give
   a tripwire that avoids this problem.
Key:
  - The default resource is process-wide, so the other test's default-constructed pmr containers
    (or any code using get_default_resource) now get the null resource.
  - Use a per-container or per-arena tripwire instead: a chain whose upstream is
    null_memory_resource(), or hft's `arena.seal()` / `-DHFT_MEM_TRIPWIRE=1`, scoped to one thread.
Concepts: L2C4
Source: CONCEPT L2C4 ; https://en.cppreference.com/w/cpp/memory/set_default_resource

### CARD 02-Q4  [type: apply]  [topic: boost]  [box: 1]
Q: A team uses `std::list<Order, boost::fast_pool_allocator<Order>>` in two threads, one list per
   thread, and frees the lists at end of day. Name two costs they pay that a per-thread
   `std::pmr::unsynchronized_pool_resource` (or `hft::thread_arena`) would avoid.
Key:
  - Both threads share one singleton pool per block size behind a lock (default Mutex = std::mutex
    here): more than 3× the cost per allocate+free pair of a pmr pool, measured single-threaded,
    before any contention.
  - The memory is not returned when the lists die (singleton keeps it until release_memory/purge
    or exit), and it is not owned by either thread (no per-thread or per-node locality).
Concepts: L2C6
Source: CONCEPT L2C6

---

## Integrate (day close — one synthesis question)

### CARD 02-INT  [type: synthesize]  [topic: all]
Q: Draw (in words) the hierarchy inside `hft::thread_arena` from the container down, and for each
   layer say what it would cost you to delete it.
Key:
  - Pool: without it, freed nodes are never reused (monotonic only grows) → memory blow-up under churn.
  - Gate: without it, no seal(); a hidden hot-path allocation would succeed silently.
  - Monotonic: without it, every pool refill and flat-container block would go straight to the
    region with no cheap carving layer for big blocks (the region would have to do it).
  - Region: without it, memory comes from malloc: first-touch faults in the hot path, no NUMA or
    huge-page control.
  - Overflow upstream: new_delete keeps production alive when the region is full; null makes tests fail.
Concepts: L2C1, L2C2, L2C3, L2C4
Source: CONCEPT L2C4 ; labs/hft_mem/hft_mem.hpp
