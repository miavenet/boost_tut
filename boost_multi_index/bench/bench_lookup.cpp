// bench_lookup — find-by-order-id: node-based vs open-addressing vs a dense slot array,
// at a cache-resident size and a cache-busting size.
// Each sample = 32 random lookups timed together, reported as ns per lookup
// (single lookups are below the ~41 ns timer tick on this machine).
// The lookups are DEPENDENT: each key is derived from the previous result, so the CPU cannot
// overlap their cache misses. That approximates the latency of one cold lookup per message.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/bench_lookup.cpp -o build/bench_lookup
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <unordered_map>
#include "lat.hpp"

struct Order { std::uint64_t id; std::uint32_t client; char side; std::int64_t price; std::uint64_t ts; std::uint32_t qty; };
namespace bmi = boost::multi_index;
using MIHash = bmi::multi_index_container<Order, bmi::indexed_by<bmi::hashed_unique<bmi::key<&Order::id>>>>;
using MIOrd  = bmi::multi_index_container<Order, bmi::indexed_by<bmi::ordered_unique<bmi::key<&Order::id>>>>;
constexpr int SAMPLES = 200000, BATCH = 32;

template <class F> void run(const char* name, std::size_t n, F&& find_qty) {
    Rng r; Lat l; l.reserve(SAMPLES);
    std::uint64_t sink = 0;
    std::vector<std::uint64_t> keys(BATCH);
    for (int s = 0; s < SAMPLES; ++s) {
        for (auto& k : keys) k = r();
        auto t0 = Clock::now();
        std::uint64_t q = 0;
        for (auto k : keys) q = find_qty((k + q) % n);   // next key depends on the last answer
        sink += q;
        l.add(ns_since(t0) / BATCH);
    }
    keep(sink);
    char label[96]; std::snprintf(label, sizeof label, "%s (n=%zu)", name, n);
    l.report(label);
}

int main() {
    for (std::size_t n : {std::size_t(10'000), std::size_t(4'000'000)}) {
        std::printf("-- %zu live orders, ns per lookup (batch of %d) --\n", n, BATCH);
        // Insert in a scrambled order so node addresses do not follow id order.
        std::vector<std::uint64_t> order(n);
        for (std::size_t i = 0; i < n; ++i) order[i] = (i * 2654435761u) % n;
        {
            MIHash c; c.reserve(n);
            for (auto id : order) c.insert(Order{id, 1, 'B', 100, id, 0});
            run("MI hashed_unique", n, [&](std::uint64_t k) { return c.find(k)->qty; });
        }
        {
            MIOrd c;
            for (auto id : order) c.insert(Order{id, 1, 'B', 100, id, 0});
            run("MI ordered_unique", n, [&](std::uint64_t k) { return c.find(k)->qty; });
        }
        {
            std::unordered_map<std::uint64_t, Order> m; m.reserve(n);
            for (auto id : order) m.emplace(id, Order{id, 1, 'B', 100, id, 0});
            run("std::unordered_map", n, [&](std::uint64_t k) { return m.find(k)->second.qty; });
        }
        {
            boost::unordered_flat_map<std::uint64_t, Order> m; m.reserve(n);
            for (auto id : order) m.emplace(id, Order{id, 1, 'B', 100, id, 0});
            run("boost::unordered_flat_map", n, [&](std::uint64_t k) { return m.find(k)->second.qty; });
        }
        {
            std::vector<Order> slots(n);                 // dense id -> slot: the id IS the index
            for (auto id : order) slots[id] = Order{id, 1, 'B', 100, id, 0};
            run("dense vector<Order>[id]", n, [&](std::uint64_t k) { return slots[k].qty; });
        }
    }
}
