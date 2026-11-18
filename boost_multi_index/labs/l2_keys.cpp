// Lab 2 — key extractors, tags, get<>, composite lookups and project<>.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l2_keys.cpp -o build/l2_keys
// Run:    ./build/l2_keys
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>          // C++17 key<> shorthand
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <type_traits>

struct Order {
    std::uint64_t id;
    std::uint32_t client;
    char side;            // 'B' or 'S'
    std::int64_t price;
    std::uint64_t ts;
    std::uint32_t qty;
    // "Aggressiveness": smaller = better. Bids: higher price is better, so negate.
    std::int64_t rank() const { return side == 'B' ? -price : price; }
};

namespace bmi = boost::multi_index;
struct ById {};
struct ByBook {};
struct ByClient {};

// Long-hand spelling: member<>, const_mem_fun<>, composite_key<>.
using LongHand = bmi::multi_index_container<
    Order,
    bmi::indexed_by<
        bmi::hashed_unique<bmi::tag<ById>, bmi::member<Order, std::uint64_t, &Order::id>>,
        bmi::ordered_non_unique<bmi::tag<ByBook>,
            bmi::composite_key<Order,
                bmi::member<Order, char, &Order::side>,
                bmi::const_mem_fun<Order, std::int64_t, &Order::rank>,
                bmi::member<Order, std::uint64_t, &Order::ts>>>,
        bmi::hashed_non_unique<bmi::tag<ByClient>, bmi::member<Order, std::uint32_t, &Order::client>>>>;

// C++17 shorthand: key<&Order::id> resolves to member<...>; key<a,b,c> to composite_key<...>.
using ShortHand = bmi::multi_index_container<
    Order,
    bmi::indexed_by<
        bmi::hashed_unique<bmi::tag<ById>, bmi::key<&Order::id>>,
        bmi::ordered_non_unique<bmi::tag<ByBook>, bmi::key<&Order::side, &Order::rank, &Order::ts>>,
        bmi::hashed_non_unique<bmi::tag<ByClient>, bmi::key<&Order::client>>>>;

static_assert(std::is_same_v<bmi::key<&Order::id>, bmi::member<Order, std::uint64_t, &Order::id>>);
static_assert(std::is_same_v<bmi::key<&Order::rank>, bmi::const_mem_fun<Order, std::int64_t, &Order::rank>>);

template <class Store>
void demo(const char* name) {
    Store s;
    std::uint64_t t = 0;
    auto add = [&](std::uint64_t id, std::uint32_t c, char side, std::int64_t px, std::uint32_t q) {
        s.insert(Order{id, c, side, px, ++t, q});
    };
    add(1, 7, 'B', 100, 5);
    add(2, 8, 'B', 101, 3);   // best bid
    add(3, 7, 'S', 103, 4);
    add(4, 9, 'B', 101, 2);   // same price as #2, later in time
    add(5, 8, 'S', 102, 6);   // best ask
    add(6, 7, 'B', 99, 1);

    std::printf("== %s ==\n", name);
    // Best 3 bids in price-time priority: partial composite lookup on side only.
    auto& book = s.template get<ByBook>();
    auto [b, e] = book.equal_range(std::make_tuple('B'));
    std::printf("best bids:");
    for (int n = 0; b != e && n < 3; ++b, ++n) std::printf(" #%llu@%lld", (unsigned long long)b->id, (long long)b->price);
    std::printf("\n");

    // Everything for client 7 (hashed_non_unique).
    std::printf("client 7 orders:");
    auto [cb, ce] = s.template get<ByClient>().equal_range(7u);
    for (; cb != ce; ++cb) std::printf(" #%llu", (unsigned long long)cb->id);
    std::printf("  (hashed: order unspecified)\n");

    // project<>: find order 4 by id, jump to its position in the book, count orders ahead at its level.
    auto it_id = s.template get<ById>().find(4);
    auto it_book = s.template project<ByBook>(it_id);
    int ahead = 0;
    for (auto p = book.begin(); p != it_book; ++p)
        if (p->side == it_book->side && p->price == it_book->price) ++ahead;
    std::printf("order #4 has %d order(s) ahead of it at %lld\n", ahead, (long long)it_book->price);

    // get<N>() by number works too: index 0 is ById.
    std::printf("get<0>().size() = %zu\n", s.template get<0>().size());
}

int main() {
    demo<LongHand>("long-hand");
    demo<ShortHand>("key<> shorthand");
    return 0;
}
