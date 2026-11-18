// b1_containers.cpp — same hot-path workload, default allocator vs the pooled hierarchy.
// Prints p50 / p99 / p99.9 / max per operation (ns). Build + run: bash bench/run.sh
//
// Node workload ("churn"): prefill 50,000 orders, then 200,000 x {erase a random live order;
//   insert a new random order}. One timed sample = one erase+insert pair.
// Flat workload ("build"): insert 100,000 ascending keys into an empty container.
//   One timed sample = one insert. Growth (reallocation / rehash) shows up in the tail.
//
// Variants (a-d, plus the library):
//   (a) default allocator                     (std::allocator -> malloc/free)
//   (b) monotonic_buffer_resource             (bump; frees are no-ops; grows from malloc)
//   (c) unsynchronized_pool over monotonic    (recycles freed nodes)
//   (d) full hierarchy + tripwire, sealed     (region -> monotonic -> pool, overflow throws)
//   (e) hft:: alias (stateless allocator on the same thread_arena, tripwire, sealed)
// Every hft_mem.hpp alias has an (e) row here next to its default-allocator (a) row; scratch<N>
// has one; pin_this_thread is called once at start (Linux only; prints the result).
#include "../labs/hft_mem/hft_mem.hpp"
#include "bench_common.hpp"

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <memory_resource>

namespace bmi = boost::multi_index;
using bench::recorder;

struct Order {
    long id;
    long px;
    long qty;
    char pad[40];  // 64-byte payload
};

constexpr int PREFILL = 50'000;
constexpr int OPS = 200'000;
constexpr int FLAT_N = 100'000;

hft::arena_config tripwire_cfg() {
    hft::arena_config c;
    c.region_bytes = std::size_t{256} << 20;
    c.tripwire = true;
    return c;
}

// ---- node containers: map-like (emplace(key, Order)) -----------------------------------------
template <class Map, class Seal>
void churn(Map& m, const char* name, Seal seal) {
    std::vector<long> live(PREFILL);
    bench::xorshift rng;
    for (int i = 0; i < PREFILL; ++i) {
        long k = static_cast<long>(rng() >> 1);
        live[i] = k;
        m.emplace(k, Order{k, k & 1023, 1, {}});
    }
    seal();
    recorder r(OPS);
    for (int i = 0; i < OPS; ++i) {
        int slot = static_cast<int>(rng() % PREFILL);
        long k = static_cast<long>(rng() >> 1);
        auto t0 = bench::now_ns();
        m.erase(live[slot]);
        m.emplace(k, Order{k, k & 1023, 1, {}});
        auto t1 = bench::now_ns();
        live[slot] = k;
        r.add(t1 - t0);
    }
    r.report(name);
}

// ---- multi_index order store (hashed by id + ordered by px) ---------------------------------
using Indices = bmi::indexed_by<bmi::hashed_unique<bmi::member<Order, long, &Order::id>>,
                                bmi::ordered_non_unique<bmi::member<Order, long, &Order::px>>>;

template <class Store, class Seal>
void churn_mi(Store& s, const char* name, Seal seal) {
    std::vector<long> live(PREFILL);
    bench::xorshift rng;
    s.template get<0>().reserve(PREFILL * 2);
    for (int i = 0; i < PREFILL; ++i) {
        long k = static_cast<long>(rng() >> 1);
        live[i] = k;
        s.insert(Order{k, k & 1023, 1, {}});
    }
    seal();
    recorder r(OPS);
    for (int i = 0; i < OPS; ++i) {
        int slot = static_cast<int>(rng() % PREFILL);
        long k = static_cast<long>(rng() >> 1);
        auto t0 = bench::now_ns();
        s.erase(live[slot]);
        s.insert(Order{k, k & 1023, 1, {}});
        auto t1 = bench::now_ns();
        live[slot] = k;
        r.add(t1 - t0);
    }
    r.report(name);
}

// ---- flat / contiguous: build from empty -------------------------------------------------------
template <class Map, class Prep>
void build(Map& m, const char* name, Prep prep) {
    prep(m);
    recorder r(FLAT_N);
    for (long k = 0; k < FLAT_N; ++k) {
        auto t0 = bench::now_ns();
        m.emplace(k, Order{k, k & 1023, 1, {}});
        auto t1 = bench::now_ns();
        r.add(t1 - t0);
    }
    bench::keep(m.size());
    r.report(name);
}

template <class Vec, class Prep>
void build_vec(Vec& v, const char* name, Prep prep) {
    prep(v);
    recorder r(FLAT_N);
    for (long k = 0; k < FLAT_N; ++k) {
        auto t0 = bench::now_ns();
        v.push_back(Order{k, k & 1023, 1, {}});
        auto t1 = bench::now_ns();
        r.add(t1 - t0);
    }
    bench::keep(v.size());
    r.report(name);
}

// ---- strings: build and destroy one 100-char string per sample --------------------------------
template <class Str, class Seal>
void strings(const char* name, Seal seal) {
    { Str warmup(100, 'x'); bench::keep(warmup.data()); }  // the pool now holds one 101-byte block
    seal();
    recorder r(OPS);
    for (int i = 0; i < OPS; ++i) {
        auto t0 = bench::now_ns();
        { Str s(100, static_cast<char>('a' + i % 26)); bench::keep(s.data()); }
        auto t1 = bench::now_ns();
        r.add(t1 - t0);
    }
    r.report(name);
}

int main() {
    auto none = [] {};
    // Pin first, as lesson 03 teaches. Linux only: on macOS the helper returns false.
    std::printf("hft::pin_this_thread(0): %s\n", hft::pin_this_thread(0) ? "pinned" : "not pinned (not Linux, or refused)");
    std::printf("== NODE: std::map churn (erase+insert per sample) ==\n");
    { std::map<long, Order> m; churn(m, "(a) std::map default", none); }
    { std::pmr::monotonic_buffer_resource mono;
      std::pmr::map<long, Order> m(&mono); churn(m, "(b) pmr::map monotonic", none); }
    { std::pmr::monotonic_buffer_resource mono;
      std::pmr::unsynchronized_pool_resource pool(&mono);
      std::pmr::map<long, Order> m(&pool); churn(m, "(c) pmr::map pool->monotonic", none); }
    { hft::thread_arena a(tripwire_cfg());
      std::pmr::map<long, Order> m(a.resource()); churn(m, "(d) pmr::map full hierarchy+tripwire", [&] { a.seal(); }); }
    { hft::thread_arena a(tripwire_cfg());
      hft::map<long, Order> m; churn(m, "(e) hft::map", [&] { a.seal(); }); }

    std::printf("== NODE: unordered_map / unordered_node_map / multi_index churn ==\n");
    { std::unordered_map<long, Order> m; m.reserve(PREFILL * 2); churn(m, "(a) std::unordered_map default", none); }
    { hft::thread_arena a(tripwire_cfg());
      hft::unordered_map<long, Order> m; m.reserve(PREFILL * 2); churn(m, "(e) hft::unordered_map", [&] { a.seal(); }); }
    { boost::unordered_node_map<long, Order> m; m.reserve(PREFILL * 2); churn(m, "(a) boost::unordered_node_map default", none); }
    { hft::thread_arena a(tripwire_cfg());
      hft::unordered_node_map<long, Order> m; m.reserve(PREFILL * 2); churn(m, "(e) hft::unordered_node_map", [&] { a.seal(); }); }
    { boost::multi_index_container<Order, Indices> s; churn_mi(s, "(a) multi_index default", none); }
    { hft::thread_arena a(tripwire_cfg());
      hft::multi_index<Order, Indices> s; churn_mi(s, "(e) hft::multi_index", [&] { a.seal(); }); }

    std::printf("== FLAT: boost::container::flat_map build, ascending keys ==\n");
    using pmr_flat = boost::container::flat_map<long, Order, std::less<long>,
                                                std::pmr::polymorphic_allocator<std::pair<long, Order>>>;
    { boost::container::flat_map<long, Order> m; build(m, "(a) flat_map default, no reserve", [](auto&) {}); }
    { boost::container::flat_map<long, Order> m; build(m, "(a) flat_map default, reserve", [](auto& x) { x.reserve(FLAT_N); }); }
    { std::pmr::monotonic_buffer_resource mono;
      pmr_flat m(&mono); build(m, "(b) flat_map monotonic, no reserve", [](auto&) {}); }
    { std::pmr::monotonic_buffer_resource mono;
      std::pmr::unsynchronized_pool_resource pool(&mono);
      pmr_flat m(&pool); build(m, "(c) flat_map pool->monotonic, no reserve", [](auto&) {}); }
    { hft::thread_arena a(tripwire_cfg());
      pmr_flat m(a.bulk()); build(m, "(d) flat_map hierarchy+tripwire, reserve", [&](auto& x) { x.reserve(FLAT_N); a.seal(); }); }
    { hft::thread_arena a(tripwire_cfg());
      hft::flat_map<long, Order> m; build(m, "(e) hft::flat_map, reserve", [&](auto& x) { x.reserve(FLAT_N); a.seal(); }); }

    std::printf("== FLAT: unordered_flat_map build / vector push_back ==\n");
    { boost::unordered_flat_map<long, Order> m; build(m, "(a) unordered_flat_map default, no reserve", [](auto&) {}); }
    { boost::unordered_flat_map<long, Order> m; build(m, "(a) unordered_flat_map default, reserve", [](auto& x) { x.reserve(FLAT_N); }); }
    { hft::thread_arena a(tripwire_cfg());
      hft::unordered_flat_map<long, Order> m; build(m, "(e) hft::unordered_flat_map, reserve", [&](auto& x) { x.reserve(FLAT_N); a.seal(); }); }
    { std::vector<Order> v; build_vec(v, "(a) std::vector default, no reserve", [](auto&) {}); }
    { std::vector<Order> v; build_vec(v, "(a) std::vector default, reserve", [](auto& x) { x.reserve(FLAT_N); }); }
    { hft::thread_arena a(tripwire_cfg());
      hft::vector<Order> v; build_vec(v, "(e) hft::vector, reserve", [&](auto& x) { x.reserve(FLAT_N); a.seal(); }); }

    std::printf("== STRING: one 100-char string built and destroyed per sample ==\n");
    strings<std::string>("(a) std::string default", none);
    { hft::thread_arena a(tripwire_cfg());
      strings<hft::string>("(e) hft::string", [&] { a.seal(); }); }

    std::printf("== SCRATCH: per-message temporaries (vector<int> of 32 + 100-char string) ==\n");
    {
        recorder r(OPS);
        for (int i = 0; i < OPS; ++i) {
            auto t0 = bench::now_ns();
            std::vector<int> v; for (int j = 0; j < 32; ++j) v.push_back(j);
            std::string s(100, 'x');
            bench::keep(v.data()); bench::keep(s.data());
            auto t1 = bench::now_ns();
            r.add(t1 - t0);
        }
        r.report("(a) std::vector + std::string default");
    }
    {
        hft::scratch<8192> sc;
        recorder r(OPS);
        for (int i = 0; i < OPS; ++i) {
            auto t0 = bench::now_ns();
            {
                std::pmr::vector<int> v(sc.resource()); for (int j = 0; j < 32; ++j) v.push_back(j);
                std::pmr::string s(100, 'x', sc.resource());
                bench::keep(v.data()); bench::keep(s.data());
            }
            sc.reset();
            auto t1 = bench::now_ns();
            r.add(t1 - t0);
        }
        r.report("(e) hft::scratch + reset() per message");
    }
}
