// Expected: compiles and prints "value_type=Order same_as_Order_alloc=0 resource_match=1 size=1000" (confirming polymorphic_allocator<> works via rebind but fails l4-alloc.cpp's strict is_same_v check).
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <cstdint>
#include <cstdio>
#include <memory_resource>
#include <type_traits>

struct Order { std::uint64_t id; std::int64_t price; };
namespace bmi = boost::multi_index;
template <class Alloc = std::allocator<Order>>
using Book = bmi::multi_index_container<Order, bmi::indexed_by<bmi::ordered_non_unique<bmi::key<&Order::price>>>, Alloc>;

int main() {
    std::pmr::monotonic_buffer_resource arena(1 << 20);
    Book<std::pmr::polymorphic_allocator<>> book(&arena);
    using A = typename decltype(book)::allocator_type;
    for (std::uint64_t i = 0; i < 1000; ++i) book.insert(Order{i, std::int64_t(i % 13)});
    std::printf("same_as_Order_alloc=%d resource_match=%d size=%zu\n",
                std::is_same_v<A, std::pmr::polymorphic_allocator<Order>> ? 1 : 0,
                book.get_allocator().resource() == &arena ? 1 : 0,
                book.size());
    return 0;
}
