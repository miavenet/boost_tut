// Lab 4 — modify() vs replace(), and the collision that silently ERASES your order.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l4_modify.cpp -o build/l4_modify
// Run:    ./build/l4_modify
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdint>
#include <cstdio>

struct Order {
    std::uint64_t id;          // exchange order id
    std::uint32_t client;
    std::uint32_t cl_ord_id;   // client's own id; (client, cl_ord_id) must be unique
    std::int64_t price;
    std::uint32_t qty;
};

namespace bmi = boost::multi_index;
using Store = bmi::multi_index_container<
    Order,
    bmi::indexed_by<
        bmi::hashed_unique<bmi::key<&Order::id>>,                          // index 0
        bmi::ordered_unique<bmi::key<&Order::client, &Order::cl_ord_id>>,  // index 1
        bmi::ordered_non_unique<bmi::key<&Order::price>>>>;                // index 2

void dump(const char* when, const Store& s) {
    std::printf("%-44s size=%zu :", when, s.size());
    for (const Order& o : s.get<2>())
        std::printf(" [id%llu c%u/%u px%lld q%u]", (unsigned long long)o.id, o.client, o.cl_ord_id,
                    (long long)o.price, o.qty);
    std::printf("\n");
}

int main() {
    Store s;
    s.insert({1, 7, 100, 50, 10});
    s.insert({2, 7, 101, 51, 10});
    s.insert({3, 9, 100, 52, 10});
    dump("start", s);

    auto& by_id = s.get<0>();

    // 1. modify() a key in place: element re-sorted in the price index, no copy of Order.
    bool ok = by_id.modify(by_id.find(1), [](Order& o) { o.price = 53; });
    std::printf("modify id1 price->53 returned %s\n", ok ? "true" : "false");
    dump("after price change", s);

    // 2. replace() with a colliding (client, cl_ord_id): refused, container untouched.
    Order copy = *by_id.find(2);
    copy.cl_ord_id = 100;                       // (7,100) already belongs to id1
    ok = by_id.replace(by_id.find(2), copy);
    std::printf("replace id2 cl_ord_id->100 returned %s\n", ok ? "true" : "false");
    dump("after colliding replace", s);

    // 3. modify() with the same collision and NO rollback: the element is ERASED.
    ok = by_id.modify(by_id.find(2), [](Order& o) { o.cl_ord_id = 100; });
    std::printf("modify id2 cl_ord_id->100 returned %s\n", ok ? "true" : "false");
    dump("after colliding modify (no rollback)", s);
    std::printf("find(2) == end()? %s\n", by_id.find(2) == by_id.end() ? "YES - order 2 is gone" : "no");

    // 4. modify() with a rollback functor: collision detected, old value restored, element kept.
    s.insert({2, 7, 101, 51, 10});
    std::uint32_t old = 101;
    ok = by_id.modify(by_id.find(2),
                      [](Order& o) { o.cl_ord_id = 100; },
                      [old](Order& o) { o.cl_ord_id = old; });
    std::printf("modify+rollback id2 cl_ord_id->100 returned %s\n", ok ? "true" : "false");
    dump("after colliding modify (with rollback)", s);
    return 0;
}
