// l1_layout.cpp - see with your own eyes that a flat_map is ONE sorted array.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include l1_layout.cpp -o l1_layout
// PREDICT before running: (a) the byte distance between consecutive elements,
// (b) the order the keys print in, (c) how many keys move when you insert 5.
#include <boost/container/flat_map.hpp>
#include <cstdio>
#include <map>

int main() {
    boost::container::flat_map<int, double> m;
    for (int k : {40, 10, 30, 20}) m.emplace(k, k / 10.0);

    std::printf("sizeof(value_type) = %zu bytes\n", sizeof(decltype(m)::value_type));
    std::printf("size=%zu capacity=%zu\n", m.size(), m.capacity());
    const char* base = reinterpret_cast<const char*>(&*m.begin());
    for (auto& [k, v] : m)
        std::printf("  key %2d at offset +%td\n", k, reinterpret_cast<const char*>(&k) - base);

    // nth / index_of: random access, a Boost extension (std::map cannot do this in O(1)).
    std::printf("nth(2)->first = %d, index_of(find(30)) = %zu\n",
                m.nth(2)->first, m.index_of(m.find(30)));

    // Insert a key that sorts first: every existing pair shifts one slot right.
    const double* before = &m.find(10)->second;
    m.emplace(5, 0.5);
    std::printf("after emplace(5): key 10 is now at offset +%td (old address %s)\n",
                reinterpret_cast<const char*>(&m.find(10)->first) - reinterpret_cast<const char*>(&*m.begin()),
                before == &m.find(10)->second ? "UNCHANGED" : "CHANGED");

    std::map<int, double> sm{{40, 4}, {10, 1}, {30, 3}, {20, 2}};
    std::printf("std::map node addresses (no pattern, one allocation each):\n");
    for (auto& [k, v] : sm) std::printf("  key %2d at %p\n", k, static_cast<const void*>(&k));
}
