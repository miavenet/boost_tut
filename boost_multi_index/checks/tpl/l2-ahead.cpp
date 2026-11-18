#include "common.hpp"
struct Order { std::uint64_t id; char side; std::int64_t price; std::uint64_t ts; };
struct ById {}; struct ByBook {};
using Store = multi_index_container<Order, indexed_by<
    hashed_unique<tag<ById>, key<&Order::id>>,
    ordered_non_unique<tag<ByBook>, key<&Order::side, &Order::price, &Order::ts>>>>;
/*ANSWER*/
int main() {
    Store s;
    std::uint64_t t = 0;
    for (std::uint64_t id = 1; id <= 40; ++id)
        s.insert({id, (id % 3) ? 'B' : 'S', std::int64_t(100 + id % 4), ++t});
    for (std::uint64_t id = 1; id <= 40; ++id) {
        const Order& me = *s.get<ById>().find(id);
        int brute = 0;
        for (const Order& o : s) brute += o.side == me.side && o.price == me.price && o.ts < me.ts;
        if (ahead(s, id) != brute) { std::printf("MISS: ahead(s,%llu)=%d, expected %d\n",
                                                (unsigned long long)id, ahead(s, id), brute); return 1; }
    }
    std::printf("HIT\n");
    return 0;
}
