#include "common.hpp"
constexpr int kTicks = 1024;
/*ANSWER*/
int main() {
    std::array<std::uint32_t, kTicks> q{};
    EXPECT(next_best_bid(q, 1023) == -1);                // empty book
    q[500] = 3; q[10] = 1; q[0] = 2;
    EXPECT(next_best_bid(q, 1023) == 500);
    EXPECT(next_best_bid(q, 500) == 500);                // 'from' itself counts if non-empty
    EXPECT(next_best_bid(q, 499) == 10);
    EXPECT(next_best_bid(q, 9) == 0);
    q[0] = 0;
    EXPECT(next_best_bid(q, 9) == -1);
    std::printf("HIT\n");
    return 0;
}
