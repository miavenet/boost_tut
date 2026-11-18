// l1_every_alias.cpp — every alias and helper in hft_mem.hpp, used the way lesson 01 teaches:
// pin (Linux) -> arena -> containers -> warm/reserve -> seal -> churn. For each alias it prints
// whether its memory sits in the arena's region and how many global operator new calls the
// sealed churn made (expected: yes / 0 for every row).
// Run: ./labs/build/l1_every_alias
#include "hft_mem/hft_mem.hpp"

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <new>

static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Order { long id; long px; };
namespace bmi = boost::multi_index;
using Indices = bmi::indexed_by<bmi::hashed_unique<bmi::member<Order, long, &Order::id>>,
                                bmi::ordered_non_unique<bmi::member<Order, long, &Order::px>>>;

constexpr long N = 1000, OPS = 10000;
int bad = 0;

void row(const char* name, bool in_region, long news, const char* where = "in region?") {
    std::printf("  %-34s %-11s %-3s  operator new in sealed churn: %ld\n", name, where, in_region ? "yes" : "NO", news);
    if (!in_region || news != 0) bad = 1;
}

// Node map-likes: erase the oldest key, emplace a new one.
template <class M>
void churn_map(const char* name, hft::thread_arena& arena, M& m) {
    for (long i = 0; i < N; ++i) m.emplace(i, Order{i, i % 17});
    bool in = arena.region().owns(&*m.begin());
    arena.seal();
    long before = g_news;
    for (long i = N; i < N + OPS; ++i) { m.erase(i - N); m.emplace(i, Order{i, i % 17}); }
    row(name, in, g_news - before);
    arena.unseal();
}

int main() {
    // Pin before the arena (first touch decides the NUMA node). Linux only; false on macOS.
    std::printf("hft::pin_this_thread(0): %s\n", hft::pin_this_thread(0) ? "pinned" : "not pinned (not Linux, or refused)");
    hft::thread_arena arena;
    arena.warm(64, 4096);  // pre-carve 64 B blocks at start-up (no region spent on bookkeeping)

    std::puts("node containers (the pool recycles freed nodes):");
    { hft::map<long, Order> m; churn_map("hft::map", arena, m); }
    { hft::unordered_map<long, Order> m; m.reserve(2 * N); churn_map("hft::unordered_map (reserved)", arena, m); }
    { hft::unordered_node_map<long, Order> m; m.reserve(2 * N); churn_map("hft::unordered_node_map (reserved)", arena, m); }
    {
        hft::multi_index<Order, Indices> s;
        s.get<0>().reserve(2 * N);
        for (long i = 0; i < N; ++i) s.insert(Order{i, i % 17});
        bool in = arena.region().owns(&*s.begin());
        arena.seal();
        long before = g_news;
        for (long i = N; i < N + OPS; ++i) { s.erase(i - N); s.insert(Order{i, i % 17}); }
        row("hft::multi_index (hashed+ordered)", in, g_news - before);
        arena.unseal();
    }

    std::puts("flat containers (reserve at warm-up, then no growth):");
    { hft::flat_map<long, Order> m; m.reserve(N + 1); churn_map("hft::flat_map (reserved)", arena, m); }
    { hft::unordered_flat_map<long, Order> m; m.reserve(2 * N); churn_map("hft::unordered_flat_map (reserved)", arena, m); }
    {
        hft::vector<Order> v; v.reserve(N);
        for (long i = 0; i < N; ++i) v.push_back(Order{i, 0});
        bool in = arena.region().owns(v.data());
        arena.seal();
        long before = g_news;
        for (long i = 0; i < OPS; ++i) { v.pop_back(); v.push_back(Order{i, 1}); }
        row("hft::vector (reserved)", in, g_news - before);
        arena.unseal();
    }
    {
        hft::vector<hft::string> names; names.reserve(8);
        names.emplace_back(100, 'x');  // longer than the small-string buffer: bytes on the heap side
        bool in = arena.region().owns(names.data()) && arena.region().owns(names[0].data());
        arena.seal();
        long before = g_news;
        for (long i = 0; i < OPS; ++i) { names.pop_back(); names.emplace_back(100, 'a' + i % 26); }
        row("hft::string inside hft::vector", in, g_news - before);
        arena.unseal();
    }

    std::puts("per-message scratch (monotonic over a fixed buffer, reset() per message):");
    {
        hft::scratch<8192> sc;
        long before = g_news;
        bool in = true;
        for (long i = 0; i < OPS; ++i) {
            {
                std::pmr::vector<int> v(sc.resource());
                for (int j = 0; j < 32; ++j) v.push_back(j);
                in = in && reinterpret_cast<const void*>(&sc) <= v.data() &&
                     static_cast<const void*>(v.data()) < reinterpret_cast<const char*>(&sc) + sizeof sc;
            }
            sc.reset();
        }
        row("hft::scratch<8192>", in, g_news - before, "in buffer?");
    }
    std::printf("region: used %zu KiB of %zu KiB, overflow allocations %zu\n",
                arena.region().used() >> 10, arena.region().capacity() >> 10, arena.region().overflow_count());
    std::puts(bad ? "SOME ROW FAILED" : "every alias: memory in the arena, zero operator new under seal");
    return bad;
}
