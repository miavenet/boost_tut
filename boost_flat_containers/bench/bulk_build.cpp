// bulk_build.cpp - three ways to fill a flat_map with N random keys, plus
// two ways to apply a batch of updates. Reports p50/p99/max over repeats (ms).
//
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include bulk_build.cpp -o bulk_build
#include "lat.hpp"
#include <boost/container/flat_map.hpp>

using FM = boost::container::flat_map<uint64_t, uint64_t>;
using Seq = FM::sequence_type;  // boost::container::vector<std::pair<uint64_t,uint64_t>>

template <class F> void row(const char* name, int reps, F&& f) {
    std::vector<double> ms;
    for (int r = 0; r < reps; ++r) { uint64_t t0 = lat::now_ns(); f(); ms.push_back(double(lat::now_ns() - t0) / 1e6); }
    auto s = lat::summarize(ms);
    std::printf("%-46s p50 %9.3f  p99 %9.3f  max %9.3f ms\n", name, s.p50, s.p99, s.max);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[built %s]\nApple M1 Max, Apple clang 21, -O2, Boost 1.90. uint64->uint64.\n", lat::build_flags());
    for (int N : {10000, 100000}) {
        lat::Rng rng(N);
        std::vector<std::pair<uint64_t, uint64_t>> input(N);
        for (auto& p : input) p = {rng.next(), 1};   // random order, (almost surely) unique
        std::printf("\nN = %d\n", N);
        int reps = N == 10000 ? 21 : 5;
        row("A insert one by one (random order)", reps, [&] {
            FM m; for (auto& p : input) m.insert(p); lat::keep(m.size()); });
        row("B sort copy, ctor(ordered_unique_range,...)", reps, [&] {
            auto v = input; std::sort(v.begin(), v.end());
            FM m(boost::container::ordered_unique_range, v.begin(), v.end()); lat::keep(m.size()); });
        row("C fill sequence, adopt_sequence(ordered_..)", reps, [&] {
            Seq s(input.begin(), input.end()); std::sort(s.begin(), s.end());
            FM m; m.adopt_sequence(boost::container::ordered_unique_range, std::move(s)); lat::keep(m.size()); });
        row("D range ctor (unsorted input, Boost sorts)", reps, [&] {
            FM m(input.begin(), input.end()); lat::keep(m.size()); });

        // Batching: apply 1000 new keys to a table that already holds N keys.
        FM base(input.begin(), input.end());
        std::vector<std::pair<uint64_t, uint64_t>> upd(1000);
        for (auto& p : upd) p = {rng.next(), 2};
        row("E 1000 updates: insert one by one", reps, [&] {
            FM m = base; for (auto& p : upd) m.insert(p); lat::keep(m.size()); });
        row("F 1000 updates: sort, insert(ordered_unique..)", reps, [&] {
            FM m = base; auto u = upd; std::sort(u.begin(), u.end());
            m.insert(boost::container::ordered_unique_range, u.begin(), u.end()); lat::keep(m.size()); });
        row("  (copy of base alone, subtract from E/F)", reps, [&] { FM m = base; lat::keep(m.size()); });
    }
}
