// l4_node_size.cpp — how big is one node, really? A recording resource prints the size of the
// most common allocation each container makes. Compare with a cache line:
// 128 B on this Mac (sysctl hw.cachelinesize), 64 B on typical x86-64 servers.
// Run: ./build/l4_node_size
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_node_map.hpp>

#include <cstdio>
#include <list>
#include <map>
#include <memory_resource>
#include <new>
#include <unordered_map>

struct recorder final : std::pmr::memory_resource {
    std::size_t sizes[1024] = {};   // histogram of request sizes < 1024 B
    void* do_allocate(std::size_t b, std::size_t a) override {
        if (b < 1024) ++sizes[b];
        return std::pmr::new_delete_resource()->allocate(b, a);
    }
    void do_deallocate(void* p, std::size_t b, std::size_t a) override {
        std::pmr::new_delete_resource()->deallocate(p, b, a);
    }
    bool do_is_equal(const std::pmr::memory_resource& o) const noexcept override { return this == &o; }
    std::size_t mode() const {
        std::size_t best = 0;
        for (std::size_t i = 0; i < 1024; ++i) if (sizes[i] > sizes[best]) best = i;
        return best;
    }
};

struct Order { long id; long px; long qty; };   // 24 bytes
namespace bmi = boost::multi_index;

template <class C, bool MapLike = true>
void show(const char* name) {
    recorder r;
    {
        C c{std::pmr::polymorphic_allocator<typename C::value_type>(&r)};
        for (long i = 0; i < 1000; ++i) {
            if constexpr (MapLike) c.emplace(i, Order{i, i, i});
            else c.insert(Order{i, i, i});
        }
    }
    std::size_t n = r.mode(), v = sizeof(typename C::value_type);
    std::printf("%-40s node=%3zu B  value_type=%2zu B  overhead=%2zu B  nodes per 64 B line: %.2f\n",
                name, n, v, n - v, 64.0 / n);
}

int main() {
    show<std::pmr::map<long, Order>>("std::pmr::map<long, Order>");
    show<std::pmr::unordered_map<long, Order>>("std::pmr::unordered_map<long, Order>");
    show<boost::unordered_node_map<long, Order, boost::hash<long>, std::equal_to<long>,
                                   std::pmr::polymorphic_allocator<std::pair<const long, Order>>>>(
        "boost::unordered_node_map<long, Order>");
    show<boost::multi_index_container<Order,
         bmi::indexed_by<bmi::hashed_unique<bmi::member<Order, long, &Order::id>>,
                         bmi::ordered_non_unique<bmi::member<Order, long, &Order::px>>>,
         std::pmr::polymorphic_allocator<Order>>, false>("multi_index<Order> (hashed + ordered)");
}
