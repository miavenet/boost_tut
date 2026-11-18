// hft_mem.hpp — pooled, hierarchical memory "behind the scenes" for hot-path containers.
//
//   USE IT (two lines):
//     hft::thread_arena arena;                    // once, at the top of the thread that owns the data
//     hft::map<long, Order> book;                 // any alias below: no resource pointer anywhere
//
//   THE HIERARCHY, built once per thread inside thread_arena (top = closest to the container):
//     container --allocator--> unsynchronized_pool_resource   (recycles small node blocks by size class)
//                                   | asks for chunks
//                                   v
//                             gate_resource                    (pass-through; seal() makes it throw)
//                                   |
//                                   v
//                             monotonic_buffer_resource        (bump pointer, never frees until release())
//                                   | asks for buffers
//                                   v
//                             region_resource                  (owns ONE pre-faulted mmap region;
//                                   | when the region is full    Linux: huge pages, NUMA node, mlock)
//                                   v
//                             overflow upstream: new_delete_resource()  (production: slow but alive)
//                                            or null_memory_resource()  (tripwire: throws std::bad_alloc)
//
//   TRIPWIRES (no call site changes):
//     arena.seal()           after warm-up: any request for NEW memory below the pool throws
//     -DHFT_MEM_TRIPWIRE=1   default arena_config::tripwire = true: a full region throws
//                            instead of silently falling back to malloc
//     -DHFT_MEM_CHECKED=0    drop the per-deallocate ownership check (default: on); the cheap
//                            "no arena on this thread" abort stays in every build
//
// Linux-only parts (huge pages, NUMA binding, pinning) are inside #ifdef __linux__ and were
// NOT executed on the authoring machine (macOS arm64). Sources: lessons/lesson-03.md.
#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include <boost/container/flat_map.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_node_map.hpp>

#ifdef __linux__
#  include <pthread.h>
#  include <sched.h>
// NUMA binding is opt-in: build with -DHFT_MEM_NUMA=1 and link -lnuma (needs libnuma-dev).
// Without it, users of this header do not need libnuma at all.
#  if defined(HFT_MEM_NUMA) && HFT_MEM_NUMA
#    if __has_include(<numaif.h>)
#      include <numaif.h>
#      define HFT_MEM_HAVE_NUMAIF 1
#    else
// Asked for NUMA binding explicitly: fail the build instead of silently not binding.
#      error "HFT_MEM_NUMA=1 needs <numaif.h> (install libnuma-dev) and -lnuma"
#    endif
#  endif
#endif

#ifndef HFT_MEM_TRIPWIRE
#  define HFT_MEM_TRIPWIRE 0
#endif
#ifndef HFT_MEM_CHECKED
#  define HFT_MEM_CHECKED 1
#endif

namespace hft {

namespace detail {
[[noreturn]] inline void die(const char* why) noexcept {
    std::fprintf(stderr, "hft_mem: %s\n", why);
    std::abort();
}
inline std::size_t page_size() noexcept {
    long pg = ::sysconf(_SC_PAGESIZE);           // -1 on error: fall back instead of SIZE_MAX
    return pg > 0 ? static_cast<std::size_t>(pg) : std::size_t{4096};
}
}  // namespace detail

struct arena_config {
    std::size_t region_bytes = std::size_t{64} << 20;  // 64 MiB, pre-faulted at construction
    std::size_t first_chunk = std::size_t{1} << 20;    // monotonic's first buffer request
    // Pool refill cost grows with chunk size (measured: lesson 01 / labs/l1_tail_histogram).
    // Capping blocks per chunk keeps every refill small: more refills, no 100+ us spike.
    std::size_t max_blocks_per_chunk = 256;
    bool tripwire = HFT_MEM_TRIPWIRE;                  // overflow upstream = null_memory_resource
    bool lock_pages = false;                           // mlock(2) the region
    bool huge_pages = true;                            // Linux: madvise(MADV_HUGEPAGE)
    int numa_node = -1;                                // Linux + -DHFT_MEM_NUMA=1: mbind(MPOL_BIND)
};

// ---------------------------------------------------------------------------------------------
// Layer 1: the region. ONE mapping, carved front to back. Deallocation inside the region is a
// no-op (the monotonic and pool layers above never give memory back early anyway).
class region_resource final : public std::pmr::memory_resource {
public:
    region_resource(const arena_config& cfg)
        : cap_(round_to_page(cfg.region_bytes)),
          overflow_(cfg.tripwire ? std::pmr::null_memory_resource()
                                 : std::pmr::new_delete_resource()) {
        void* p = ::mmap(nullptr, cap_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) throw std::bad_alloc();
        base_ = static_cast<std::byte*>(p);
#ifdef __linux__
        // NOT executed on the authoring machine. man7.org/linux/man-pages/man2/madvise.2.html
        if (cfg.huge_pages) ::madvise(base_, cap_, MADV_HUGEPAGE);
#  ifdef HFT_MEM_HAVE_NUMAIF
        // NOT executed. man7.org/linux/man-pages/man2/mbind.2.html — before the first touch,
        // because the policy "has an effect only for new allocations". The page says nodemask
        // holds "up to maxnode bits" and "the kernel will use bits only up to maxnode". We pass a
        // 128-bit mask with maxnode = 128 and accept nodes 0..126 only, so the requested bit is
        // inside the mask whether "up to" is read inclusively or exclusively, and the shift below
        // is never by >= the width of unsigned long (that shift is undefined behaviour).
        if (cfg.numa_node >= 0) {
            constexpr int kBits = static_cast<int>(sizeof(unsigned long) * 8);
            unsigned long mask[2] = {0, 0};
            if (cfg.numa_node > 2 * kBits - 2) {
                std::fprintf(stderr, "hft: numa_node %d out of range (0..%d); not bound\n",
                             cfg.numa_node, 2 * kBits - 2);
            } else {
                mask[cfg.numa_node / kBits] = 1UL << (cfg.numa_node % kBits);
                if (::mbind(base_, cap_, MPOL_BIND, mask, sizeof(mask) * 8, 0) == 0) numa_bound_ = true;
                else std::perror("hft: mbind");  // carry on unbound; check numa_bound()
            }
        }
#  endif
#endif
#ifndef HFT_MEM_HAVE_NUMAIF
        if (cfg.numa_node >= 0)
            std::fprintf(stderr, "hft: numa_node ignored (build on Linux with -DHFT_MEM_NUMA=1 -lnuma)\n");
#endif
        prefault();  // first touch happens HERE, on the constructing thread, not on the hot path
        if (cfg.lock_pages) {
            if (::mlock(base_, cap_) == 0) locked_ = true;
            else std::perror("hft: mlock");  // carry on unlocked; check locked()
        }
    }
    ~region_resource() override { ::munmap(base_, cap_); }
    region_resource(const region_resource&) = delete;
    region_resource& operator=(const region_resource&) = delete;

    bool owns(const void* p) const noexcept {
        auto a = reinterpret_cast<std::uintptr_t>(p), b = reinterpret_cast<std::uintptr_t>(base_);
        return a >= b && a < b + cap_;
    }
    // True if p lies inside a block this region handed out from its OVERFLOW upstream (after the
    // region filled up). Slow path: a linear scan over the (few) live overflow blocks.
    bool in_overflow(const void* p) const noexcept {
        auto a = reinterpret_cast<std::uintptr_t>(p);
        for (const auto& [start, len] : overflow_blocks_)
            if (a == start || (a > start && a - start < len)) return true;  // a == start: len-0 block
        return false;
    }
    std::size_t used() const noexcept { return off_; }
    std::size_t capacity() const noexcept { return cap_; }
    std::size_t overflow_count() const noexcept { return overflows_; }
    bool locked() const noexcept { return locked_; }          // mlock succeeded
    bool numa_bound() const noexcept { return numa_bound_; }  // mbind succeeded (Linux only)

private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        if (!std::has_single_bit(align)) detail::die("alignment is not a power of two");
        // Align the ADDRESS base_ + off_, not the offset: mmap only promises page alignment, so
        // an offset rounded to `align` is not an aligned address when align > page size.
        // Every comparison is written so that nothing can wrap around (bytes may be huge).
        const std::uintptr_t b = reinterpret_cast<std::uintptr_t>(base_);
        const std::uintptr_t cur = b + off_;
        const std::uintptr_t mask = static_cast<std::uintptr_t>(align) - 1;
        if (cur <= UINTPTR_MAX - mask) {
            const std::size_t start = static_cast<std::size_t>(((cur + mask) & ~mask) - b);
            if (start < cap_ && bytes <= cap_ - start) {  // start == cap_ would hand out one-past-the-end
                off_ = start + bytes;
                return base_ + start;
            }
        }
        // Region full: ask the overflow upstream FIRST (a tripwire/null upstream throws here with
        // nothing recorded and no heap touched), then remember the block so the checked
        // deallocate can still tell "overflow memory of this arena" from "someone else's".
        void* p = overflow_->allocate(bytes, align);
        try {
            overflow_blocks_.emplace_back(reinterpret_cast<std::uintptr_t>(p), bytes);
        } catch (...) {
            overflow_->deallocate(p, bytes, align);  // bookkeeping failed: give the block back
            throw;
        }
        ++overflows_;
        return p;
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        if (owns(p)) return;  // region memory: freed when the region dies
        auto a = reinterpret_cast<std::uintptr_t>(p);
        for (auto& blk : overflow_blocks_)
            if (blk.first == a) { blk = overflow_blocks_.back(); overflow_blocks_.pop_back(); break; }
        overflow_->deallocate(p, bytes, align);  // give overflow memory back
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;  // only THIS region can free its own memory
    }
    static std::size_t round_to_page(std::size_t n) {
        if (n == 0) throw std::invalid_argument("hft_mem: arena_config::region_bytes must be > 0");
        const std::size_t pg = detail::page_size();
        if (n > SIZE_MAX - (pg - 1)) throw std::bad_alloc();  // n + pg - 1 would wrap
        return (n + pg - 1) / pg * pg;
    }
    void prefault() noexcept {
        const std::size_t pg = detail::page_size();
        for (std::size_t i = 0; i < cap_; i += pg)
            reinterpret_cast<volatile std::byte*>(base_)[i] = std::byte{0};
    }

    std::byte* base_ = nullptr;
    std::size_t cap_;
    std::size_t off_ = 0;
    std::size_t overflows_ = 0;
    std::pmr::memory_resource* overflow_;
    std::vector<std::pair<std::uintptr_t, std::size_t>> overflow_blocks_;  // live overflow blocks
    bool locked_ = false;
    bool numa_bound_ = false;
};

// ---------------------------------------------------------------------------------------------
// The gate: a pass-through between the pool and the monotonic layer. Once sealed, every request
// for NEW memory (a pool refill, or a big block such as a flat container growing) throws
// std::bad_alloc, while the pool keeps recycling blocks it already has. This is the tripwire
// that turns "the hot path asked for fresh memory" into an immediate failure in test.
class gate_resource final : public std::pmr::memory_resource {
public:
    // A null upstream behaves like null_memory_resource() (every request throws), not a crash.
    explicit gate_resource(std::pmr::memory_resource* up) noexcept
        : up_(up ? up : std::pmr::null_memory_resource()) {}
    void seal() noexcept { sealed_ = true; }
    void unseal() noexcept { sealed_ = false; }
    bool sealed() const noexcept { return sealed_; }
private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        if (sealed_) return std::pmr::null_memory_resource()->allocate(bytes, align);  // throws
        return up_->allocate(bytes, align);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        up_->deallocate(p, bytes, align);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
    std::pmr::memory_resource* up_;
    bool sealed_ = false;
};

// ---------------------------------------------------------------------------------------------
// Layer 3: the pool. Same as std::pmr::unsynchronized_pool_resource, plus two members that make a
// QUALIFIED call to its do_allocate/do_deallocate. A qualified call is never virtually dispatched,
// so hft::allocator reaches the pool with a direct call (check: objdump, lesson 03).
class pool_resource final : public std::pmr::unsynchronized_pool_resource {
public:
    using std::pmr::unsynchronized_pool_resource::unsynchronized_pool_resource;
    void* allocate_direct(std::size_t bytes, std::size_t align) {
        return unsynchronized_pool_resource::do_allocate(bytes, align);
    }
    void deallocate_direct(void* p, std::size_t bytes, std::size_t align) noexcept {
        unsynchronized_pool_resource::do_deallocate(p, bytes, align);
    }
};

// ---------------------------------------------------------------------------------------------
// The per-thread hierarchy. Members are destroyed in REVERSE declaration order:
// pool_ first, then gate_, mono_, region_ — the only safe teardown order.
//
// SINGLE-THREAD object. Every layer below is unsynchronized; the arena belongs to the thread that
// constructed it (owner()). Nesting on one thread is allowed: the newest arena becomes current()
// and the previous one is restored when it dies. Containers must not cross arenas: a container
// built under arena A and freed while arena B is current aborts in the checked build. If arenas on
// one thread die out of LIFO order (unique_ptr/optional), the chain is spliced so current() never
// points at a destroyed arena. Destroying an arena on a thread other than its owner aborts: the
// owner's thread_local pointer cannot be repaired from another thread.
class thread_arena {
public:
    explicit thread_arena(const arena_config& cfg = {})
        : region_(cfg),
          mono_(cfg.first_chunk, &region_),
          gate_(&mono_),
          pool_(std::pmr::pool_options{cfg.max_blocks_per_chunk, 0}, &gate_),
          previous_(current_),
          owner_(std::this_thread::get_id()) {
        current_ = this;
    }
    ~thread_arena() {
        if (owner_ != std::this_thread::get_id())
            detail::die("thread_arena destroyed on a thread other than the one that created it");
        if (current_ == this) {
            current_ = previous_;
        } else {  // out-of-LIFO destruction: unlink this arena from the chain of newer ones
            for (thread_arena* x = current_; x; x = x->previous_)
                if (x->previous_ == this) { x->previous_ = previous_; break; }
        }
    }
    thread_arena(const thread_arena&) = delete;
    thread_arena& operator=(const thread_arena&) = delete;

    static thread_arena* current() noexcept { return current_; }

    // For explicit (stateful) pmr containers: std::pmr::map<K,V> m{arena.resource()};
    std::pmr::memory_resource* resource() noexcept { return &pool_; }
    std::pmr::memory_resource* bulk() noexcept { return &gate_; }  // big reserves: flat containers
    region_resource& region() noexcept { return region_; }
    void seal() noexcept { gate_.seal(); }     // after warm-up: any NEW memory request throws
    void unseal() noexcept { gate_.unseal(); }
    bool sealed() const noexcept { return gate_.sealed(); }

    // Pre-carve `count` blocks of `bytes` into the pool's free list (allocate all, free all),
    // so the hot path never pays a pool refill. Call at start-up, before seal().
    // The blocks are remembered by threading a link THROUGH each held block (memcpy, so any
    // alignment works), so warm() spends none of the finite region on its own bookkeeping.
    // Blocks smaller than a pointer fall back to a temporary heap vector (start-up only).
    // Blocks are freed in allocation order, so the pool hands them out last-allocated first.
    void warm(std::size_t bytes, std::size_t count, std::size_t align = alignof(std::max_align_t)) {
        if (count == 0) return;
        if (bytes < sizeof(void*)) {
            std::vector<void*> tmp;
            tmp.reserve(count);
            for (std::size_t i = 0; i < count; ++i) tmp.push_back(pool_.allocate_direct(bytes, align));
            for (void* p : tmp) pool_.deallocate_direct(p, bytes, align);
            return;
        }
        auto next_of = [](void* p) { void* n; std::memcpy(&n, p, sizeof n); return n; };
        auto set_next = [](void* p, void* n) { std::memcpy(p, &n, sizeof n); };
        void* first = nullptr;
        void* last = nullptr;
        auto give_back = [&] {
            for (void* p = first; p;) { void* n = next_of(p); pool_.deallocate_direct(p, bytes, align); p = n; }
        };
        try {
            for (std::size_t i = 0; i < count; ++i) {
                void* p = pool_.allocate_direct(bytes, align);
                set_next(p, nullptr);
                if (last) set_next(last, p); else first = p;
                last = p;
            }
        } catch (...) { give_back(); throw; }
        give_back();
    }
    std::thread::id owner() const noexcept { return owner_; }

    // Called by hft::allocator: direct (non-virtual) calls into the pool layer.
    void* allocate(std::size_t bytes, std::size_t align) { return pool_.allocate_direct(bytes, align); }
    void deallocate(void* p, std::size_t bytes, std::size_t align) noexcept {
        pool_.deallocate_direct(p, bytes, align);
    }

private:
    region_resource region_;
    std::pmr::monotonic_buffer_resource mono_;
    gate_resource gate_;
    pool_resource pool_;
    thread_arena* previous_;
    std::thread::id owner_;
    static inline thread_local thread_arena* current_ = nullptr;
};

namespace detail {
inline thread_arena& arena_or_throw() {
    thread_arena* a = thread_arena::current();
    if (!a) throw std::logic_error("hft_mem: no thread_arena on this thread");
    return *a;
}
}  // namespace detail

// ---------------------------------------------------------------------------------------------
// The stateless allocator: zero bytes, default-constructible, finds the CURRENT THREAD's arena.
//
// is_always_equal = true is an ASSUMPTION, not a fact: storage from one hft::allocator can be
// freed by another only if both run on the same thread under the same current() arena. The
// standard's equality requirement holds only under that invariant (one owning thread, containers
// never handed to another thread or across nested arenas). The checked build (HFT_MEM_CHECKED=1,
// default) enforces it at every deallocate and aborts when it is broken.
template <class T>
struct allocator {
    using value_type = T;
    using is_always_equal = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    allocator() noexcept = default;
    template <class U>
    allocator(const allocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > SIZE_MAX / sizeof(T)) throw std::bad_array_new_length();  // n * sizeof(T) would wrap
        return static_cast<T*>(detail::arena_or_throw().allocate(n * sizeof(T), alignof(T)));
    }
    void deallocate(T* p, std::size_t n) noexcept {
        thread_arena* a = thread_arena::current();
        // Kept in every build (also HFT_MEM_CHECKED=0): one predictable branch instead of a null
        // dereference when a container outlives its arena or dies on an arena-less thread.
        if (!a) [[unlikely]]
            detail::die("deallocate with no thread_arena on this thread "
                        "(container outlived its arena, or was destroyed on another thread)");
#if HFT_MEM_CHECKED
        // Memory is this arena's if it is in the region OR in a block the region took from its
        // overflow upstream; the second test only runs off the fast path (owns() false).
        if (!a->region().owns(p) && !a->region().in_overflow(p))
            detail::die("deallocate of memory that is not from this thread's arena "
                        "(container moved to / destroyed on another thread?)");
#endif
        a->deallocate(p, n * sizeof(T), alignof(T));
    }
    template <class U>
    friend bool operator==(const allocator&, const allocator<U>&) noexcept { return true; }
};

// ---------------------------------------------------------------------------------------------
// The aliases application code types. Each just plugs hft::allocator into the right slot.
template <class T>
using vector = std::vector<T, allocator<T>>;
using string = std::basic_string<char, std::char_traits<char>, allocator<char>>;
template <class K, class V, class Cmp = std::less<K>>
using map = std::map<K, V, Cmp, allocator<std::pair<const K, V>>>;
template <class K, class V, class H = std::hash<K>, class Eq = std::equal_to<K>>
using unordered_map = std::unordered_map<K, V, H, Eq, allocator<std::pair<const K, V>>>;
template <class K, class V, class Cmp = std::less<K>>   // flat_map stores pair<K,V>, not pair<const K,V>
using flat_map = boost::container::flat_map<K, V, Cmp, allocator<std::pair<K, V>>>;
template <class K, class V, class H = boost::hash<K>, class Eq = std::equal_to<K>>
using unordered_flat_map = boost::unordered_flat_map<K, V, H, Eq, allocator<std::pair<const K, V>>>;
template <class K, class V, class H = boost::hash<K>, class Eq = std::equal_to<K>>
using unordered_node_map = boost::unordered_node_map<K, V, H, Eq, allocator<std::pair<const K, V>>>;
template <class Value, class IndexSpecifierList>  // the allocator is multi_index's LAST argument
using multi_index = boost::multi_index_container<Value, IndexSpecifierList, allocator<Value>>;

// ---------------------------------------------------------------------------------------------
// Per-message scratch: a fixed buffer, monotonic on top, null upstream. reset() once per message.
template <std::size_t Bytes>
class scratch {
public:
    std::pmr::memory_resource* resource() noexcept { return &mono_; }
    void reset() noexcept { mono_.release(); }
private:
    alignas(128) std::byte buf_[Bytes];  // 128: a whole line on Apple M-series (64 B on x86-64)
    std::pmr::monotonic_buffer_resource mono_{buf_, Bytes, std::pmr::null_memory_resource()};
};

// Pin the calling thread to one CPU BEFORE constructing its thread_arena (first touch decides
// the NUMA node). Linux only; NOT executed on the authoring machine.
// man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html
inline bool pin_this_thread(int cpu) noexcept {
#ifdef __linux__
    // CPU_SET(3): CPU_SETSIZE "specifies a value one greater than the maximum CPU number".
    if (cpu < 0 || cpu >= CPU_SETSIZE) return false;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return ::pthread_setaffinity_np(::pthread_self(), sizeof(set), &set) == 0;
#else
    (void)cpu;
    return false;  // this library implements pinning on Linux only
#endif
}

}  // namespace hft
