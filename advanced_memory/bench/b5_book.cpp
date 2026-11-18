// b5_book.cpp — the same order-book workload on four designs; p50/p99/p99.9/max per operation.
// Release build: NDEBUG is defined below (production config). Build: bash labs/build.sh
//
// Workload (identical sequence for every design, deterministic RNG):
//   prefill 50,000 orders, random price in 1,024 levels, 16 clients;
//   then 400,000 operations: 35% add, 35% cancel a random live order by id,
//   20% modify (reduce qty in place, keeps priority), 10% walk one random level (sum qty).
//   Every operation is timed alone and recorded in its own row.
// Designs:
//   A  intrusive: hft::slab + Boost.Intrusive lists (level, client) + hft::unordered_flat_map<id,Order*>
//   A2 intrusive, id = slab handle (no hash map at all)
//   B  boost::multi_index (hashed id + ordered_non_unique px), default allocator (malloc)
//   C  the same multi_index on hft::allocator (pooled thread arena)
//   D  pointer model: new Order + std::list<Order*> per level + unordered_map<id, {Order*, it}>
#ifndef BENCH_ASSERTS
#  define NDEBUG 1
#endif
#include "../labs/hft_mem/hft_book.hpp"
#include "bench_common.hpp"

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

#include <cstdio>
#include <list>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bmi = boost::multi_index;
using bench::recorder;

constexpr int PREFILL = 50'000, OPS = 400'000, LEVELS = 1024, CLIENTS = 16;
constexpr std::size_t CAP = 200'000;

// One op stream for all designs. Cancels pick a random live id from `live` (swap-remove).
struct op { std::uint8_t kind; std::uint32_t r1, r2; };
std::vector<op> make_ops() {
    bench::xorshift rng;
    std::vector<op> v(OPS);
    for (auto& o : v) {
        std::uint32_t p = static_cast<std::uint32_t>(rng() % 100);
        o.kind = p < 35 ? 0 : p < 70 ? 1 : p < 90 ? 2 : 3;
        o.r1 = static_cast<std::uint32_t>(rng());
        o.r2 = static_cast<std::uint32_t>(rng());
    }
    return v;
}

// Drive any design through the same ops. D must provide add(id,px,client,qty), cancel(id),
// reduce(id,qty), level_qty(px). For designs whose id is assigned by the book (A2) add returns it.
template <class Design>
void drive(const char* name, Design& d, const std::vector<op>& ops) {
    std::vector<long> live;
    live.reserve(CAP);
    long next_id = 1;
    bench::xorshift rng;
    for (int i = 0; i < PREFILL; ++i) {
        long id = d.add(next_id++, static_cast<int>(rng() % LEVELS), static_cast<int>(rng() % CLIENTS), 100);
        live.push_back(id);
    }
    d.warm_done();
    recorder r_add(OPS), r_cxl(OPS), r_mod(OPS), r_walk(OPS);
    // Each recorder counts faults from its own construction, so r_add would also count the
    // first touch of the three recorders built after it. Re-arm all four here.
    for (recorder* r : {&r_add, &r_cxl, &r_mod, &r_walk}) r->faults0 = bench::minor_faults();
    long sink = 0;
    for (const op& o : ops) {
        if (o.kind == 0 || live.empty()) {
            int px = static_cast<int>(o.r1 % LEVELS), cl = static_cast<int>(o.r2 % CLIENTS);
            long want = next_id++;
            auto t0 = bench::now_ns();
            long id = d.add(want, px, cl, 100);
            auto t1 = bench::now_ns();
            live.push_back(id);
            r_add.add(t1 - t0);
        } else if (o.kind == 1) {
            std::size_t j = o.r1 % live.size();
            long id = live[j];
            auto t0 = bench::now_ns();
            bool ok = d.cancel(id);
            auto t1 = bench::now_ns();
            sink += ok;
            live[j] = live.back(); live.pop_back();
            r_cxl.add(t1 - t0);
        } else if (o.kind == 2) {
            long id = live[o.r1 % live.size()];
            long q = 1 + o.r2 % 99;
            auto t0 = bench::now_ns();
            sink += d.reduce(id, q);
            auto t1 = bench::now_ns();
            r_mod.add(t1 - t0);
        } else {
            int px = static_cast<int>(o.r1 % LEVELS);
            auto t0 = bench::now_ns();
            sink += d.level_qty(px);
            auto t1 = bench::now_ns();
            r_walk.add(t1 - t0);
        }
    }
    bench::keep(sink);
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s add", name);    r_add.report(buf);
    std::snprintf(buf, sizeof buf, "%s cancel", name); r_cxl.report(buf);
    std::snprintf(buf, sizeof buf, "%s modify", name); r_mod.report(buf);
    std::snprintf(buf, sizeof buf, "%s walk", name);   r_walk.report(buf);
    std::printf("%s final live=%zu\n", name, live.size());
}

// ---- A / A2: the intrusive book ------------------------------------------------------------
template <hft::book::IdIndex Ix>
struct intrusive_design {
    hft::book::book<Ix> b{CAP, LEVELS, CLIENTS};
    long add(long id, int px, int cl, long q) {
        auto* o = b.add(id, px, cl, q);     // links 3 hooks: level, client, send
        b.mark_sent(*o);                    // pretend it was sent and acked at once: unlink send, link ack
        b.mark_acked(*o);                   // unlink ack. So a timed A/A2 "add" = 6 hook operations
                                            // (4 links, 2 unlinks); 2 hooks stay linked (level, client),
                                            // which cancel unlinks. Keeps the state lists empty.
        return o->id;
    }
    bool cancel(long id) { return b.cancel(id); }
    bool reduce(long id, long q) { return b.reduce(id, q); }
    long level_qty(int px) { return b.level_qty(px); }
    void warm_done() {}
};

// ---- B / C: multi_index ------------------------------------------------------------------------
struct MOrder { long id; long qty; int px; int client; };
using MIndices = bmi::indexed_by<bmi::hashed_unique<bmi::member<MOrder, long, &MOrder::id>>,
                                 bmi::ordered_non_unique<bmi::member<MOrder, int, &MOrder::px>>>;
template <class Store>
struct mi_design {
    Store s;
    mi_design() { s.template get<0>().reserve(CAP); }
    long add(long id, int px, int cl, long q) { s.insert(MOrder{id, q, px, cl}); return id; }
    bool cancel(long id) { return s.template get<0>().erase(id) == 1; }
    bool reduce(long id, long q) {
        auto& ix = s.template get<0>();
        auto it = ix.find(id);
        if (it == ix.end() || q >= it->qty) return false;
        return ix.modify(it, [q](MOrder& o) { o.qty = q; });
    }
    long level_qty(int px) {
        long sum = 0;
        auto [a, b] = s.template get<1>().equal_range(px);
        for (; a != b; ++a) sum += a->qty;
        return sum;
    }
    void warm_done() {}
};
using MI_default = bmi::multi_index_container<MOrder, MIndices>;
using MI_hft = hft::multi_index<MOrder, MIndices>;

// ---- D: pointer model ------------------------------------------------------------------------
struct POrder { long id; long qty; int px; int client; };
struct pointer_design {
    struct entry { POrder* o; std::list<POrder*>::iterator lvl, cl; };
    std::vector<std::list<POrder*>> levels{LEVELS}, clients{CLIENTS};
    std::unordered_map<long, entry> ids;
    pointer_design() { ids.reserve(CAP); }
    ~pointer_design() { for (auto& [k, e] : ids) delete e.o; }
    long add(long id, int px, int cl, long q) {
        POrder* o = new POrder{id, q, px, cl};                               // allocation 1
        auto& l = levels[px]; auto& c = clients[cl];
        ids.emplace(id, entry{o, l.insert(l.end(), o), c.insert(c.end(), o)});   // 2, 3 and 4
        return id;
    }
    bool cancel(long id) {
        auto it = ids.find(id);
        if (it == ids.end()) return false;
        POrder* o = it->second.o;
        levels[o->px].erase(it->second.lvl);
        clients[o->client].erase(it->second.cl);
        ids.erase(it);
        delete o;
        return true;
    }
    bool reduce(long id, long q) {
        auto it = ids.find(id);
        if (it == ids.end() || q >= it->second.o->qty) return false;
        it->second.o->qty = q;
        return true;
    }
    long level_qty(int px) { long s = 0; for (POrder* o : levels[px]) s += o->qty; return s; }
    void warm_done() {}
};

int main(int argc, char** argv) {
    // One design per process when an argument is given (A, A2, B, C, D): bench/run_intrusive.sh
    // runs them that way so no design inherits another's heap or cache state.
    std::string_view only = argc > 1 ? argv[1] : "";
    auto want = [&](std::string_view k) { return only.empty() || only == k; };
    auto ops = make_ops();
    std::printf("== ORDER BOOK: %d prefill, %d ops (35%% add, 35%% cancel, 20%% modify, 10%% walk level) ==\n", PREFILL, OPS);
    {
        hft::arena_config cfg; cfg.region_bytes = std::size_t{256} << 20;
        hft::thread_arena arena(cfg);
        if (want("A")) { intrusive_design<hft::book::IdIndex::flat> d; drive("A  intrusive+slab+flat_map", d, ops); }
        if (want("A2")) { intrusive_design<hft::book::IdIndex::handle> d; drive("A2 intrusive+slab, id=handle", d, ops); }
    }
    if (want("B")) { mi_design<MI_default> d; drive("B  multi_index default alloc", d, ops); }
    if (want("C")) {
        hft::arena_config cfg; cfg.region_bytes = std::size_t{256} << 20;
        hft::thread_arena arena(cfg);
        arena.warm(sizeof(MOrder) + 64, CAP);   // pre-carve pool blocks (node size < MOrder + 64 B)
        { mi_design<MI_hft> d; drive("C  multi_index hft::allocator", d, ops); }
    }
    if (want("D")) { pointer_design d; drive("D  list<Order*>+unordered_map", d, ops); }
}
