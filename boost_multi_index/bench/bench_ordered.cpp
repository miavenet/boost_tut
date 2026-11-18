// bench_ordered — per-op latency of an ordered_unique index (red-black tree): does rebalancing show in the tail?
// Nodes come from a pre-touched pmr arena, so malloc and first-touch page faults are out of the loop
// and what remains is the tree work (search, link, rebalance, unlink) plus cache misses.
// Rows: insert ascending keys, insert pseudo-random keys, erase pseudo-random keys.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/bench_ordered.cpp -o build/bench_ordered
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <memory_resource>
#include "lat.hpp"

struct Order { std::uint64_t id; std::uint32_t client; std::int64_t price; std::uint32_t qty; };
namespace bmi = boost::multi_index;
using Store = bmi::multi_index_container<Order, bmi::indexed_by<bmi::ordered_unique<bmi::key<&Order::id>>>,
                                         std::pmr::polymorphic_allocator<Order>>;
constexpr std::size_t N = 1'000'000;
// A fixed, shuffled permutation of 0..N-1 (deterministic xorshift Fisher-Yates), so keys really are random.
static std::vector<std::uint64_t> make_perm() {
    std::vector<std::uint64_t> p(N); Rng r;
    for (std::size_t i = 0; i < N; ++i) p[i] = i;
    for (std::size_t i = N - 1; i > 0; --i) std::swap(p[i], p[r() % (i + 1)]);
    return p;
}

int main() {
    const auto perm = make_perm();
    std::printf("N=%zu ops on one ordered_unique index, timed one at a time (timer tick ~41 ns); nodes from a pre-touched arena\n", N);
    for (int pass = 0; pass < 2; ++pass) {
        std::vector<std::byte> buf(N * 128);   // zero-filled -> every page touched
        std::pmr::monotonic_buffer_resource arena(buf.data(), buf.size());
        Store s(&arena);
        Lat ins; ins.reserve(N);
        for (std::size_t i = 0; i < N; ++i) {
            std::uint64_t k = pass == 0 ? i : perm[i];
            auto t0 = Clock::now();
            s.insert({k, 1, 100, 1});
            ins.add(ns_since(t0));
        }
        ins.report(pass == 0 ? "insert, ascending keys" : "insert, pseudo-random keys");
        if (pass == 1) {
            Lat er; er.reserve(N);
            for (std::size_t i = 0; i < N; ++i) {
                std::uint64_t k = perm[i];   // same order as the inserts
                auto t0 = Clock::now();
                s.erase(k);
                er.add(ns_since(t0));
            }
            er.report("erase, pseudo-random keys");
        }
    }
}
