// l2_mutable_key.cpp - Boost's value_type is std::pair<Key, T>: the key is NOT const.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include l2_mutable_key.cpp -o l2_mutable_key
// PREDICT: does the marked line compile? If so, what do the two finds print?
#include <boost/container/flat_map.hpp>
#include <cstdio>
#include <type_traits>
#include <utility>

int main() {
    using FM = boost::container::flat_map<int, int>;
    static_assert(std::is_same_v<FM::value_type, std::pair<int, int>>);   // not pair<const int,int>

    FM m{{10, 1}, {20, 2}, {30, 3}};
    auto it = m.find(10);
    it->first = 99;            // <-- compiles. With std::map or std::flat_map it does not.

    for (auto& [k, v] : m) std::printf("%d ", k);       // the sort invariant is now broken
    std::printf("\nfind(99) %s, find(20) %s\n",
                m.find(99) != m.end() ? "found" : "NOT found",
                m.find(20) != m.end() ? "found" : "NOT found");
}
