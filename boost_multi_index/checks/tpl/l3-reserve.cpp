#include "common.hpp"
struct Order { std::uint64_t id; std::uint32_t client; std::int64_t price; };
using Store = multi_index_container<Order, indexed_by<
    hashed_unique<key<&Order::id>>,
    ordered_non_unique<key<&Order::price>>,
    hashed_non_unique<key<&Order::client>>>>;
/*ANSWER*/
int main() {
    for (std::size_t n : {1000u, 50000u}) {
        Store s;
        prepare(s, n);
        std::size_t b0 = s.get<0>().bucket_count(), b2 = s.get<2>().bucket_count();
        for (std::size_t i = 0; i < n; ++i) s.insert({i, std::uint32_t(i), std::int64_t(i % 7)});
        EXPECT(s.size() == n);
        EXPECT(s.get<0>().bucket_count() == b0);         // no rehash of the id index
        EXPECT(s.get<2>().bucket_count() == b2);         // no rehash of the client index
    }
    std::printf("HIT\n");
    return 0;
}
