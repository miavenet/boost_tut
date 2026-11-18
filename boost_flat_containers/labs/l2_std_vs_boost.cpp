// l2_std_vs_boost.cpp - Boost flat_map (one sequence of pairs) vs C++23
// std::flat_map (two sequences: keys, values). Same job, different plumbing.
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include l2_std_vs_boost.cpp -o l2_std_vs_boost
#include <boost/container/flat_map.hpp>
#include <cstdio>
#include <flat_map>
#include <type_traits>
#include <vector>

int main() {
    // ---- Boost: a CONTAINER that owns one vector<pair<K,T>> -----------------
    using BFM = boost::container::flat_map<int, double>;
    static_assert(std::is_same_v<BFM::value_type, std::pair<int, double>>);
    static_assert(std::is_same_v<BFM::reference, std::pair<int, double>&>);      // a real reference

    BFM::sequence_type seq;                       // fill the raw sequence, sorted + unique...
    for (int i = 0; i < 5; ++i) seq.emplace_back(i * 10, i * 1.5);
    BFM b;
    b.adopt_sequence(boost::container::ordered_unique_range, std::move(seq));  // ...O(1) adopt
    b.reserve(100);                               // Boost has reserve/capacity
    std::printf("boost: size=%zu capacity=%zu\n", b.size(), b.capacity());
    BFM::sequence_type out = b.extract_sequence(); // take the vector back; b is now empty
    std::printf("boost: extracted %zu pairs, map now size=%zu\n", out.size(), b.size());

    // ---- std (C++23): an ADAPTOR over two containers ----------------------------
    using SFM = std::flat_map<int, double>;       // = flat_map<int,double,less<int>,vector<int>,vector<double>>
    static_assert(std::is_same_v<SFM::value_type, std::pair<int, double>>);
    static_assert(std::is_same_v<SFM::reference, std::pair<const int&, double&>>); // a PROXY by value

    std::vector<int> keys{0, 10, 20, 30, 40};
    std::vector<double> vals{0, 1.5, 3, 4.5, 6};
    SFM s(std::sorted_unique, std::move(keys), std::move(vals));   // O(N), trusts you
    std::printf("std:   keys() contiguous? %s  (keys().data()[3] = %d)\n",
                s.keys().data() + 3 == &s.keys()[3] ? "yes" : "no", s.keys().data()[3]);
    auto c = std::move(s).extract();              // containers{keys, values}; s now empty
    c.values[0] = 99;
    s.replace(std::move(c.keys), std::move(c.values));               // put them back
    std::printf("std:   after replace size=%zu, s[0]=%g\n", s.size(), s.at(0));

    // auto& p = *s.begin();  // does not compile: *it is a prvalue proxy pair
    auto p = *s.begin();      // fine: p.second is a double& into values()
    p.second = 7;
    std::printf("std:   wrote through proxy, s[0]=%g\n", s.at(0));
}
