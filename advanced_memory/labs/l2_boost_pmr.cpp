// l2_boost_pmr.cpp — Boost's own pmr, std::pmr inside Boost containers, and the no-allocator
// options. Build line needs  -L/opt/homebrew/lib -lboost_container  (labs/build.sh adds it):
// boost::container::pmr's resources live in the compiled Boost.Container library.
// Run: ./build/l2_boost_pmr
#include <boost/container/flat_map.hpp>
#include <boost/container/pmr/flat_map.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include <cstdio>
#include <memory_resource>
#include <new>
#include <stdexcept>
#include <type_traits>

// Two DIFFERENT base classes. A std::pmr resource cannot feed a boost::container::pmr container.
static_assert(!std::is_same_v<boost::container::pmr::memory_resource, std::pmr::memory_resource>);

int main() {
    std::puts("1) boost::container::pmr::flat_map on boost's own monotonic resource");
    {
        boost::container::pmr::monotonic_buffer_resource mono;
        boost::container::pmr::flat_map<int, double> m(&mono);
        m.reserve(64);
        m[3] = 1.5;
        std::printf("  size=%zu\n", m.size());
    }

    std::puts("2) boost::container::flat_map with a STD pmr allocator (what hft_mem does)");
    {
        std::pmr::monotonic_buffer_resource mono;
        using A = std::pmr::polymorphic_allocator<std::pair<int, double>>;  // pair<K,V>, not pair<const K,V>
        boost::container::flat_map<int, double, std::less<int>, A> m(&mono);
        m.reserve(64);
        m[3] = 1.5;
        std::printf("  size=%zu, resource is &mono? %s\n", m.size(),
                    m.get_allocator().resource() == &mono ? "yes" : "no");
    }

    std::puts("3) small_vector<T,N>: N elements inline (no allocation), then falls back to its allocator");
    {
        boost::container::small_vector<int, 8> v;
        for (int i = 0; i < 8; ++i) v.push_back(i);
        const void* inline_buf = v.data();
        v.push_back(8);
        std::printf("  sizeof=%zu, data moved off the object after 9th push? %s\n", sizeof(v),
                    v.data() != inline_buf ? "yes" : "no");
    }

    std::puts("4) static_vector<T,N>: N elements inline, NEVER allocates; overflow throws");
    {
        boost::container::static_vector<int, 8> v;
        for (int i = 0; i < 8; ++i) v.push_back(i);
        try {
            v.push_back(8);
            std::puts("  no throw?!");
        } catch (const std::bad_alloc&) {
            std::puts("  9th push_back threw std::bad_alloc");
        } catch (const std::exception& e) {
            std::printf("  9th push_back threw a std::exception that is NOT std::bad_alloc: %s\n", e.what());
        }
        std::printf("  sizeof(static_vector<int,8>)=%zu\n", sizeof(v));
    }
}
