// Lab 1b — the same order store as ONE boost::multi_index_container with three indices.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l1_mi.cpp -o build/l1_mi
// Run:    ./build/l1_mi
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

static long g_allocs = 0;
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Order {
    std::uint64_t id;
    std::uint32_t client;
    char side;
    std::int64_t price;
    std::uint64_t ts;
    std::uint32_t qty;
};

namespace bmi = boost::multi_index;
struct ById {};
struct ByPrice {};
struct ByClient {};

using OrderStore = bmi::multi_index_container<
    Order,
    bmi::indexed_by<
        bmi::hashed_unique<bmi::tag<ById>, bmi::member<Order, std::uint64_t, &Order::id>>,
        bmi::ordered_non_unique<bmi::tag<ByPrice>,
            bmi::composite_key<Order,
                bmi::member<Order, char, &Order::side>,
                bmi::member<Order, std::int64_t, &Order::price>,
                bmi::member<Order, std::uint64_t, &Order::ts>>>,
        bmi::hashed_non_unique<bmi::tag<ByClient>, bmi::member<Order, std::uint32_t, &Order::client>>>>;

int main() {
    OrderStore s;
    s.get<ById>().reserve(1024);
    s.get<ByClient>().reserve(1024);  // pre-size bucket arrays, same as the hand-rolled lab

    const int N = 1000;
    long before = g_allocs;
    for (int i = 0; i < N; ++i)
        s.insert(Order{std::uint64_t(i), std::uint32_t(i % 7), (i & 1) ? 'S' : 'B',
                       100 + i % 50, std::uint64_t(i), 10});
    std::printf("multi_index: allocations per insert() = %ld\n", (g_allocs - before) / N);

    before = g_allocs;
    auto& ids = s.get<ById>();
    for (int i = 0; i < N / 2; ++i) ids.erase(std::uint64_t(i));   // ONE erase, all 3 indices
    std::printf("multi_index: allocations per erase()  = %ld\n", (g_allocs - before) / (N / 2));
    std::printf("sizes: by_id=%zu by_price=%zu by_client=%zu (one container, cannot disagree)\n",
                s.get<ById>().size(), s.get<ByPrice>().size(), s.get<ByClient>().size());

    long qty = 0;
    auto [b, e] = s.get<ByClient>().equal_range(0u);
    for (; b != e; ++b) qty += b->qty;
    std::printf("client 0 open qty = %ld\n", qty);
    return 0;
}
