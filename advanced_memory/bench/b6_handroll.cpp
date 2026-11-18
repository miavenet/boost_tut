// b6_handroll.cpp — Boost.Intrusive vs the minimum hand-rolled version, component by component,
// on the same workload. p50/p99/p99.9/max per operation (ns).
// Release build: NDEBUG and HFT_MEM_CHECKED=0 are defined below, so neither side pays debug
// checks. Build with -DBENCH_ASSERTS to keep Boost's safe-mode asserts (labs/build.sh builds
// that variant as bench/build/b6_handroll_asserts).
// Usage: b6_handroll [group [variant]]   group = lists|ids|pools|floor; no argument = all but floor.
//   bench/run_intrusive.sh runs every variant in its OWN process: run in one process, the
//   first variant measured paid for warm-up the later ones did not (seen while authoring).
//
//   LISTS  50,000 objects in 1,024 lists; 400,000 ops: 45% push_back, 45% erase a random
//          linked object (given the object), 10% walk one random list.
//   IDS    50,000 live ids (random 63-bit); 400,000 ops: 50% find(random live id),
//          50% replace (erase a random live object, insert a new id).
//   POOLS  50,000 live objects of 64 B; 400,000 x (destroy a random live object, create one).
#ifndef BENCH_ASSERTS
#  define NDEBUG 1
#endif
#define HFT_MEM_CHECKED 0
#include "../labs/hft_mem/hft_handroll.hpp"
#include "../labs/hft_mem/hft_slab.hpp"
#include "bench_common.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <string_view>
#include <vector>

namespace bi = boost::intrusive;
using bench::recorder;

constexpr int N = 50'000, LISTS = 1024, OPS = 400'000;

// ================================ LISTS ========================================================
template <class Hook>
struct BObj { long id; long qty; Hook h; };

// A policy gives: obj type, list type, push(L&, obj&), erase(L&, obj&), walk(L&) -> sum.
template <class Hook, class... Opt>
struct boost_list {
    using obj = BObj<Hook>;
    using list = bi::list<obj, bi::member_hook<obj, Hook, &obj::h>, Opt...>;
    static void push(list& l, obj& o) { l.push_back(o); }
    static void erase(list& l, obj& o) { l.erase(list::s_iterator_to(o)); }
    static long walk(list& l) { long s = 0; for (auto& o : l) s += o.qty; return s; }
};
template <class Hook>
struct boost_slist {                                   // singly linked: erase needs the predecessor
    using obj = BObj<Hook>;
    using list = bi::slist<obj, bi::member_hook<obj, Hook, &obj::h>, bi::cache_last<true>>;
    static void push(list& l, obj& o) { l.push_back(o); }
    static void erase(list& l, obj& o) { l.erase(list::s_iterator_to(o)); }   // linear in position
    static long walk(list& l) { long s = 0; for (auto& o : l) s += o.qty; return s; }
};
struct hand_dlist {
    struct obj { long id; long qty; hr::dlink h; };
    using list = hr::dlist<obj, &obj::h>;
    static void push(list& l, obj& o) { l.push_back(o); }
    static void erase(list&, obj& o) { list::erase(o); }
    static long walk(list& l) { long s = 0; l.for_each([&](obj& o) { s += o.qty; }); return s; }
};

template <class P>
void list_bench(const char* name) {
    using obj = typename P::obj;
    std::vector<obj> objs(N);
    std::vector<typename P::list> lists(LISTS);
    std::vector<int> where(N, -1);                     // which list object i is in (-1 = none)
    bench::xorshift rng;
    for (int i = 0; i < N; ++i) {
        objs[i].id = i; objs[i].qty = 1;
        if (i % 2 == 0) { int l = static_cast<int>(rng() % LISTS); P::push(lists[l], objs[i]); where[i] = l; }
    }
    recorder rp(OPS), re(OPS), rw(OPS);
    for (recorder* r : {&rp, &re, &rw}) r->faults0 = bench::minor_faults();
    long sink = 0;
    for (int k = 0; k < OPS; ++k) {
        std::uint64_t x = rng();
        int i = static_cast<int>((x >> 8) % N);
        int kind = static_cast<int>(x % 100);
        if (kind < 90) {                               // push if not linked, else erase
            if (where[i] < 0) {
                int l = static_cast<int>((x >> 40) % LISTS);
                auto t0 = bench::now_ns(); P::push(lists[l], objs[i]); auto t1 = bench::now_ns();
                where[i] = l; rp.add(t1 - t0);
            } else {
                auto t0 = bench::now_ns(); P::erase(lists[where[i]], objs[i]); auto t1 = bench::now_ns();
                where[i] = -1; re.add(t1 - t0);
            }
        } else {
            auto& l = lists[(x >> 40) % LISTS];
            auto t0 = bench::now_ns(); sink += P::walk(l); auto t1 = bench::now_ns();
            rw.add(t1 - t0);
        }
    }
    bench::keep(sink);
    char b[96];
    std::snprintf(b, sizeof b, "%s push_back", name); rp.report(b);
    std::snprintf(b, sizeof b, "%s erase(obj)", name); re.report(b);
    std::snprintf(b, sizeof b, "%s walk", name); rw.report(b);
    for (int i = 0; i < N; ++i) if (where[i] >= 0) P::erase(lists[where[i]], objs[i]);
}

// index-linked list: same driver, written out because it is addressed by slot number
void idx_list_bench(const char* name) {
    struct obj { long id; long qty; hr::ilink h; };
    std::vector<obj> objs(N);
    std::vector<hr::idx_list<obj, &obj::h>> lists;   // non-copyable: built in place (moved on growth)
    lists.reserve(LISTS);
    for (int l = 0; l < LISTS; ++l) lists.emplace_back(objs.data());
    std::vector<int> where(N, -1);
    bench::xorshift rng;
    for (int i = 0; i < N; ++i) {
        objs[i].id = i; objs[i].qty = 1;
        if (i % 2 == 0) { int l = static_cast<int>(rng() % LISTS); lists[l].push_back(static_cast<std::uint32_t>(i)); where[i] = l; }
    }
    recorder rp(OPS), re(OPS), rw(OPS);
    for (recorder* r : {&rp, &re, &rw}) r->faults0 = bench::minor_faults();
    long sink = 0;
    for (int k = 0; k < OPS; ++k) {
        std::uint64_t x = rng();
        int i = static_cast<int>((x >> 8) % N);
        int kind = static_cast<int>(x % 100);
        if (kind < 90) {
            if (where[i] < 0) {
                int l = static_cast<int>((x >> 40) % LISTS);
                auto t0 = bench::now_ns(); lists[l].push_back(static_cast<std::uint32_t>(i)); auto t1 = bench::now_ns();
                where[i] = l; rp.add(t1 - t0);
            } else {
                auto t0 = bench::now_ns(); lists[where[i]].erase(static_cast<std::uint32_t>(i)); auto t1 = bench::now_ns();
                where[i] = -1; re.add(t1 - t0);
            }
        } else {
            auto& l = lists[(x >> 40) % LISTS];
            auto t0 = bench::now_ns();
            long s = 0;
            for (std::uint32_t j = l.front(); j != hr::nil; j = l.next(j)) s += objs[j].qty;
            auto t1 = bench::now_ns();
            sink += s; rw.add(t1 - t0);
        }
    }
    bench::keep(sink);
    char b[96];
    std::snprintf(b, sizeof b, "%s push_back", name); rp.report(b);
    std::snprintf(b, sizeof b, "%s erase(obj)", name); re.report(b);
    std::snprintf(b, sizeof b, "%s walk", name); rw.report(b);
}

// ================================ IDS ==========================================================
struct HObj {
    long id;
    long qty;
    hr::hlink hh;                                          // hand-rolled hash chain (16 B)
    bi::unordered_set_member_hook<bi::link_mode<bi::normal_link>> uh;   // 8 B
    bi::set_member_hook<bi::link_mode<bi::normal_link>, bi::optimize_size<true>> th;   // 24 B
};
struct hobj_id { using type = long; const long& operator()(const HObj& o) const noexcept { return o.id; } };
struct mix { std::size_t operator()(long k) const noexcept {   // the same hash for every hashed variant
    std::uint64_t x = static_cast<std::uint64_t>(k); x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; return x; } };
using BUSet = bi::unordered_set<HObj, bi::member_hook<HObj, decltype(HObj::uh), &HObj::uh>,
                                bi::key_of_value<hobj_id>, bi::hash<mix>, bi::power_2_buckets<true>,
                                bi::constant_time_size<true>>;
using BSet = bi::set<HObj, bi::member_hook<HObj, decltype(HObj::th), &HObj::th>, bi::key_of_value<hobj_id>>;
using HIdx = hr::hash_index<HObj, long, &HObj::id, &HObj::hh, mix>;

template <class Find, class Insert, class Erase>
void id_bench(const char* name, Find find, Insert insert, Erase erase) {
    hft::slab<HObj> pool(N + 1, std::pmr::new_delete_resource());
    std::vector<HObj*> live(N);
    bench::xorshift rng;
    for (int i = 0; i < N; ++i) { live[i] = pool.create(); live[i]->id = static_cast<long>(rng() >> 1); live[i]->qty = 1; insert(*live[i]); }
    recorder rf(OPS), rr(OPS);
    for (recorder* r : {&rf, &rr}) r->faults0 = bench::minor_faults();
    long sink = 0;
    for (int k = 0; k < OPS; ++k) {
        std::uint64_t x = rng();
        int j = static_cast<int>((x >> 8) % N);
        if (x & 1) {
            long id = live[j]->id;
            auto t0 = bench::now_ns(); HObj* o = find(id); auto t1 = bench::now_ns();
            sink += o->qty; rf.add(t1 - t0);
        } else {
            HObj* o = live[j];
            long nid = static_cast<long>(rng() >> 1);
            auto t0 = bench::now_ns();
            erase(*o);
            o->id = nid;                               // reuse the object: one erase + one insert
            insert(*o);
            auto t1 = bench::now_ns();
            rr.add(t1 - t0);
        }
    }
    bench::keep(sink);
    char b[96];
    std::snprintf(b, sizeof b, "%s find(id)", name); rf.report(b);
    std::snprintf(b, sizeof b, "%s erase+insert", name); rr.report(b);
    for (auto* o : live) pool.destroy(o);
}

// ================================ POOLS ========================================================
struct PObj { long id; long qty; char pad[48]; };   // 64 B

template <class Create, class Destroy>
void pool_bench(const char* name, Create create, Destroy destroy) {
    std::vector<PObj*> live(N);
    bench::xorshift rng;
    for (int i = 0; i < N; ++i) live[i] = create(i);
    recorder r(OPS);
    for (int k = 0; k < OPS; ++k) {
        int j = static_cast<int>(rng() % N);
        auto t0 = bench::now_ns();
        destroy(live[j]);
        live[j] = create(k);
        auto t1 = bench::now_ns();
        r.add(t1 - t0);
    }
    r.report(name);
    for (auto* p : live) destroy(p);
}

int main(int argc, char** argv) {
    std::string_view G = argc > 1 ? argv[1] : "", V = argc > 2 ? argv[2] : "";
    auto sel = [&](std::string_view g, std::string_view v) { return (G.empty() || G == g) && (V.empty() || V == v); };
    std::string_view only = G;
    if (G == "floor") {   // the cost of the measurement itself: two now_ns() calls around nothing
        recorder r(OPS);
        for (int k = 0; k < OPS; ++k) { auto t0 = bench::now_ns(); auto t1 = bench::now_ns(); r.add(t1 - t0); }
        r.report("timer floor (empty timed region)");
        return 0;
    }
    if (only.empty() || only == "lists") {
        std::puts("== LISTS: 50k objects, 1024 lists ==");
        using safe = bi::list_member_hook<>;
        using normal = bi::list_member_hook<bi::link_mode<bi::normal_link>>;
        if (sel("lists", "dlist")) list_bench<hand_dlist>("hand dlist (ptr)");
        if (sel("lists", "idx")) idx_list_bench("hand idx_list (u32)");
        if (sel("lists", "safe")) list_bench<boost_list<safe>>("bi::list safe,size");
        if (sel("lists", "normal")) list_bench<boost_list<normal>>("bi::list normal,size");
        if (sel("lists", "safe_nosize")) list_bench<boost_list<safe, bi::constant_time_size<false>>>("bi::list safe,nosize");
        if (sel("lists", "normal_nosize")) list_bench<boost_list<normal, bi::constant_time_size<false>>>("bi::list normal,nosize");
        if (sel("lists", "slist")) list_bench<boost_slist<bi::slist_member_hook<bi::link_mode<bi::normal_link>>>>("bi::slist normal");
    }
    if (only.empty() || only == "ids") {
        std::puts("== IDS: 50k live ids ==");
        if (sel("ids", "hash")) {
            HIdx h(65536);
            id_bench("hand hash_index 64k bkt", [&](long id) { return h.find(id); },
                     [&](HObj& o) { h.insert(o); }, [&](HObj& o) { HIdx::erase(o); });
        }
        if (sel("ids", "uset")) {
            std::vector<BUSet::bucket_type> b(65536);
            BUSet s(BUSet::bucket_traits(b.data(), b.size()));
            id_bench("bi::unordered_set 64k bkt", [&](long id) { return &*s.find(id); },
                     [&](HObj& o) { s.insert(o); }, [&](HObj& o) { s.erase(s.iterator_to(o)); });
            s.clear();
        }
        if (sel("ids", "set")) {
            BSet s;
            id_bench("bi::set (rbtree)", [&](long id) { return &*s.find(id); },
                     [&](HObj& o) { s.insert(o); }, [&](HObj& o) { s.erase(BSet::s_iterator_to(o)); });
            s.clear();
        }
        if (sel("ids", "flatmap")) {
            boost::unordered_flat_map<long, HObj*, mix> m;
            m.reserve(N);
            std::size_t bc0 = m.bucket_count();
            id_bench("unordered_flat_map<id,T*>", [&](long id) { return m.find(id)->second; },
                     [&](HObj& o) { m.emplace(o.id, &o); }, [&](HObj& o) { m.erase(o.id); });
            std::printf("unordered_flat_map bucket_count: after reserve %zu, after the run %zu%s\n", bc0,
                        m.bucket_count(), m.bucket_count() != bc0 ? "  <- it rehashed during the run" : "");
        }
        if (sel("ids", "flatmap2x")) {
            boost::unordered_flat_map<long, HObj*, mix> m;
            m.reserve(2 * N);   // headroom: see labs/l6_flat_drift
            std::size_t bc0 = m.bucket_count();
            id_bench("unordered_flat_map<id,T*> 2x rsv", [&](long id) { return m.find(id)->second; },
                     [&](HObj& o) { m.emplace(o.id, &o); }, [&](HObj& o) { m.erase(o.id); });
            std::printf("unordered_flat_map bucket_count: after reserve %zu, after the run %zu%s\n", bc0,
                        m.bucket_count(), m.bucket_count() != bc0 ? "  <- it rehashed during the run" : "");
        }
    }
    if (only.empty() || only == "pools") {
        std::puts("== POOLS: 50k live x 64 B, destroy+create pairs ==");
        if (sel("pools", "hand")) {
            hr::pool<PObj> p(N + 1);
            pool_bench("hand pool (free list)", [&](long i) { return p.create(PObj{i, 1, {}}); },
                       [&](PObj* o) { p.destroy(o); });
        }
        if (sel("pools", "slab")) {
            hft::arena_config cfg; cfg.region_bytes = std::size_t{64} << 20;
            hft::thread_arena arena(cfg);
            hft::slab<PObj> s(N + 1);
            pool_bench("hft::slab (unchecked)", [&](long i) { return s.create(PObj{i, 1, {}}); },
                       [&](PObj* o) { s.destroy(o); });
        }
        if (sel("pools", "pool")) {
            boost::pool<> p(sizeof(PObj));
            pool_bench("boost::pool<> malloc/free", [&](long i) { return ::new (p.malloc()) PObj{i, 1, {}}; },
                       [&](PObj* o) { p.free(o); });
        }
        if (sel("pools", "object_pool")) {
            boost::object_pool<PObj> p;
            pool_bench("boost::object_pool construct/destroy", [&](long i) { return p.construct(PObj{i, 1, {}}); },
                       [&](PObj* o) { p.destroy(o); });
        }
        if (sel("pools", "pool_ordered")) {   // isolates object_pool's cost: same pool, ORDERED free
            boost::pool<> p(sizeof(PObj));
            pool_bench("boost::pool<> malloc/ordered_free", [&](long i) { return ::new (p.malloc()) PObj{i, 1, {}}; },
                       [&](PObj* o) { p.ordered_free(o); });
        }
        if (sel("pools", "new"))
            pool_bench("operator new/delete", [](long i) { return new PObj{i, 1, {}}; }, [](PObj* o) { delete o; });
    }
}
