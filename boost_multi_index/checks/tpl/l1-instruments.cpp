#include "common.hpp"
struct Instrument { std::string symbol; int exchange; std::uint32_t local_id; double tick; };
/*ANSWER*/
int main() {
    Instruments t;
    EXPECT(t.insert({"ESZ6", 1, 10, 0.25}).second);
    EXPECT(t.insert({"NQZ6", 1, 11, 0.25}).second);
    EXPECT(t.insert({"FDAX", 2, 10, 0.5}).second);       // same local_id, other exchange: fine
    EXPECT(!t.insert({"ESZ6", 3, 99, 0.1}).second);      // symbol must be unique
    EXPECT(!t.insert({"XXXX", 1, 11, 0.1}).second);      // (exchange, local_id) must be unique
    EXPECT(t.get<0>().bucket_count() > 0);               // index 0 hashed on symbol
    EXPECT(t.get<0>().find(std::string("FDAX"))->exchange == 2);
    auto& ix = t.get<1>();
    EXPECT(ix.find(std::make_tuple(1, 11u))->symbol == "NQZ6");
    auto [b, e] = ix.equal_range(std::make_tuple(1));    // partial key: all of exchange 1
    int n = 0; for (; b != e; ++b) ++n;
    EXPECT(n == 2);
    std::printf("HIT\n");
    return 0;
}
