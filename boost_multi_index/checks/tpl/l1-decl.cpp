#include "common.hpp"
struct Order { std::uint64_t id; std::int64_t price; std::uint32_t qty; };
/*ANSWER*/
int main() {
    Store s;
    EXPECT(s.insert(Order{1, 100, 5}).second);
    EXPECT(s.insert(Order{2, 99, 5}).second);
    EXPECT(!s.insert(Order{1, 101, 5}).second);          // id must be unique
    EXPECT(s.insert(Order{3, 100, 5}).second);           // price may repeat
    EXPECT(s.get<0>().bucket_count() > 0);               // index 0 is hashed
    EXPECT(s.get<0>().find(std::uint64_t(2))->price == 99);
    auto lb = s.get<1>().lower_bound(std::int64_t(100)); // index 1 is ordered by price
    EXPECT(lb != s.get<1>().end() && lb->price == 100);
    EXPECT(s.get<1>().begin()->id == 2);
    EXPECT(s.get<1>().count(std::int64_t(100)) == 2);
    std::printf("HIT\n");
    return 0;
}
