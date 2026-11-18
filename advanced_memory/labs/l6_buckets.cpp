// l6_buckets.cpp — intrusive id lookup: unordered_set with YOUR bucket array, set (tree),
// key_of_value + find(id), erase_and_dispose into the slab.
// Run: ./labs/build/l6_buckets
#include "hft_mem/hft_slab.hpp"

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/unordered_set.hpp>

#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

namespace bi = boost::intrusive;

struct Order {
    long id;
    long px;
    bi::unordered_set_member_hook<> id_hook;                      // 8 B: hashed index
    bi::set_member_hook<bi::optimize_size<true>> tree_hook;       // 24 B: ordered index
};
struct id_of {                       // key_of_value: lets find() take a plain long
    using type = long;
    const long& operator()(const Order& o) const noexcept { return o.id; }
};
using IdHash = bi::unordered_set<Order,
    bi::member_hook<Order, bi::unordered_set_member_hook<>, &Order::id_hook>,
    bi::key_of_value<id_of>, bi::hash<std::hash<long>>, bi::power_2_buckets<true>>;
using IdTree = bi::set<Order,
    bi::member_hook<Order, bi::set_member_hook<bi::optimize_size<true>>, &Order::tree_hook>,
    bi::key_of_value<id_of>>;

constexpr long N = 50'000, FINDS = 1'000'000;

template <class C>
double mean_find_ns(C& c) {
    long hits = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (long i = 0; i < FINDS; ++i) hits += c.find((i * 7919) % N) != c.end();
    auto t1 = std::chrono::steady_clock::now();
    if (hits != FINDS) std::puts("BUG: missing ids");
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / FINDS;
}

int main() {
    hft::thread_arena arena;
    hft::slab<Order> orders(N);

    std::puts("== unordered_set: the bucket array is yours; it never rehashes by itself ==");
    std::vector<IdHash::bucket_type> small(64), big(65536);   // power of two (power_2_buckets)
    {
        IdHash ids(IdHash::bucket_traits(small.data(), small.size()));
        for (long i = 0; i < N; ++i) ids.insert(*orders.create(Order{i, i % 100, {}, {}}));
        std::printf("size=%zu  bucket_count=%zu  (load factor %.0f: nothing grew)\n",
                    ids.size(), ids.bucket_count(), double(ids.size()) / ids.bucket_count());
        std::printf("mean find(id), 64 buckets:     %6.1f ns\n", mean_find_ns(ids));
        ids.rehash(IdHash::bucket_traits(big.data(), big.size()));   // YOU choose when, with YOUR array
        std::printf("after rehash to 65536 buckets: %6.1f ns   (bucket_count=%zu)\n",
                    mean_find_ns(ids), ids.bucket_count());

        std::puts("\n== set (red-black tree) by id: O(log n) ==");
        IdTree tree;
        for (auto& o : ids) tree.insert(o);                 // the SAME orders, a second hook
        std::printf("mean find(id), tree of %zu:     %6.1f ns\n", tree.size(), mean_find_ns(tree));

        std::puts("\n== erase_and_dispose: unlink and hand the object back to the slab ==");
        tree.clear();                                        // unlink from the tree first
        long before = orders.size();
        auto it = ids.find(42);
        ids.erase_and_dispose(it, [&](Order* o) { orders.destroy(o); });
        std::printf("slab live: %ld -> %u; find(42) now %s\n", before, orders.size(),
                    ids.find(42) == ids.end() ? "end()" : "found");
        ids.clear_and_dispose([&](Order* o) { orders.destroy(o); });
        std::printf("after clear_and_dispose: slab live %u\n", orders.size());
    }   // container dies BEFORE its bucket arrays (declared after them): required by the docs
}
