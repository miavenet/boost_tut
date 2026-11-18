// Lab 5 — iterator / pointer stability per index type, and what random_access gives up.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l5_stability.cpp -o build/l5_stability
// Run:    ./build/l5_stability
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Order { std::uint64_t id; std::int64_t price; };

namespace bmi = boost::multi_index;
using Store = bmi::multi_index_container<
    Order,
    bmi::indexed_by<
        bmi::hashed_unique<bmi::key<&Order::id>>,     // 0
        bmi::ordered_non_unique<bmi::key<&Order::price>>,  // 1
        bmi::sequenced<>,                             // 2
        bmi::random_access<>>>;                       // 3

int main() {
    Store s;
    s.insert({42, 100});
    const Order* p = &*s.get<0>().find(42);
    auto it_hash = s.get<0>().find(42);
    std::size_t buckets_before = s.get<0>().bucket_count();
    std::size_t cap_before = s.get<3>().capacity();

    for (std::uint64_t i = 1000; i < 101000; ++i) s.insert({i, std::int64_t(i % 500)});  // rehashes + regrows

    std::printf("hashed buckets %zu -> %zu, random_access capacity %zu -> %zu\n", buckets_before,
                s.get<0>().bucket_count(), cap_before, s.get<3>().capacity());
    std::printf("pointer to element still valid?          %s\n",
                p == &*s.get<0>().find(42) && p->id == 42 ? "yes" : "NO");
    std::printf("hashed iterator still valid after rehash? %s\n", it_hash->id == 42 ? "yes (by spec)" : "NO");
    // Element address is the same seen through every index: there is one node.
    auto it_ra = std::find_if(s.get<3>().begin(), s.get<3>().end(), [](const Order& o) { return o.id == 42; });
    std::printf("same address through random_access?       %s\n", &*it_ra == p ? "yes" : "NO");

    // Contrast: std::vector moves its elements on growth -> old pointers dangle.
    std::vector<Order> v{{42, 100}};
    const Order* vp = v.data();
    for (std::uint64_t i = 0; i < 100000; ++i) v.push_back({i, 0});
    std::printf("std::vector element moved on growth?     %s\n", vp != v.data() ? "yes - old pointer dangles" : "no");

    // What random_access gives up #1: contiguity. Neighbours are separate heap nodes.
    auto& ra = s.get<3>();
    int contiguous = 0;
    for (std::size_t i = 0; i + 1 < 1000; ++i) contiguous += (&ra[i + 1] == &ra[i] + 1);
    std::printf("random_access: adjacent elements contiguous in memory: %d of 999\n", contiguous);

    // What random_access gives up #2: insert at the front shifts the whole pointer array.
    using RA = bmi::multi_index_container<Order, bmi::indexed_by<bmi::random_access<>>>;
    auto time_ms = [](auto&& f) {
        auto t0 = std::chrono::steady_clock::now(); f();
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    };
    RA front, back;
    double tf = time_ms([&] { for (std::uint64_t i = 0; i < 50000; ++i) front.push_front({i, 0}); });
    double tb = time_ms([&] { for (std::uint64_t i = 0; i < 50000; ++i) back.push_back({i, 0}); });
    std::printf("random_access 50000 x push_front: %.1f ms   50000 x push_back: %.1f ms\n", tf, tb);
    return 0;
}
