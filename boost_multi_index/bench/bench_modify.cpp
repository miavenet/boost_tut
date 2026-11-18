// bench_modify — the cost of changing an order in place (modify) vs by copy (replace),
// for a NON-key field (qty) and a KEY field (price), in the 3-index order store.
// Each sample = 16 operations on random live orders, reported as ns per operation.
// Swept over the number of distinct clients, i.e. the size of each equal-key group in the
// hashed_non_unique client index. Mechanism (boost/multi_index/hashed_index.hpp, in_place()
// for hashed_non_unique_tag, Boost 1.90): after modify() the index cannot know whether the key
// changed, so it walks the element's equal-key group to verify its position -> O(group size).
// replace() compares old and new key first and skips the walk when the key is unchanged.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/bench_modify.cpp -o build/bench_modify
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include "lat.hpp"

struct Order { std::uint64_t id; std::uint32_t client; char side; std::int64_t price; std::uint64_t ts; std::uint32_t qty; };
namespace bmi = boost::multi_index;
using Store = bmi::multi_index_container<Order, bmi::indexed_by<
    bmi::hashed_unique<bmi::key<&Order::id>>,
    bmi::ordered_non_unique<bmi::key<&Order::side, &Order::price, &Order::ts>>,
    bmi::hashed_non_unique<bmi::key<&Order::client>>>>;
constexpr std::size_t N = 100'000;
constexpr int SAMPLES = 20000, BATCH = 16;

template <class F> void run(const char* name, Store& s, F&& op) {
    Rng r; Lat l; l.reserve(SAMPLES);
    auto& ids = s.get<0>();
    std::uint64_t keys[BATCH];
    for (int i = 0; i < SAMPLES; ++i) {
        for (auto& k : keys) k = r() % N;
        auto t0 = Clock::now();
        for (auto k : keys) op(ids, ids.find(k), r());
        l.add(ns_since(t0) / BATCH);
    }
    l.report(name);
}

int main() {
  for (std::size_t clients : {N, std::size_t(1000), std::size_t(64)}) {
    Store s; s.get<0>().reserve(N); s.get<2>().reserve(N);
    for (std::size_t i = 0; i < N; ++i) s.insert(Order{i, std::uint32_t(i % clients), (i & 1) ? 'S' : 'B', std::int64_t(i % 1000), i, 1});
    std::printf("-- N=%zu live orders, 3 indices, %zu clients (%zu orders per client); ns per op incl. find(id)\n",
                N, clients, N / clients);
    run("find only (baseline)", s, [](auto&, auto it, std::uint64_t v) { keep(it->qty + v); });
    run("modify qty (non-key)", s, [](auto& ix, auto it, std::uint64_t v) { ix.modify(it, [v](Order& o) { o.qty = std::uint32_t(v); }); });
    run("replace qty (non-key)", s, [](auto& ix, auto it, std::uint64_t v) { Order c = *it; c.qty = std::uint32_t(v); ix.replace(it, c); });
    run("modify price (key)", s, [](auto& ix, auto it, std::uint64_t v) { ix.modify(it, [v](Order& o) { o.price = std::int64_t(v % 1000); }); });
    run("replace price (key)", s, [](auto& ix, auto it, std::uint64_t v) { Order c = *it; c.price = std::int64_t(v % 1000); ix.replace(it, c); });
    std::printf("size after all ops = %zu (non-unique indices: nothing can collide)\n", s.size());
  }
}
