// bench_rehash — per-insert latency of a hashed index, with and without reserve().
// The mean hides the rehash; the max shows it.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/bench_rehash.cpp -o build/bench_rehash
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <memory_resource>
#include <unordered_map>
#include "lat.hpp"

struct Order { std::uint64_t id; std::uint32_t client; std::int64_t price; std::uint32_t qty; };
namespace bmi = boost::multi_index;
using Store = bmi::multi_index_container<Order, bmi::indexed_by<bmi::hashed_unique<bmi::key<&Order::id>>>>;
constexpr int N = 1'000'000;

template <class F> void run(const char* name, F&& insert_one) {
    Lat l; l.reserve(N);
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        insert_one(std::uint64_t(i) * 2654435761u);
        l.add(ns_since(t0));
    }
    l.report(name);
}

int main() {
    std::printf("N=%d single inserts, timed one at a time (timer tick ~41 ns)\n", N);
    { Store s; run("MI hashed_unique, no reserve", [&](std::uint64_t k) { s.insert({k, 1, 100, 1}); }); }
    { Store s; s.reserve(N); run("MI hashed_unique, reserve(N)", [&](std::uint64_t k) { s.insert({k, 1, 100, 1}); }); }
    {
        // reserve + nodes from a pre-touched arena: no malloc and no first-touch page fault in the loop.
        std::vector<std::byte> buf(std::size_t(N) * 128);          // zero-filled -> every page touched
        std::pmr::monotonic_buffer_resource arena(buf.data(), buf.size());
        using PStore = bmi::multi_index_container<Order, bmi::indexed_by<bmi::hashed_unique<bmi::key<&Order::id>>>,
                                                  std::pmr::polymorphic_allocator<Order>>;
        PStore s(&arena); s.reserve(N);
        run("MI hashed_unique, reserve + pmr arena", [&](std::uint64_t k) { s.insert({k, 1, 100, 1}); });
    }
    { std::unordered_map<std::uint64_t, Order> m; run("std::unordered_map, no reserve", [&](std::uint64_t k) { m.emplace(k, Order{k, 1, 100, 1}); }); }
    { std::unordered_map<std::uint64_t, Order> m; m.reserve(N); run("std::unordered_map, reserve(N)", [&](std::uint64_t k) { m.emplace(k, Order{k, 1, 100, 1}); }); }
    { boost::unordered_flat_map<std::uint64_t, Order> m; run("boost::unordered_flat_map, no reserve", [&](std::uint64_t k) { m.emplace(k, Order{k, 1, 100, 1}); }); }
    { boost::unordered_flat_map<std::uint64_t, Order> m; m.reserve(N); run("boost::unordered_flat_map, reserve(N)", [&](std::uint64_t k) { m.emplace(k, Order{k, 1, 100, 1}); }); }
}
