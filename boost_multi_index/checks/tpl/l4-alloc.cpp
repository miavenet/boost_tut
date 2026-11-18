#include "common.hpp"
struct Order { std::uint64_t id; std::int64_t price; };
template <class Alloc = std::allocator<Order>>
using Book = multi_index_container<Order, indexed_by<ordered_non_unique<key<&Order::price>>>, Alloc>;
template <class T> struct is_pmr_alloc : std::false_type {};
template <class T> struct is_pmr_alloc<std::pmr::polymorphic_allocator<T>> : std::true_type {};
int main() {
    std::pmr::monotonic_buffer_resource arena(1 << 20);
    /*ANSWER*/
    using A = typename std::remove_cvref_t<decltype(book)>::allocator_type;
    // Any polymorphic_allocator<T> is accepted: Book<std::pmr::polymorphic_allocator<>> (T = std::byte)
    // is just as valid, because the container rebinds the allocator to its node type.
    if constexpr (!is_pmr_alloc<A>::value) {
        std::printf("MISS: book's allocator_type is not a std::pmr::polymorphic_allocator\n");
        return 1;
    } else {
        EXPECT(book.get_allocator().resource() == &arena);
        for (std::uint64_t i = 0; i < 1000; ++i) book.insert(Order{i, std::int64_t(i % 13)});
        EXPECT(book.size() == 1000);
        std::printf("HIT\n");
        return 0;
    }
}
