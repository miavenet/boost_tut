// insert_tail.cpp - per-operation latency of WRITES, one op per sample.
// The point: the mean can look fine while max is ugly (reallocation, rehash),
// and position (front vs back) decides how many bytes every insert moves.
//
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include insert_tail.cpp -o insert_tail
// Run:   ./insert_tail
#include "lat.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <map>

using FM = boost::container::flat_map<uint64_t, uint64_t>;

template <class F> lat::Summary time_each(int n, F&& f) {
    std::vector<double> v; v.reserve(n);
    for (int i = 0; i < n; ++i) { uint64_t t0 = lat::now_ns(); f(i); v.push_back(double(lat::now_ns() - t0)); }
    return lat::summarize(v);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[built %s]\n", lat::build_flags());
    std::printf("Apple M1 Max, Apple clang 21, -O2, Boost 1.90. ns per single op (timer tick ~41.7 ns).\n\n");
    lat::print_header();
    constexpr int N = 1 << 20;

    // 1. Growth: append ascending keys (cheapest position) with and without reserve.
    {
        FM m; int reallocs = 0; size_t cap = m.capacity();
        auto s = time_each(N, [&](int i) { m.emplace_hint(m.end(), i, i); });
        lat::print_row("flat_map append, no reserve", s);
        FM c; for (int i = 0; i < N; ++i) { c.emplace_hint(c.end(), i, i); if (c.capacity() != cap) { ++reallocs; cap = c.capacity(); } }
        std::printf("   ^ %d reallocations while growing to %d elements\n", reallocs, N);
    }
    {
        FM m; m.reserve(N);
        auto s = time_each(N, [&](int i) { m.emplace_hint(m.end(), i, i); });
        lat::print_row("flat_map append, reserve(N) first", s);
    }

    // 2. Steady state, 10k elements: insert one key then erase it again.
    //    Front = smaller than every key (moves all 10k pairs), back = larger than all.
    {
        constexpr int S = 10000, R = 200000;
        FM m; m.reserve(S + 1);
        for (int i = 0; i < S; ++i) m.emplace_hint(m.end(), 2 * (i + 1), i);
        lat::print_row("flat_map 10k: insert at FRONT", time_each(R, [&](int) { m.emplace(0, 0); m.erase(0); }));
        lat::print_row("flat_map 10k: insert at BACK", time_each(R, [&](int) { m.emplace(1u << 30, 0); m.erase(1u << 30); }));
        lat::Rng rng(7);
        lat::print_row("flat_map 10k: insert at RANDOM pos", time_each(R, [&](int) { uint64_t k = 2 * (rng.next() % S) + 1; m.emplace(k, 0); m.erase(k); }));
        // erase timed alone
        std::vector<double> v; v.reserve(R);
        for (int i = 0; i < R; ++i) { m.emplace(0, 0); uint64_t t0 = lat::now_ns(); m.erase(0); v.push_back(double(lat::now_ns() - t0)); }
        lat::print_row("flat_map 10k: erase at FRONT", lat::summarize(v));
        v.clear();
        for (int i = 0; i < R; ++i) { m.emplace(1u << 30, 0); uint64_t t0 = lat::now_ns(); m.erase(1u << 30); v.push_back(double(lat::now_ns() - t0)); }
        lat::print_row("flat_map 10k: erase at BACK", lat::summarize(v));

        std::map<uint64_t, uint64_t> sm;
        for (int i = 0; i < S; ++i) sm.emplace(2 * (i + 1), i);
        lat::print_row("std::map 10k: insert+erase RANDOM", time_each(R, [&](int) { uint64_t k = 2 * (rng.next() % S) + 1; sm.emplace(k, 0); sm.erase(k); }));
    }

    // 3. Hash table growth: boost::unordered_flat_map rehash spikes vs reserve.
    {
        boost::unordered_flat_map<uint64_t, uint64_t> h;
        lat::Rng rng(9);
        size_t buckets = h.bucket_count(); int rehashes = 0;
        std::vector<double> v; v.reserve(N);
        for (int i = 0; i < N; ++i) {
            uint64_t k = rng.next(); uint64_t t0 = lat::now_ns(); h.emplace(k, i); v.push_back(double(lat::now_ns() - t0));
            if (h.bucket_count() != buckets) { ++rehashes; buckets = h.bucket_count(); }
        }
        lat::print_row("unordered_flat_map insert, no reserve", lat::summarize(v));
        std::printf("   ^ %d rehashes while growing to %d elements\n", rehashes, N);
    }
    {
        boost::unordered_flat_map<uint64_t, uint64_t> h; h.reserve(N);
        lat::Rng rng(9);
        lat::print_row("unordered_flat_map insert, reserve(N)", time_each(N, [&](int i) { h.emplace(rng.next(), i); }));
    }
}
