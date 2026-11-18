// direct_index.cpp - dense instrument ids (0..N-1): index an array vs search a table.
// ns per lookup as p50 | p99 of 64-lookup batch averages (timer tick ~41.7 ns).
//
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include direct_index.cpp -o direct_index
#include "lat.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

struct Config { uint32_t tick, lot; uint64_t flags; double band; };   // 24 bytes

template <class F> void run(const char* name, const std::vector<uint32_t>& probes, F&& f) {
    constexpr int B = 64, S = 4000;
    std::vector<double> v; v.reserve(S);
    size_t pi = 0; uint64_t sink = 0;
    for (int s = 0; s < S; ++s) {
        uint64_t t0 = lat::now_ns();
        for (int i = 0; i < B; ++i) sink += f(probes[(pi++) % probes.size()]);
        v.push_back(double(lat::now_ns() - t0) / B);
    }
    lat::keep(sink);
    auto sm = lat::summarize(v);
    std::printf("  %-34s %7.1f | %7.1f\n", name, sm.p50, sm.p99);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("[built %s]\nApple M1 Max, Apple clang 21, -O2. Dense uint32 ids -> 24-byte Config.\n", lat::build_flags());
    for (uint32_t N : {2000u, 100000u, 1000000u}) {
        std::vector<Config> direct(N);
        boost::container::flat_map<uint32_t, Config> fm;
        fm.reserve(N);
        boost::unordered_flat_map<uint32_t, Config> h;
        h.reserve(N);
        for (uint32_t i = 0; i < N; ++i) {
            Config c{1 + i % 5, 100, i, 0.05};
            direct[i] = c; fm.emplace_hint(fm.end(), i, c); h.emplace(i, c);
        }
        lat::Rng rng(N);
        std::vector<uint32_t> probes(1 << 20);
        for (auto& p : probes) p = uint32_t(rng.next() % N);
        std::printf("N = %u  ns/lookup p50 | p99\n", N);
        run("direct index  t[id]", probes, [&](uint32_t id) { return direct[id].flags; });
        run("boost::container::flat_map::find", probes, [&](uint32_t id) { return fm.find(id)->second.flags; });
        run("boost::unordered_flat_map::find", probes, [&](uint32_t id) { return h.find(id)->second.flags; });
    }
}
