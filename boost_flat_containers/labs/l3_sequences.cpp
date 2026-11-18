// l3_sequences.cpp - swap the sequence under a flat_map and count heap allocations.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include l3_sequences.cpp -o l3_sequences
// PREDICT for each table: allocations after 16 inserts? after the 17th insert?
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <cstdio>
#include <cstdlib>
#include <new>

static long g_allocs = 0;                     // every global operator new bumps this
void* operator new(std::size_t n) { ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace bc = boost::container;
using P = std::pair<int, int>;
using Heap   = bc::flat_map<int, int>;                                       // vector<P>: heap
using Small  = bc::flat_map<int, int, std::less<int>, bc::small_vector<P, 16>>;  // 16 inline, then heap
using Static = bc::flat_map<int, int, std::less<int>, bc::static_vector<P, 16>>; // 16 inline, never heap

template <class Table> void trial(const char* name) {
    Table t;
    long a0 = g_allocs;
    for (int i = 0; i < 16; ++i) t.emplace(i, i);
    long a16 = g_allocs - a0;
    try {
        t.emplace(16, 16);                     // the 17th element
        std::printf("%-7s sizeof=%4zu  allocs after 16: %ld  after 17th: %ld  size=%zu\n",
                    name, sizeof(Table), a16, g_allocs - a0, t.size());
    } catch (const std::bad_alloc&) {
        std::printf("%-7s 17th insert threw std::bad_alloc\n", name);
    } catch (const std::exception& e) {
        std::printf("%-7s sizeof=%4zu  allocs after 16: %ld  17th insert threw: \"%s\" (size still %zu)\n",
                    name, sizeof(Table), a16, e.what(), t.size());
    }
}

int main() {
    trial<Heap>("Heap");
    trial<Small>("Small");
    trial<Static>("Static");
}
