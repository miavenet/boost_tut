// l2_boost_pool.cpp — the older Boost.Pool options, and the costs the docs warn about.
// Run: ./build/l2_boost_pool
#include <boost/pool/object_pool.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/singleton_pool.hpp>

#include <cstdio>
#include <list>
#include <mutex>
#include <type_traits>
#include <vector>

// pool_allocator / fast_pool_allocator default to details::pool::default_mutex, which this
// toolchain (threads on, C++11 <mutex>) defines as std::mutex: every allocate takes a lock.
static_assert(std::is_same_v<boost::details::pool::default_mutex, std::mutex>);

struct Order { long id; long px; };

int main() {
    std::puts("1) boost::pool<>: one block size, an object you own, no lock");
    {
        boost::pool<> p(sizeof(Order));
        void* a = p.malloc();
        p.free(a);
        void* b = p.malloc();
        std::printf("  reused freed block? %s\n", a == b ? "yes" : "no");
    }   // all its memory goes back when p is destroyed

    std::puts("2) object_pool<T>: construct/destroy T; the pool destroys leftovers at its end");
    {
        boost::object_pool<Order> op;
        Order* o = op.construct(Order{1, 100});
        std::printf("  constructed id=%ld, is_from(pool)? %s\n", o->id, op.is_from(o) ? "yes" : "no");
        // (no op.destroy(o): ~object_pool calls the destructor for us)
    }

    std::puts("3) pool_allocator (a SINGLETON pool): memory outlives the container");
    {
        std::vector<Order, boost::pool_allocator<Order>> v;
        for (long i = 0; i < 1000; ++i) v.push_back({i, i});
    }   // vector gone ... the process-wide singleton pool still holds the memory
    using S = boost::singleton_pool<boost::pool_allocator_tag, sizeof(Order)>;
    bool freed = S::release_memory();   // pool_allocator uses ordered allocation, as release_memory requires
    std::printf("  release_memory() after the vector died freed something? %s\n", freed ? "yes" : "no");

    std::puts("4) ordered_free keeps the free list sorted: freeing can be O(free blocks)");
    {
        boost::pool<> p(64);
        std::vector<void*> v;
        for (int i = 0; i < 4; ++i) v.push_back(p.ordered_malloc());
        for (void* x : v) p.ordered_free(x);
        std::puts("  ordered_malloc/ordered_free work; use them only when you need arrays (ordered_malloc(n))");
    }
}
