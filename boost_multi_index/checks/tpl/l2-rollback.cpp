#include "common.hpp"
struct Order { std::uint64_t id; std::uint32_t client; std::uint32_t cl_ord_id; std::int64_t price; };
using Store = multi_index_container<Order, indexed_by<
    hashed_unique<key<&Order::id>>,
    ordered_unique<key<&Order::client, &Order::cl_ord_id>>>>;
/*ANSWER*/
int main() {
    Store s;
    s.insert({1, 7, 100, 50});
    s.insert({2, 7, 101, 51});
    EXPECT(!set_cl_ord_id(s, 2, 100));                   // collides with order 1
    EXPECT(s.size() == 2);                               // ...and order 2 must survive
    EXPECT(s.get<0>().find(std::uint64_t(2))->cl_ord_id == 101);
    EXPECT(set_cl_ord_id(s, 2, 102));                    // no collision: succeeds
    EXPECT(s.get<0>().find(std::uint64_t(2))->cl_ord_id == 102);
    EXPECT(s.get<1>().find(std::make_tuple(7u, 102u))->id == 2);
    EXPECT(!set_cl_ord_id(s, 9, 1));                     // unknown id: false, no crash
    std::printf("HIT\n");
    return 0;
}
