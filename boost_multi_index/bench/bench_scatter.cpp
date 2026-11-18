// bench_scatter — the same ordered price index, the same data; only WHERE the nodes live differs.
//   arrival order : "sorted" = orders arrive in price-time order (node i+1 allocated right after node i)
//                   "random" = orders arrive at random prices (allocation order != price order)
//   allocator     : std::allocator (malloc) or std::pmr::monotonic_buffer_resource (bump arena)
// Workload: walk the price index from the best price, as a "best-N" query does.
// Reported: ns per node visited (samples = 1000-node chunks) and how often the next node in
// price order sits within one node (64 B) of the previous one.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bench/bench_scatter.cpp -o build/bench_scatter
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdlib>
#include <memory_resource>
#include "lat.hpp"

struct Order { std::uint64_t id; std::uint32_t client; char side; std::int64_t price; std::uint64_t ts; std::uint32_t qty; };
namespace bmi = boost::multi_index;
template <class A>
using Book = bmi::multi_index_container<Order,
    bmi::indexed_by<bmi::ordered_non_unique<bmi::key<&Order::price, &Order::ts>>>, A>;
constexpr std::size_t N = 2'000'000;

// Counts blocks the arena requests from its upstream, so the run shows whether N*64 + slack was enough.
struct CountingUpstream : std::pmr::memory_resource {
    std::size_t blocks = 0;
    void* do_allocate(std::size_t n, std::size_t a) override { ++blocks; return std::pmr::new_delete_resource()->allocate(n, a); }
    void do_deallocate(void* p, std::size_t n, std::size_t a) override { std::pmr::new_delete_resource()->deallocate(p, n, a); }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
};
// 64 B per node plus 64 B for the container's header node, plus slack for alignment.
constexpr std::size_t ARENA = N * 64 + 64 + 4096;

template <class C> void build(C& c, bool random_arrival) {
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t k = random_arrival ? (i * 2654435761u) % N : i;   // a permutation of 0..N-1
        c.insert(Order{k, 1, 'S', std::int64_t(k / 4), k, 1});
    }
}

template <class C> void walk(const char* name, C& c) {
    std::size_t adjacent = 0;
    const Order* prev = nullptr;
    for (auto& o : c) {
        if (prev) adjacent += std::llabs((const char*)&o - (const char*)prev) <= 64;  // node = 64 B
        prev = &o;
    }
    Lat l; std::uint64_t sink = 0;
    for (int rep = 0; rep < 5; ++rep) {
        auto it = c.begin();
        while (it != c.end()) {
            auto t0 = Clock::now();
            for (int k = 0; k < 1000 && it != c.end(); ++k, ++it) sink += it->qty;
            l.add(ns_since(t0) / 1000);
        }
    }
    keep(sink);
    std::printf("[next node within 64 B: %5.1f%%] ", 100.0 * double(adjacent) / double(N - 1));
    l.report(name);
}

int main() {
    std::printf("N=%zu orders; ns per node over 1000-node walks of the price index\n", N);
    { Book<std::allocator<Order>> c; build(c, false); walk("sorted arrival, std::allocator", c); }
    { Book<std::allocator<Order>> c; build(c, true);  walk("random arrival, std::allocator", c); }
    { CountingUpstream up; std::pmr::monotonic_buffer_resource a(ARENA, &up);
      Book<std::pmr::polymorphic_allocator<Order>> c(&a); build(c, false); walk("sorted arrival, pmr arena", c);
      std::printf("  (arena requested %zu block(s) from upstream)\n", up.blocks); }
    { CountingUpstream up; std::pmr::monotonic_buffer_resource a(ARENA, &up);
      Book<std::pmr::polymorphic_allocator<Order>> c(&a); build(c, true);  walk("random arrival, pmr arena", c);
      std::printf("  (arena requested %zu block(s) from upstream)\n", up.blocks); }
}
