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

### CARD 03-P1  [type: pretest]  [topic: touch]  [box: 1]
Q: Cold guess. You `mmap` 64 MiB (16 KiB pages = 4,096 pages) and write one byte per page, twice.
   How many page faults in pass 1, and in pass 2?
Key:
  - 4,096, then 0. mmap gave address space only; the first write to each page faults
    (≈1 µs per page here). That is why the region pre-faults at start-up. CONCEPT L3C2.
Concepts: L3C2
Source: labs/l3_first_touch.cpp run 2026-11-13

### CARD 03-P2  [type: pretest]  [topic: numa]  [box: 1]
Q: Cold guess (Linux, two sockets). The main thread `mmap`s a region and writes every page. Then it
   starts a worker thread pinned to a CPU on the OTHER socket and hands it the region. On which
   node is the memory?
Key:
  - On the main thread's node. The default policy allocates "on the node of the CPU that
    triggered the allocation", which is whoever touched first. Pin first, then touch. CONCEPT L3C4.
Concepts: L3C4
Source: https://man7.org/linux/man-pages/man2/set_mempolicy.2.html

### CARD 03-P3  [type: pretest]  [topic: tls]  [box: 1]
Q: Cold guess. Which costs more per 64-byte allocate+free pair on this Mac: a stateful pmr
   allocator (virtual call through `memory_resource*`) or `hft::allocator` (no pointer, direct
   call, but a `thread_local` lookup)?
Key:
  - Roughly a tie: the two medians were within about 15% of each other, and the stateless one
    was NOT the faster (bench/b2_alloc_flavors). Removing the virtual call did not pay here,
    because the thread_local lookup costs about as much. Measure, do not assume. CONCEPT L3C5.
Concepts: L3C5
Source: bench/RESULTS.md (2026-11-13)

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
Elaborate: Why would `return true;` in do_is_equal be a memory-corruption bug?
ElaborateKey:
  - Containers use is_equal to decide whether memory from one allocator may be freed through
    another (move, swap). "Always equal" lets region A be asked to free region B's blocks.
HandsOn: Run `./labs/build/l3_region`. Then delete the alignment line (`start = off_`), rebuild with `bash labs/build.sh`, and run again.
HandsOnKey:
  - Original prints "b % 64 = 0". Without the rounding, b lands right after the 1-byte block and
    prints "b % 64 = 1": a misaligned pointer. (Surprise found while authoring: a plain `b % 64`
    test printed 0 even for the broken code, because the optimiser folded it; the lab reads the
    pointer through a volatile. Tests of allocators need the same care.)
Source: https://en.cppreference.com/w/cpp/memory/memory_resource/do_allocate ; https://en.cppreference.com/w/cpp/memory/memory_resource/do_is_equal ; labs/l3_region.cpp run 2026-11-13

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
Elaborate: Pre-faulting removed the faults. Name one cost it does NOT remove.
ElaborateKey:
  - Cold caches and TLB entries for memory you have not used recently; pages can also be
    reclaimed later unless locked (mlock). Zero faults is not zero tail.
HandsOn: Run `./labs/build/l3_first_touch` twice and record pass 1 and pass 2 ns per page.
HandsOnKey:
  - Pass 1 about 1 µs per page with 4,096 faults; pass 2 with 0 faults and far less time
    (7–137 ns per page over 5 runs here; it varies a lot). mlock returned 0.
Source: https://man7.org/linux/man-pages/man2/mmap.2.html ; https://man7.org/linux/man-pages/man2/madvise.2.html ; https://man7.org/linux/man-pages/man2/mlock.2.html ; labs/l3_first_touch.cpp ; bench/results_r2/RESULTS.md (5 runs, 2026-11-14)

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
Elaborate: `l3_numa_linux.cpp` checks the result of the MAP_HUGETLB mmap and falls back, but calls madvise(MADV_HUGEPAGE) with no fallback. Why the difference?
ElaborateKey:
  - MAP_HUGETLB depends on setup outside the program: pages a root user reserved in advance
    (nr_hugepages) and permission (mmap(2) lists EPERM without CAP_IPC_LOCK or the
    hugetlb_shm_group group). madvise(MADV_HUGEPAGE) is advice on a mapping that already
    exists and works with normal pages, so the program can carry on whatever it returns.
Source: https://man7.org/linux/man-pages/man2/mmap.2.html ; https://man7.org/linux/man-pages/man2/madvise.2.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html

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
Elaborate: Why must the arena be constructed ON the worker thread, after pinning, rather than by main() and passed in?
ElaborateKey:
  - The arena's constructor pre-faults the region, and the first touch decides the node.
    Constructed by main(), the pages land on main's node; the thread_local registration would
    also point at main's thread, not the worker's.
HandsOn: Read `labs/l3_numa_linux.cpp` top to bottom and number its five steps. On a Linux NUMA box: `clang++ -std=c++23 -O2 labs/l3_numa_linux.cpp -lnuma -o numa && ./numa 0`.
HandsOnKey:
  - Steps: pin, map (hugetlb or madvise), mbind, touch + mlock, verify (move_pages + numa_maps).
    On this Mac the program only prints that it is Linux-only.
Source: https://man7.org/linux/man-pages/man2/set_mempolicy.2.html ; https://man7.org/linux/man-pages/man2/mbind.2.html ; https://man7.org/linux/man-pages/man3/numa.3.html ; https://man7.org/linux/man-pages/man2/move_pages.2.html ; https://man7.org/linux/man-pages/man7/numa.7.html ; https://man7.org/linux/man-pages/man8/numastat.8.html ; https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html

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
Elaborate: The same `hft::vector<hft::string>` type, used in two threads, lands in two different regions. What makes that happen, and what does it imply if one thread hands its vector to the other?
ElaborateKey:
  - Each thread's arena registers itself in a thread_local pointer that the allocator reads on
    every call. A vector handed to the other thread would be freed into that thread's arena, which
    does not own the memory: the checked build aborts.
HandsOn: Run `./labs/build/l3_behind_the_scenes`.
HandsOnKey:
  - Both threads print "vector buffer in MY region? yes  string buffer in MY region? yes" with
    different arena addresses; main prints "no thread_arena on this thread".
Source: labs/hft_mem/hft_mem.hpp ; labs/l3_behind_the_scenes.cpp run 2026-11-13 ; bench/asm_alloc.sh + bench/asm_alloc_paths.cpp (output: _evidence/r2/asm-alloc.txt, 2026-11-14) ; bench/RESULTS.md

---

## Completion cards

### CARD 03-C1  [type: completion]  [topic: region]  [box: 1]
Q: Fill in the body of `do_is_equal(const std::pmr::memory_resource& other) const noexcept` for a
   resource that owns its own region (one statement).
Key:
  - `return this == &other;`
AcceptIf: check(checks/l3_is_equal.sh)
Concepts: L3C1
Source: CONCEPT L3C1

---

## Question cards

### CARD 03-Q1  [type: apply]  [topic: region]  [box: 1]
Q: Code (one line is fine). Write the body of `do_allocate(std::size_t bytes, std::size_t align)`
   for a bump region with members `std::byte* base_; std::size_t cap_; std::size_t off_;`. It must
   honour `align` and throw `std::bad_alloc` when the region is full. (Graded by compiling and
   testing alignment, overlap and exhaustion.)
Key:
  - `std::size_t s = (off_ + align - 1) & ~(align - 1); if (s + bytes > cap_) throw std::bad_alloc(); off_ = s + bytes; return base_ + s;`
AcceptIf: check(checks/l3_do_allocate.sh)
Concepts: L3C1
Source: labs/l3_region.cpp

### CARD 03-Q3  [type: recall]  [topic: touch]  [box: 1]
Q: What does pre-faulting buy you, what are two Linux ways to ask the kernel to do it, and what
   does mlock add?
Key:
  - It moves every page fault (about a microsecond each here) to start-up, out of the hot path.
  - Linux: mmap with MAP_POPULATE, or madvise(MADV_POPULATE_WRITE) (Linux 5.14+). Or write a byte per page.
  - mlock keeps the pages resident in RAM (no later paging), limited by RLIMIT_MEMLOCK unless CAP_IPC_LOCK.
Concepts: L3C2
Source: CONCEPT L3C2

### CARD 03-Q4  [type: apply]  [topic: numa]  [box: 1]
Q: Order these five start-up steps for a NUMA-local hot-path region (answer with the letters,
   e.g. "X Y Z ..."): P = pin the thread to its CPU, M = mmap the region, B = mbind the region to
   the node, T = touch (pre-fault) every page, H = start the hot path.
Key:
  - P M B T H (or M P B T H). Required: M before B (bind needs the range), B before T (the policy
    affects only new allocations), P before T (first touch picks the node), T before H.
AcceptIf: check(checks/l3_numa_order.sh)
Concepts: L3C4
Source: CONCEPT L3C4

### CARD 03-Q2  [type: recall]  [topic: region]  [box: 1]
Q: From memory: the three functions a custom memory_resource overrides, and the one rule each must
   keep.
Key:
  - do_allocate: return storage of at least `bytes` aligned to `alignment`, or throw (never return null).
  - do_deallocate: accept exactly what this resource handed out (a region may do nothing).
  - do_is_equal: true only if memory from one can be freed by the other (a region: only itself).
Concepts: L3C1
Source: CONCEPT L3C1

### CARD 03-Q6  [type: apply]  [topic: tls]  [box: 1]
Q: Your team must choose between `std::pmr::map<K,V> m(arena.resource())` (stateful) and
   `hft::map<K,V>` (stateless) for 2,000 small per-symbol maps owned by one pinned thread. Using
   this lesson's measurements, give one argument for each side and say which you would pick.
Key:
  - Stateless: 8 bytes smaller per container (24 vs 32 B; 16 KB over 2,000 maps), no resource
    pointer to pass, nested hft types pick up the arena automatically.
  - Stateful: the container is not tied to the current thread (another thread could free it if it
    uses the same resource, with external synchronisation), and the call cost measured about the
    same, so there is no speed reason to avoid it.
  - Pick: either is defensible; with one owning pinned thread, stateless is simpler. A good
    answer bases the choice on ownership and size, not on "virtual calls are slow".
Concepts: L3C5
Source: CONCEPT L3C5 ; bench/RESULTS.md

### CARD 03-Q5  [type: recall]  [topic: numa]  [box: 1]
Q: Name three ways to verify on Linux which NUMA node your region's pages are on.
Key:
  - `numastat -p <pid>` (per-node memory of a process).
  - `/proc/<pid>/numa_maps`: the `N<node>=<pages>` fields of the region's line.
  - `move_pages(pid, count, pages, NULL, status, 0)`: with nodes NULL it returns each page's node in status.
Concepts: L3C4
Source: CONCEPT L3C4

### CARD 03-Q7  [type: apply]  [topic: huge]  [box: 1]
Q: Your Linux box shows `/sys/kernel/mm/transparent_hugepage/enabled` = `madvise` and
   `HugePages_Total: 0` in `/proc/meminfo`, and you have no root. Which huge-page route can your
   region use, which one cannot, and how do you check afterwards that it got huge pages?
Key:
  - Usable: transparent huge pages via `madvise(MADV_HUGEPAGE)` on the normal mapping (with
    `enabled` = `madvise`, THP is used only in regions marked this way; only hugepage-aligned
    parts can be replaced). This is `hft::region_resource`'s default route.
  - Not usable: `mmap(..., MAP_HUGETLB)`: it takes pages from a pool a root user reserves via
    `/proc/sys/vm/nr_hugepages`, and the pool is empty (HugePages_Total 0).
  - Verify: the `AnonHugePages` fields of `/proc/PID/smaps` for the region.
Concepts: L3C3
Source: CONCEPT L3C3 ; https://man7.org/linux/man-pages/man2/madvise.2.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/transhuge.html ; https://www.kernel.org/doc/html/latest/admin-guide/mm/hugetlbpage.html

---

## Integrate (day close — one synthesis question)

### CARD 03-INT  [type: synthesize]  [topic: all]
Q: Write the start-up sequence of one hot-path worker thread on a two-socket Linux box, from thread
   start to the first market-data message, naming which hft_mem call or Linux call does each step
   and why the order matters.
Key:
  - Pin first: `hft::pin_this_thread(cpu)` (pthread_setaffinity_np), so first touch happens on that node.
  - Construct `hft::thread_arena` on that thread with `numa_node` set (built with -DHFT_MEM_NUMA=1):
    mmap → madvise(MADV_HUGEPAGE) → mbind → pre-fault (the touch) → optional mlock.
  - Create the containers after the arena (aliases), reserve/warm, then `arena.seal()`.
  - Verify placement once (numastat -p / numa_maps / move_pages), then start the hot path.
Concepts: L3C1, L3C2, L3C3, L3C4, L3C5
Source: CONCEPT L3C4 ; CONCEPT L3C5 ; labs/hft_mem/hft_mem.hpp
