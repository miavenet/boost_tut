// test_intrusive.cpp — regression tests for labs/hft_mem/hft_slab.hpp, hft_book.hpp and
// hft_handroll.hpp (one mode per bug fixed). Run with tests/run.sh.
// Each mode exits 0 on the fixed library; "death" modes must abort (exit 134) with a message.
// -DHFT_TEST_OLD_HEADER (set by run.sh when HFT_INC is given) skips calls to API added by the fix
// so the same tests compile against the pre-fix headers and show each bug.
#include "hft_mem/hft_book.hpp"
#include "hft_mem/hft_handroll.hpp"
#include "hft_mem/hft_slab.hpp"

#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
int fail(const char* why) { std::printf("FAIL: %s\n", why); return 1; }

struct MayThrow {
    long x, y;
    explicit MayThrow(bool boom) : x(-1), y(-1) { if (boom) throw std::runtime_error("boom"); x = y = 1; }
};

// slab::try_create popped the slot and counted it live BEFORE the constructor ran.
int slab_ctor_throw() {
    hft::thread_arena arena;
    hft::slab<MayThrow> s(2);
    MayThrow* a = s.try_create(false);
    try { (void)s.try_create(true); return fail("no throw"); } catch (const std::runtime_error&) {}
    std::printf("after a throwing constructor: size=%u\n", s.size());
    if (s.size() != 1) return fail("throwing constructor leaked a slot");
    MayThrow* b = s.try_create(false);
    if (!b) return fail("second slot lost");
    s.destroy(a); s.destroy(b);
    std::puts("ok: slot and count restored; both slots usable");
    return 0;
}

// slab(cap, upstream): if the generation array's allocation threw, the slot array leaked.
struct counting_resource : std::pmr::memory_resource {
    int calls = 0, fail_at = 2; long outstanding = 0;
    void* do_allocate(std::size_t n, std::size_t al) override {
        if (++calls == fail_at) throw std::bad_alloc();
        outstanding += static_cast<long>(n);
        return std::pmr::new_delete_resource()->allocate(n, al);
    }
    void do_deallocate(void* p, std::size_t n, std::size_t al) override {
        outstanding -= static_cast<long>(n);
        std::pmr::new_delete_resource()->deallocate(p, n, al);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};
// Fixed by making ONE allocation (slots + generations): it either succeeds or leaves nothing.
int slab_alloc_fail() {
    counting_resource ok; ok.fail_at = 0;
    { hft::slab<long> s(1000, &ok); }
    std::printf("upstream allocations per slab: %d\n", ok.calls);
    for (int k = 1; k <= 2; ++k) {
        counting_resource r; r.fail_at = k;
        try { hft::slab<long> s(1000, &r); if (k == 1) return fail("no throw"); } catch (const std::bad_alloc&) {}
        std::printf("allocation %d throws: %ld bytes still allocated\n", k, r.outstanding);
        if (r.outstanding != 0) return fail("leaked");
    }
    return ok.calls == 1 ? 0 : fail("more than one upstream allocation (a later one can throw and leak the first)");
}

// destroy() twice: owns() only checked the address, so the second call corrupted the free list.
int slab_double_destroy() {
    hft::thread_arena arena;
    hft::slab<long> s(4);
    long* p = s.create(10);
    s.destroy(p);
    s.destroy(p);  // must abort here (checked build)
    std::printf("survived: live=%u\n", s.size());
    long* a = s.create(1); long* b = s.create(2);
    std::printf("a == b: %d\n", a == b);
    return fail("double destroy not caught");
}

// get() of a default/forged handle {i, 0} returned a pointer into a FREE slot.
int slab_forged_handle() {
    hft::thread_arena arena;
    hft::slab<long> s(4);
    hft::slab<long>::handle forged{0, 0};
    if (s.get(forged)) return fail("forged handle to a never-used slot accepted");
    long* p = s.create(7);
    auto h = s.handle_of(p);
    if (s.get(h) != p) return fail("live handle rejected");
    s.destroy(p);
    if (s.get(h)) return fail("stale handle accepted");
    long* q = s.create(8);  // LIFO: same slot
    if (q != p || s.get(h)) return fail("stale handle accepted after reuse");
    hft::slab<long>::handle h2 = s.handle_of(q);
    h2.gen += 1;  // the slot's next (free) generation
    if (s.get(h2)) return fail("handle with a free generation accepted");
    s.destroy(q);
    std::puts("ok: forged, stale and reused-slot handles all rejected");
    return 0;
}

// handle_of()/index_of() of a foreign pointer: no check in checked builds (read out of bounds).
int slab_handle_foreign() {
    hft::thread_arena arena;
    hft::slab<long> s(4);
    long local = 0;
    auto h = s.handle_of(&local);  // must abort here
    std::printf("handle {%u, %u}\n", h.index, h.gen);
    return fail("foreign handle_of not caught");
}
int slab_at_range() {
    hft::thread_arena arena;
    hft::slab<long> s(4);
    long* p = s.at(4);  // one past the end: must abort here
    std::printf("at(4) = %p\n", static_cast<void*>(p));
    return fail("at() out of range not caught");
}

// CRITICAL-1: book<flat> reserved its id map for 1 x capacity. Fill to capacity with random
// (exchange-assigned) ids, seal the arena, then churn at constant size: any reallocation of the
// id map throws std::bad_alloc under seal().
int book_churn_full() {
    constexpr std::size_t CAP = 50'000;
    constexpr int OPS = 200'000;
    hft::arena_config cfg; cfg.region_bytes = std::size_t{64} << 20;
    hft::thread_arena arena(cfg);
    hft::book::book<hft::book::IdIndex::flat> b(CAP, 64, 8);
    std::uint64_t x = 88172645463325252ull;
    auto rnd = [&] { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; };
    std::vector<long> live(CAP);
    for (auto& id : live) {
        do { id = static_cast<long>(rnd() >> 1); } while (!b.add(id, int(id % 64), int(id % 8), 1));
    }
#ifndef HFT_TEST_OLD_HEADER
    const std::size_t bc0 = b.id_bucket_count();
#endif
    std::printf("filled: %u live of capacity %zu\n", b.size(), CAP);
    arena.seal();
    int ops = 0;
    try {
        for (; ops < OPS; ++ops) {
            std::size_t j = rnd() % CAP;
            if (!b.cancel(live[j])) return fail("cancel of a live id failed");
            long id;
            do { id = static_cast<long>(rnd() >> 1); } while (!b.add(id, int(id % 64), int(id % 8), 1));
            live[j] = id;
        }
    } catch (const std::bad_alloc&) {
        std::printf("std::bad_alloc under seal() at churn op %d: the id map reallocated\n", ops);
        arena.unseal();
        return 1;
    }
    arena.unseal();
#ifndef HFT_TEST_OLD_HEADER
    std::printf("id map buckets: %zu before churn, %zu after\n", bc0, b.id_bucket_count());
    if (b.id_bucket_count() != bc0) return fail("rehashed");
#endif
    std::printf("ok: %d cancel+add at %u/%zu live, sealed arena, no reallocation\n", ops, b.size(), CAP);
    return 0;
}

// add() ignored ids_.emplace(...).second: a duplicate id left an orphan order in every list.
int book_dup_id() {
    hft::thread_arena arena;
    hft::book::book<hft::book::IdIndex::flat> b(16, 4, 2);
    b.add(5, 1, 0, 10);
    hft::book::Order* d = b.add(5, 2, 1, 20);
    std::printf("duplicate add returned %s, size=%u\n", d ? "an order" : "nullptr", b.size());
    if (d || b.size() != 1) return fail("duplicate id accepted");
    b.cancel(5);
    return b.size() == 0 ? 0 : fail("orphan left");
}

// add() indexed levels_[px] / clients_[client] unchecked.
int book_bad_px() {
    hft::thread_arena arena;
    hft::book::book<hft::book::IdIndex::flat> b(16, 4, 2);
    bool bad = b.add(1, 4, 0, 1) || b.add(2, -1, 0, 1) || b.add(3, 0, 2, 1) || b.add(4, 0, -1, 1);
    std::printf("out-of-range px/client accepted? %d, size=%u\n", bad, b.size());
    return (!bad && b.size() == 0) ? 0 : fail("out-of-range price or client accepted");
}

// mark_sent/mark_acked on an order not in that state: erase of an unlinked hook (null deref,
// or a Boost assert with asserts on). Now: no change.
int book_mark_state() {
    hft::thread_arena arena;
    hft::book::book<hft::book::IdIndex::flat> b(16, 4, 2);
    hft::book::Order* o = b.add(1, 1, 0, 1);
    b.mark_acked(*o);  // not pending ack
    b.mark_sent(*o);
    b.mark_sent(*o);   // already sent
    std::printf("pending-send %d pending-ack %d, queues %zu/%zu\n", b.is_pending_send(*o), b.is_pending_ack(*o),
                b.send_queue().size(), b.ack_queue().size());
    if (b.is_pending_send(*o) || !b.is_pending_ack(*o) || b.ack_queue().size() != 1) return fail("state broken");
    b.mark_acked(*o);
    b.mark_acked(*o);
    return b.ack_queue().empty() ? 0 : fail("ack queue");
}

// hr::pool::create popped the slot before the constructor ran.
int hr_pool_ctor_throw() {
    hr::pool<MayThrow> p(1);
    try { (void)p.create(true); return fail("no throw"); } catch (const std::runtime_error&) {}
    MayThrow* a = p.create(false);
    std::printf("after a throwing constructor, create() = %s\n", a ? "a slot" : "nullptr");
    if (!a) return fail("slot leaked");
    p.destroy(a);
    return 0;
}

// hr::pool / hash_index / idx_list were copyable (copies kept pointers into the source).
int hr_nocopy() {
    struct O { long id; hr::ilink h; hr::hlink hh; };
    using IL = hr::idx_list<O, &O::h>;
    using HI = hr::hash_index<O, long, &O::id, &O::hh, std::hash<long>>;
    int bad = 0;
    if (std::is_copy_constructible_v<hr::pool<long>> || std::is_copy_assignable_v<hr::pool<long>>) { std::puts("hr::pool copyable"); bad = 1; }
    if (std::is_copy_constructible_v<HI> || std::is_copy_assignable_v<HI> || std::is_move_constructible_v<HI>) { std::puts("hr::hash_index copyable/movable"); bad = 1; }
    if (std::is_copy_constructible_v<IL> || std::is_copy_assignable_v<IL>) { std::puts("hr::idx_list copyable"); bad = 1; }
    // idx_list may MOVE (no link points at its header): the source must be left empty.
    std::vector<O> v(4);
    IL a(v.data());
    a.push_back(0); a.push_back(1);
    IL b(static_cast<IL&&>(a));
    if (!a.empty() || b.front() != 0 || b.next(0) != 1) { std::puts("idx_list move did not transfer"); bad = 1; }
    if (!bad) std::puts("ok: pool, hash_index non-copyable; idx_list move-only and leaves the source empty");
    return bad;
}

// dlink/hlink copies cloned live links (a copy of a linked object looked linked into the list).
int hr_link_copy() {
    struct Item { long id; hr::dlink h; };
    hr::dlist<Item, &Item::h> l;
    Item a{1, {}}, b{2, {}};
    l.push_back(a); l.push_back(b);
    Item c = a;                          // copy of a linked object
    Item d{4, {}}; d = b;                // assignment onto an unlinked object
    b = Item{9, {}};                     // assignment onto a linked object keeps its links
    std::printf("copy linked? %d  assigned-to linked? %d  b still linked? %d\n", c.h.is_linked(), d.h.is_linked(), b.h.is_linked());
    int bad = c.h.is_linked() || d.h.is_linked() || !b.h.is_linked();
    int n = 0; l.for_each([&](Item&) { ++n; });
    if (n != 2) bad = 1;
    struct H { long id; hr::hlink hh; };
    hr::hash_index<H, long, &H::id, &H::hh, std::hash<long>> ix(8);
    H x{3, {}};
    ix.insert(x);
    H y = x;
    if (y.hh.is_linked()) { std::puts("hlink copy linked"); bad = 1; }
    hr::hash_index<H, long, &H::id, &H::hh, std::hash<long>>::erase(x);
    l.clear();
    if (!bad) std::puts("ok: copies are unlinked, assignment keeps the target's links");
    return bad;
}

// hash_index(0) underflowed the mask; a non-power-of-two count broke `& mask_` silently.
int hr_hash_buckets() {
    struct H { long id; hr::hlink hh; };
    using HI = hr::hash_index<H, long, &H::id, &H::hh, std::hash<long>>;
    int rejected = 0;
    for (std::size_t n : {std::size_t{0}, std::size_t{3}, std::size_t{100}}) {
        try { HI h(n); } catch (const std::invalid_argument&) { ++rejected; }
    }
    HI ok(64);
    std::printf("bad bucket counts rejected: %d of 3\n", rejected);
    return rejected == 3 ? 0 : fail("bad bucket count accepted");
}
}  // namespace

int main(int argc, char** argv) {
    const std::string_view m = argc > 1 ? argv[1] : "";
    struct { const char* name; int (*fn)(); } modes[] = {
        {"slab-ctor-throw", slab_ctor_throw}, {"slab-alloc-fail", slab_alloc_fail},
        {"slab-double-destroy", slab_double_destroy}, {"slab-forged-handle", slab_forged_handle},
        {"slab-handle-foreign", slab_handle_foreign}, {"slab-at-range", slab_at_range},
        {"book-churn-full", book_churn_full}, {"book-dup-id", book_dup_id}, {"book-bad-px", book_bad_px},
        {"book-mark-state", book_mark_state}, {"hr-pool-ctor-throw", hr_pool_ctor_throw},
        {"hr-nocopy", hr_nocopy}, {"hr-link-copy", hr_link_copy}, {"hr-hash-buckets", hr_hash_buckets},
    };
    for (auto& x : modes)
        if (m == x.name) return x.fn();
    std::fprintf(stderr, "usage: %s <mode>; modes:", argv[0]);
    for (auto& x : modes) std::fprintf(stderr, " %s", x.name);
    std::fputc('\n', stderr);
    return 2;
}
