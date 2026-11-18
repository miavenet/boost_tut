// test_hft_mem.cpp — regression tests for labs/hft_mem/hft_mem.hpp (one mode per bug fixed). Run all of them with:  bash tests/run.sh
// Each mode exits 0 on the fixed library; "death" modes must abort (exit 134) with a message.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include -Ilabs tests/test_hft_mem.cpp
//        (and again with -DHFT_MEM_CHECKED=0 for the mode `unchecked-null`).
#include "hft_mem/hft_mem.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <future>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
std::size_t g_news = 0;  // global operator new calls (see the replacement below), for the tripwire test
}
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {
constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
void* opaque(void* p) { void* volatile v = p; return v; }  // keep the optimiser from folding tests
int fail(const char* why) { std::printf("FAIL: %s\n", why); return 1; }

// region_resource::do_allocate: `start + bytes > cap_` wrapped for huge `bytes`.
int region_overflow() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 20; cfg.tripwire = true;
    hft::region_resource r(cfg);
    (void)r.allocate(16, 8);  // off_ > 8, so start + (SIZE_MAX - 8) wraps to a small number
    try { void* p = r.allocate(kMax - 8, 8); std::printf("returned %p\n", p); return fail("huge request did not throw"); }
    catch (const std::bad_alloc&) {}
    std::puts("ok: region_resource::allocate(SIZE_MAX-8) throws std::bad_alloc");
    return 0;
}

// hft::allocator<T>::allocate: `n * sizeof(T)` wrapped.
int alloc_overflow() {
    hft::thread_arena arena;
    hft::allocator<long> a;
    try { long* p = a.allocate(kMax / sizeof(long) + 1); std::printf("returned %p\n", static_cast<void*>(p)); return fail("no throw"); }
    catch (const std::bad_array_new_length&) {}
    std::puts("ok: allocator<long>::allocate(SIZE_MAX/8+1) throws std::bad_array_new_length");
    return 0;
}

// region_resource aligned the OFFSET, so align > page size was not honoured. mmap's base is
// only page aligned; asking for 2^15 .. 2^23 makes a misaligned result near-certain if broken.
int overalign() {
    hft::arena_config cfg; cfg.region_bytes = std::size_t{64} << 20; cfg.tripwire = true;
    hft::region_resource r(cfg);
    for (int k = 15; k <= 23; ++k) {
        const std::size_t al = std::size_t{1} << k;
        (void)r.allocate(1, 1);
        auto addr = reinterpret_cast<std::uintptr_t>(opaque(r.allocate(64, al)));
        std::printf("align 2^%d: addr %% align = %zu\n", k, static_cast<std::size_t>(addr % al));
        if (addr % al) return fail("misaligned");
    }
    std::puts("ok: every over-page alignment honoured");
    return 0;
}

// A non-power-of-two alignment is a precondition violation: abort with a message (death test).
int bad_align() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 20;
    hft::region_resource r(cfg);
    volatile std::size_t three = 3;  // run-time value: the compiler would warn on a literal 3
    (void)r.allocate(8, three);
    return fail("alignment 3 accepted");
}

// Checked build: once the region overflowed, ANY foreign pointer used to pass the ownership
// check. Thread 2 keeps its arena alive while main frees thread 2's map: must abort (death test).
int cross_thread_after_overflow() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 16; cfg.first_chunk = 4096;
    hft::thread_arena a1(cfg);
    hft::vector<char> big; big.reserve(std::size_t{1} << 20);  // > region: forces overflow
    std::printf("main arena overflow_count=%zu\n", a1.region().overflow_count());
    if (a1.region().overflow_count() == 0) return fail("setup: no overflow");
    std::promise<hft::map<long, long>*> made; std::promise<void> done;
    std::thread t2([&] {
        hft::thread_arena a2;
        auto* m = new hft::map<long, long>();
        (*m)[42] = 99;
        made.set_value(m);
        done.get_future().wait();  // a2 (and its region) stays alive while main frees the map
    });
    hft::map<long, long>* foreign = made.get_future().get();
    std::fflush(stdout);
    delete foreign;  // must abort here: thread 2's nodes are not in main's region or overflow
    done.set_value(); t2.join();
    return fail("cross-thread free after overflow was not caught");
}

// ...and overflow memory of THIS arena must still be accepted (no false abort).
int own_overflow_ok() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 16; cfg.first_chunk = 4096;
    hft::thread_arena a1(cfg);
    {
        hft::map<long, long> m;
        for (long i = 0; i < 20000; ++i) m[i] = i;  // ~1 MB of nodes: most come from overflow
        hft::vector<long> v; v.reserve(100000);
#ifndef HFT_TEST_OLD_HEADER  // (old header, for the before-fix run: no in_overflow())
        if (!a1.region().in_overflow(v.data())) return fail("big block not recognised as overflow");
#endif
    }
    std::printf("ok: overflow_count=%zu, own overflow memory freed without abort\n", a1.region().overflow_count());
    return 0;
}

// Two arenas on one thread destroyed out of LIFO order: current() must never dangle.
int nested_out_of_order() {
    auto outer = std::make_unique<hft::thread_arena>();
    auto inner = std::make_unique<hft::thread_arena>();
    hft::thread_arena* in = inner.get();
    outer.reset();  // the OLDER arena dies first
    if (hft::thread_arena::current() != in) return fail("current() is not the surviving arena");
    inner.reset();
    if (hft::thread_arena::current() != nullptr) return fail("current() dangles after both died");
    std::puts("ok: out-of-order destruction leaves current() == nullptr");
    return 0;
}

// Destroying an arena on another thread cannot repair the owner's thread_local: abort (death test).
int foreign_destroy() {
    auto* a = new hft::thread_arena();
    std::thread([a] { delete a; }).join();
    return fail("arena destroyed on a foreign thread without abort");
}

// warm() used a pmr::vector on the monotonic layer, burning count * 8 bytes of the region.
// Same pool blocks with and without warm(): region use must be the same.
int warm_no_leak() {
    constexpr std::size_t N = 200'000, SZ = 64;
    std::size_t used_warm, used_plain;
    {
        hft::thread_arena a;
        a.warm(SZ, N);
        used_warm = a.region().used();
    }
    {
        hft::thread_arena a;
        hft::allocator<std::byte> al;
        std::vector<std::byte*> held(N);
        for (auto& p : held) p = al.allocate(SZ);
        used_plain = a.region().used();
        for (auto* p : held) al.deallocate(p, SZ);
    }
    std::printf("region used: warm(%zu x %zu B) = %zu KiB, same blocks allocated directly = %zu KiB\n",
                N, SZ, used_warm >> 10, used_plain >> 10);
    return used_warm <= used_plain ? (std::puts("ok: warm() spends no region on bookkeeping"), 0)
                                   : fail("warm() used more region than the blocks themselves");
}

// region_bytes edge cases: 0 is rejected clearly; a size that would wrap when rounded throws.
int region_size_edges() {
    hft::arena_config cfg; cfg.region_bytes = 0;
    try { hft::region_resource r(cfg); return fail("region_bytes=0 accepted"); }
    catch (const std::invalid_argument& e) { std::printf("ok: region_bytes=0 -> invalid_argument: %s\n", e.what()); }
    cfg.region_bytes = kMax - 1;
    try { hft::region_resource r(cfg); return fail("region_bytes=SIZE_MAX-1 accepted"); }
    catch (const std::bad_alloc&) { std::puts("ok: region_bytes=SIZE_MAX-1 -> bad_alloc"); }
    return 0;
}

// gate_resource with a null upstream used to dereference it; now it behaves like the null resource.
int gate_null() {
    hft::gate_resource g(nullptr);
    try { (void)g.allocate(64); return fail("allocate on null upstream did not throw"); }
    catch (const std::bad_alloc&) {}
    std::puts("ok: gate_resource(nullptr).allocate throws std::bad_alloc");
    return 0;
}

int noexcept_dealloc() {
    hft::pool_resource p;
    if (!noexcept(p.deallocate_direct(nullptr, 8, 8))) return fail("deallocate_direct is not noexcept");
    std::puts("ok: pool_resource::deallocate_direct is noexcept");
    return 0;
}

// pin_this_thread: out-of-range CPUs are rejected before CPU_SET (on macOS it is always false).
int pin_range() {
    if (hft::pin_this_thread(-1) || hft::pin_this_thread(1 << 20)) return fail("out-of-range cpu accepted");
    std::printf("ok: pin_this_thread(-1) and (1<<20) return false; pin_this_thread(0) = %s\n",
                hft::pin_this_thread(0) ? "true" : "false (not Linux, or not allowed)");
    return 0;
}

// lock_pages: the outcome of mlock is now observable instead of only printed.
int mlock_status() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 20; cfg.lock_pages = true;
    hft::region_resource r(cfg);
#ifdef HFT_TEST_OLD_HEADER
    return fail("old header has no locked()/numa_bound(): mlock failure was only printed");
#else
    std::printf("ok: lock_pages=true -> locked() = %s, numa_bound() = %s\n",
                r.locked() ? "true" : "false", r.numa_bound() ? "true" : "false");
#endif
    return 0;
}

// Built with -DHFT_MEM_CHECKED=0: a container that outlives its arena used to null-dereference
// (SIGSEGV, exit 139). Now it aborts with a message (exit 134) in every build (death test).
int unchecked_null() {
    std::optional<hft::vector<int>> v;
    {
        hft::thread_arena a;
        v.emplace();
        v->push_back(1);
    }
    v.reset();
    return fail("no abort");
}

// Exhausted tripwire region: the overflow bookkeeping used to run BEFORE the upstream call, so it
// heap-allocated (operator new) and left overflow_count() == 1 after the bad_alloc.
int tripwire_no_bookkeeping() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 16; cfg.tripwire = true;
    hft::region_resource r(cfg);
    (void)r.allocate(r.capacity(), 1);  // region exactly full
    const std::size_t news0 = g_news;
    bool threw = false;
    try { (void)r.allocate(64, 8); } catch (const std::bad_alloc&) { threw = true; }
    const std::size_t news = g_news - news0;
    std::printf("threw=%d operator-new calls=%zu overflow_count=%zu\n", threw, news, r.overflow_count());
    if (!threw) return fail("exhausted tripwire region did not throw");
    if (news != 0) return fail("failed overflow allocation called operator new");
    if (r.overflow_count() != 0) return fail("overflow_count() incremented by a failed allocation");
    std::puts("ok: failed tripwire allocation: 0 operator new calls, overflow_count() == 0");
    return 0;
}

// Zero-byte requests at capacity used to return base_ + cap_ (one past the region, not owned), and
// in_overflow() could not match a len == 0 block. Also the exact test the checked allocator does.
int zero_byte_edges() {
    hft::arena_config cfg; cfg.region_bytes = 1 << 16;  // not tripwire: overflow -> new/delete
    hft::region_resource r(cfg);
    (void)r.allocate(r.capacity(), 1);  // off_ == cap_
    void* p = r.allocate(0, 1);
    std::printf("zero-byte at capacity: owns=%d in_overflow=%d overflow_count=%zu\n",
                r.owns(p), r.in_overflow(p), r.overflow_count());
    if (!(r.owns(p) || r.in_overflow(p))) return fail("zero-byte block at capacity is neither owned nor tracked (checked deallocate would abort)");
    r.deallocate(p, 0, 1);
    if (r.in_overflow(p) && !r.owns(p)) return fail("zero-byte block still tracked after deallocate");
    // a zero-byte block that really came from the overflow upstream: tracked, and freed cleanly
    void* q = r.allocate(0, 64);
    if (!(r.owns(q) || r.in_overflow(q))) return fail("zero-byte aligned block not tracked");
    r.deallocate(q, 0, 64);
    // same through a thread_arena and the allocator's checked test (owns || in_overflow)
    hft::thread_arena a(cfg);
    (void)a.region().allocate(a.region().capacity() - a.region().used(), 1);
    void* z = a.region().allocate(0, 1);
    if (!(a.region().owns(z) || a.region().in_overflow(z))) return fail("checked cross-check rejects zero-byte overflow block");
    a.region().deallocate(z, 0, 1);
    std::puts("ok: zero-byte blocks at capacity are owned/tracked and freed without UB");
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    const std::string_view m = argc > 1 ? argv[1] : "";
    struct { const char* name; int (*fn)(); } modes[] = {
        {"region-overflow", region_overflow}, {"alloc-overflow", alloc_overflow},
        {"overalign", overalign}, {"bad-align", bad_align},
        {"cross-thread-after-overflow", cross_thread_after_overflow}, {"own-overflow-ok", own_overflow_ok},
        {"nested-out-of-order", nested_out_of_order}, {"foreign-destroy", foreign_destroy},
        {"warm-no-leak", warm_no_leak}, {"region-size-edges", region_size_edges},
        {"gate-null", gate_null}, {"noexcept-dealloc", noexcept_dealloc},
        {"tripwire-no-bookkeeping", tripwire_no_bookkeeping}, {"zero-byte-edges", zero_byte_edges},
        {"pin-range", pin_range}, {"mlock-status", mlock_status}, {"unchecked-null", unchecked_null},
    };
    for (auto& x : modes)
        if (m == x.name) return x.fn();
    std::fprintf(stderr, "usage: %s <mode>; modes:", argv[0]);
    for (auto& x : modes) std::fprintf(stderr, " %s", x.name);
    std::fputc('\n', stderr);
    return 2;
}
