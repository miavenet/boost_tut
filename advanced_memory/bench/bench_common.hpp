// bench_common.hpp — per-operation latency recorder that prints tails, not just averages.
// Timer: std::chrono::steady_clock. On the authoring Mac its smallest step is ~41 ns, so small
// values are quantised (0, 41, 83, 125 ns ...). Tails in the microseconds are not affected.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <sys/resource.h>

namespace bench {
using clock = std::chrono::steady_clock;

inline std::int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
}

inline long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;
}

// Construct it right before the measured loop: report() also prints the page faults taken
// between construction and report (i.e. inside the loop).
struct recorder {
    std::vector<std::int64_t> v;
    std::size_t n = 0;
    // Pre-sized AND pre-touched (resize writes every element): recording never allocates and
    // never takes a first-touch page fault inside the measured loop.
    long faults0;
    explicit recorder(std::size_t cap) : v(cap), faults0(minor_faults()) {}
    void add(std::int64_t ns) { v[n++] = ns; }
    void report(const char* name) {
        long faults = minor_faults() - faults0;
        v.resize(n);
        std::sort(v.begin(), v.end());
        auto pct = [&](double p) { return v[static_cast<std::size_t>(p * (v.size() - 1))]; };
        double sum = 0;
        for (auto x : v) sum += static_cast<double>(x);
        std::printf("%-44s n=%-7zu mean=%6.0f p50=%5lld p99=%5lld p99.9=%6lld max=%8lld ns  faults=%ld\n",
                    name, v.size(), sum / v.size(), (long long)pct(0.50), (long long)pct(0.99),
                    (long long)pct(0.999), (long long)v.back(), faults);
        std::fflush(stdout);
    }
};

// Cheap deterministic RNG so every run does the same work.
struct xorshift {
    std::uint64_t s = 88172645463325252ull;
    std::uint64_t operator()() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
};

template <class T>
inline void keep(T const& x) { asm volatile("" : : "r,m"(x) : "memory"); }
}  // namespace bench
