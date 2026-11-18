// lat.hpp — tiny latency recorder: store every sample, report p50 / p99 / p99.9 / max (+ mean).
// Timer: std::chrono::steady_clock. On Apple Silicon its tick is 1/24 MHz = 41.67 ns, so a
// single-op sample is quantised to multiples of ~41 ns and each now() call costs ~20 ns
// (measured: bench/clock_probe). For sub-100 ns operations we time a BATCH of ops and divide
// (stated per bench). Re-run on your own box: an isolated x86-64 Linux core with rdtsc will differ.
#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Lat {
    std::vector<double> ns;
    void reserve(std::size_t n) { ns.reserve(n); }
    void add(double v) { ns.push_back(v); }
    double pct(double p) {
        std::size_t i = std::min(ns.size() - 1, std::size_t(p / 100.0 * double(ns.size())));
        std::nth_element(ns.begin(), ns.begin() + std::ptrdiff_t(i), ns.end());
        return ns[i];
    }
    void report(const char* name) {
        double sum = 0, mx = 0;
        for (double v : ns) { sum += v; mx = std::max(mx, v); }
        std::size_t spikes = 0;
        for (double v : ns) spikes += v > 100000.0;
        std::printf("%-40s n=%-8zu mean=%8.1f  p50=%8.1f  p99=%8.1f  p99.9=%9.1f  max=%10.1f ns  >100us:%zu\n",
                    name, ns.size(), sum / double(ns.size()), pct(50), pct(99), pct(99.9), mx, spikes);
    }
};

using Clock = std::chrono::steady_clock;
inline double ns_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
}
template <class T> inline void keep(T const& v) { asm volatile("" : : "r,m"(v) : "memory"); }

// Deterministic xorshift so every run sees the same key sequence.
struct Rng {
    std::uint64_t s = 0x9E3779B97F4A7C15ull;
    std::uint64_t operator()() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
};
