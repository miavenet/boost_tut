// Lab 3 — measure, don't assert: node size and bytes/element for each index mix.
// A spying allocator (the allocator template parameter!) records every allocation.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l3_node_size.cpp -o build/l3_node_size
// Run:    ./build/l3_node_size
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/ranked_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <memory>
#include <unordered_map>

struct Order {
    std::uint64_t id;
    std::uint32_t client;
    char side;
    std::int64_t price;
    std::uint64_t ts;
    std::uint32_t qty;
};

// ---- spying allocator: counts single-object allocations (nodes) and all live bytes ----
struct Spy {
    static inline std::size_t node_size = 0, node_allocs = 0, other_allocs = 0, live_bytes = 0;
    static void reset() { node_size = node_allocs = other_allocs = live_bytes = 0; }
};
template <class T>
struct SpyAlloc {
    using value_type = T;
    SpyAlloc() = default;
    template <class U> SpyAlloc(const SpyAlloc<U>&) {}
    T* allocate(std::size_t n) {
        if (n == 1) { ++Spy::node_allocs; Spy::node_size = sizeof(T); }
        else ++Spy::other_allocs;               // bucket arrays, pointer arrays, headers
        Spy::live_bytes += n * sizeof(T);
        return std::allocator<T>{}.allocate(n);
    }
    void deallocate(T* p, std::size_t n) {
        Spy::live_bytes -= n * sizeof(T);
        std::allocator<T>{}.deallocate(p, n);
    }
    template <class U> bool operator==(const SpyAlloc<U>&) const { return true; }
};

namespace bmi = boost::multi_index;
template <class... Idx>
using MI = bmi::multi_index_container<Order, bmi::indexed_by<Idx...>, SpyAlloc<Order>>;

using IdOrd  = bmi::ordered_unique<bmi::key<&Order::id>>;
using IdHash = bmi::hashed_unique<bmi::key<&Order::id>>;
using PxOrd  = bmi::ordered_non_unique<bmi::key<&Order::side, &Order::price, &Order::ts>>;
using ClHash = bmi::hashed_non_unique<bmi::key<&Order::client>>;
using IdRank = bmi::ranked_unique<bmi::key<&Order::id>>;

constexpr int N = 100000;

template <class C, class F>
void measure(const char* name, F insert_all) {
    Spy::reset();
    {
        C c;
        insert_all(c);
        std::printf("%-34s node=%4zu B  allocs/elem=%.2f  live bytes/elem=%6.1f\n", name,
                    Spy::node_size, double(Spy::node_allocs) / N, double(Spy::live_bytes) / N);
    }
}

template <class C>
void mi(const char* name) {
    measure<C>(name, [](C& c) {
        for (int i = 0; i < N; ++i)
            c.insert(c.end(), Order{std::uint64_t(i) * 7919 % N, std::uint32_t(i % 50), 'B', i % 100, std::uint64_t(i), 1});
    });
}

int main() {
    std::printf("sizeof(Order) = %zu, sizeof(void*) = %zu, N = %d\n", sizeof(Order), sizeof(void*), N);
    std::printf("-- one index --\n");
    mi<MI<IdOrd>>("ordered_unique");
    mi<MI<IdHash>>("hashed_unique");
    mi<MI<bmi::sequenced<>>>("sequenced");
    mi<MI<bmi::random_access<>>>("random_access");
    mi<MI<IdRank>>("ranked_unique");
    std::printf("-- several indices --\n");
    mi<MI<IdHash, PxOrd>>("hashed + ordered");
    mi<MI<IdHash, PxOrd, ClHash>>("hashed + ordered + hashed");
    mi<MI<IdOrd, PxOrd, IdRank>>("ordered + ordered + ranked");
    std::printf("-- std:: for comparison (one access path each) --\n");
    measure<std::map<std::uint64_t, Order, std::less<>, SpyAlloc<std::pair<const std::uint64_t, Order>>>>(
        "std::map<id,Order>", [](auto& c) { for (int i = 0; i < N; ++i) c.emplace(i, Order{}); });
    measure<std::unordered_map<std::uint64_t, Order, std::hash<std::uint64_t>, std::equal_to<>,
                               SpyAlloc<std::pair<const std::uint64_t, Order>>>>(
        "std::unordered_map<id,Order>", [](auto& c) { for (int i = 0; i < N; ++i) c.emplace(i, Order{}); });
    measure<std::list<Order, SpyAlloc<Order>>>(
        "std::list<Order>", [](auto& c) { for (int i = 0; i < N; ++i) c.push_back(Order{}); });

    // The hand-rolled store from Lab 1 (3 containers, 2 of them holding Order*).
    std::printf("-- hand-rolled 3-container store (Lab 1a) vs MI with the same 3 paths --\n");
    using PK = std::tuple<char, std::int64_t, std::uint64_t>;
    struct HandRolled {
        std::unordered_map<std::uint64_t, Order, std::hash<std::uint64_t>, std::equal_to<>,
                           SpyAlloc<std::pair<const std::uint64_t, Order>>> by_id;
        std::map<PK, Order*, std::less<>, SpyAlloc<std::pair<const PK, Order*>>> by_price;
        std::unordered_multimap<std::uint32_t, Order*, std::hash<std::uint32_t>, std::equal_to<>,
                                SpyAlloc<std::pair<const std::uint32_t, Order*>>> by_client;
    };
    Spy::reset();
    {
        HandRolled h;
        for (int i = 0; i < N; ++i) {
            Order o{std::uint64_t(i), std::uint32_t(i % 50), 'B', i % 100, std::uint64_t(i), 1};
            Order* p = &h.by_id.emplace(o.id, o).first->second;
            h.by_price.emplace(PK{o.side, o.price, o.ts}, p);
            h.by_client.emplace(o.client, p);
        }
        std::printf("%-34s allocs/elem=%.2f  live bytes/elem=%6.1f\n", "hand-rolled (3 containers)",
                    double(Spy::node_allocs) / N, double(Spy::live_bytes) / N);
    }
    return 0;
}
