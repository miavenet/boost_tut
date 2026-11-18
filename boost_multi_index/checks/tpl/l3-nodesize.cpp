#include "common.hpp"
#include <memory>
struct Order { std::uint64_t id; std::uint32_t client; char side; std::int64_t price; std::uint64_t ts; std::uint32_t qty; };
static std::size_t node_bytes = 0;
template <class T> struct Spy {
    using value_type = T;
    Spy() = default;
    template <class U> Spy(const Spy<U>&) {}
    T* allocate(std::size_t n) { if (n == 1) node_bytes = sizeof(T); return std::allocator<T>{}.allocate(n); }
    void deallocate(T* p, std::size_t n) { std::allocator<T>{}.deallocate(p, n); }
    template <class U> bool operator==(const Spy<U>&) const { return true; }
};
using Store = multi_index_container<Order, indexed_by<
    hashed_unique<key<&Order::id>>,
    ordered_non_unique<key<&Order::side, &Order::price, &Order::ts>>>, Spy<Order>>;
constexpr long predicted =
/*ANSWER*/
;
int main() {
    Store s; s.insert(Order{});
    std::printf("sizeof(Order)=%zu, measured node=%zu, you predicted %ld\n", sizeof(Order), node_bytes, predicted);
    EXPECT(long(node_bytes) == predicted);
    std::printf("HIT\n");
    return 0;
}
