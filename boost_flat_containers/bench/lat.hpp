// lat.hpp - tiny latency helpers shared by the benches and labs.
// Timer: std::chrono::steady_clock. On Apple Silicon its tick is 1/24 MHz
// (~41.7 ns, see bench/timer_res.cpp), so single operations faster than that
// are timed in BATCHES and divided; slow single events (reallocation, rehash)
// are timed one by one.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace lat {
using clk = std::chrono::steady_clock;

inline uint64_t now_ns() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
               clk::now().time_since_epoch()).count();
}

struct Summary { double p50, p99, p999, max, mean; };

// Percentiles by nearest rank on a sorted copy. Not averages: tails matter.
inline Summary summarize(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    auto at = [&](double q) {
        size_t i = (size_t)(q * (double)(v.size() - 1) + 0.5);
        return v[std::min(i, v.size() - 1)];
    };
    double sum = 0; for (double x : v) sum += x;
    return {at(0.50), at(0.99), at(0.999), v.back(), sum / (double)v.size()};
}

inline void print_header() {
    std::printf("%-34s %9s %9s %9s %11s %9s\n", "case", "p50", "p99", "p99.9", "max", "mean");
}
inline void print_row(const char* name, const Summary& s) {
    std::printf("%-34s %9.1f %9.1f %9.1f %11.1f %9.1f\n", name, s.p50, s.p99, s.p999, s.max, s.mean);
}

// Which build is this? Boost containers keep BOOST_ASSERT checks unless NDEBUG.
inline const char* build_flags() {
#ifdef NDEBUG
    return "NDEBUG defined: assertions OFF";
#else
    return "NDEBUG not defined: assert()/BOOST_ASSERT checks ON";
#endif
}

// xorshift64* - deterministic, seedable, no <random> distribution quirks.
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 1) {}
    uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 2685821657736338717ULL; }
};

// Defeat dead-code elimination without a memory fence.
template <class T> inline void keep(T const& v) { asm volatile("" : : "r,m"(v) : "memory"); }
}  // namespace lat
