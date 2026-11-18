// eytzinger.cpp - large sorted-set membership: sorted array vs Eytzinger (BFS) layout.
// Layout + branchless search after Khuong & Morin, "Array Layouts for
// Comparison-Based Searching", https://arxiv.org/abs/1509.05053
// Reports ns per lookup as p50 | p99 of 64-lookup batch averages.
//
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include eytzinger.cpp -o eytzinger
#include "lat.hpp"
#include <boost/container/flat_set.hpp>

// b[1..n] holds the sorted keys in breadth-first (heap) order; b[0] unused.
static size_t fill(const std::vector<uint64_t>& s, std::vector<uint64_t>& b, size_t i, size_t k) {
    if (k < b.size()) { i = fill(s, b, i, 2 * k); b[k] = s[i++]; i = fill(s, b, i, 2 * k + 1); }
    return i;
}
// Returns the Eytzinger index of the first element >= x (0 if none).
static inline size_t eytz_lower_bound(const uint64_t* b, size_t n, uint64_t x) {
    size_t k = 1;
    while (k <= n) {
        __builtin_prefetch(b + 16 * k);          // 16 levels-ahead block of descendants
        k = 2 * k + (b[k] < x);                  // no branch on the comparison
    }
    k >>= __builtin_ffsll((long long)~k);        // undo the final run of right turns
    return k;
}

template <class F> void run(const char* name, const std::vector<uint64_t>& probes, F&& f) {
    constexpr int B = 64, S = 4000;
    std::vector<double> v; v.reserve(S);
    size_t pi = 0; uint64_t sink = 0;
    for (int w = 0; w < 200 * B; ++w) sink += f(probes[(pi++) % probes.size()]);   // warm caches/predictors
    for (int s = 0; s < S; ++s) {
        uint64_t t0 = lat::now_ns();
        for (int i = 0; i < B; ++i) sink += f(probes[(pi++) % probes.size()]);
        v.push_back(double(lat::now_ns() - t0) / B);
    }
    lat::keep(sink);
    auto sm = lat::summarize(v);
    std::printf("  %-28s %7.1f | %7.1f\n", name, sm.p50, sm.p99);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[built %s]\nApple M1 Max, Apple clang 21, -O2. uint64 keys, successful lookups.\n", lat::build_flags());
    for (size_t N : {size_t(4096), size_t(1) << 16, size_t(1) << 20, size_t(1) << 24}) {
        lat::Rng rng(N);
        std::vector<uint64_t> sorted(N);
        for (size_t i = 0; i < N; ++i) sorted[i] = 3 * i + 1;
        std::vector<uint64_t> eyt(N + 1 + 16 * (N + 1), 0);   // padding: prefetch never leaves the buffer
        std::vector<uint64_t> b(N + 1);
        fill(sorted, b, 0, 1);
        std::copy(b.begin(), b.end(), eyt.begin());
        boost::container::flat_set<uint64_t> fs(boost::container::ordered_unique_range, sorted.begin(), sorted.end());
        std::vector<uint64_t> probes(1 << 20);
        for (auto& p : probes) p = sorted[rng.next() % N];
        // sanity: all three agree on every probe in a sample
        for (int i = 0; i < 1000; ++i) {
            uint64_t x = probes[i];
            if (eyt[eytz_lower_bound(eyt.data(), N, x)] != x || *fs.find(x) != x) { std::printf("MISMATCH\n"); return 1; }
        }
        std::printf("N = %zu (%zu KiB of keys)  ns/lookup p50 | p99\n", N, N * 8 / 1024);
        run("std::lower_bound (sorted)", probes, [&](uint64_t x) { return *std::lower_bound(sorted.begin(), sorted.end(), x); });
        run("boost::flat_set::find", probes, [&](uint64_t x) { return *fs.find(x); });
        run("Eytzinger + prefetch", probes, [&](uint64_t x) { return eyt[eytz_lower_bound(eyt.data(), N, x)]; });
    }
}
