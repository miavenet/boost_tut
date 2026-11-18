// lookup_crossover.cpp - successful-lookup latency vs N for six table choices.
//
// Each sample times a BATCH of 64 random successful finds and divides by 64
// (steady_clock ticks every ~41.7 ns here, too coarse for one lookup).
// So p99 below is "p99 of 64-lookup batch averages", not per-lookup p99.
//
// Build: clang++ -std=c++23 -O2 -I/opt/homebrew/include lookup_crossover.cpp -o lookup_crossover
// Run:   ./lookup_crossover            (all configs, a few minutes)
//        ./lookup_crossover quick      (N <= 4096 only)
#include "lat.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <array>
#include <cstring>
#include <flat_map>
#include <map>
#include <string_view>
#include <unordered_map>

struct K32 {                       // a 32-byte key, e.g. a padded symbol string
    std::array<char, 32> b{};
    friend bool operator<(const K32& x, const K32& y) { return std::memcmp(x.b.data(), y.b.data(), 32) < 0; }
    friend bool operator==(const K32& x, const K32& y) { return std::memcmp(x.b.data(), y.b.data(), 32) == 0; }
};
struct V64 { uint64_t w[8]; };    // a 64-byte mapped value (a config record)

struct Hash {
    size_t operator()(uint64_t k) const { return std::hash<uint64_t>{}(k * 0x9E3779B97F4A7C15ULL); }
    size_t operator()(const K32& k) const { return std::hash<std::string_view>{}({k.b.data(), 32}); }
};

template <class K> K make_key(uint64_t x);
template <> uint64_t make_key<uint64_t>(uint64_t x) { return x; }
template <> K32 make_key<K32>(uint64_t x) {
    K32 k; std::snprintf(k.b.data(), 32, "SYM%020llu", (unsigned long long)x); return k;
}
template <class V> V make_val(uint64_t x) { V v{}; std::memcpy(&v, &x, sizeof x); return v; }
template <class V> uint64_t first_word(const V& v) { uint64_t x; std::memcpy(&x, &v, sizeof x); return x; }

constexpr int BATCH = 64, SAMPLES = 3000;

template <class K, class Find>
double run(const std::vector<K>& probes, Find&& find, double* p99) {
    std::vector<double> per;
    per.reserve(SAMPLES);
    size_t pi = 0;
    uint64_t sink = 0;
    for (int w = 0; w < 200; ++w) for (int i = 0; i < BATCH; ++i) sink += find(probes[(pi++) % probes.size()]);  // warm
    for (int s = 0; s < SAMPLES; ++s) {
        uint64_t t0 = lat::now_ns();
        for (int i = 0; i < BATCH; ++i) sink += find(probes[(pi++) % probes.size()]);
        uint64_t t1 = lat::now_ns();
        per.push_back(double(t1 - t0) / BATCH);
    }
    lat::keep(sink);
    auto sm = lat::summarize(per);
    *p99 = sm.p99;
    return sm.p50;
}

template <class K, class V>
void config(const char* title, const std::vector<size_t>& Ns) {
    std::printf("\n== %s  (sizeof key=%zu, sizeof value=%zu) ns/lookup p50 | p99-of-batches ==\n",
                title, sizeof(K), sizeof(V));
    std::printf("%8s %15s %15s %15s %15s %15s %15s\n", "N", "linear_scan", "boost_flat", "std_flat",
                "std_map", "std_unordered", "boost_unord_flat");
    for (size_t N : Ns) {
        lat::Rng rng(42 + N);
        std::vector<K> keys;
        std::vector<std::pair<K, V>> pairs;
        {
            boost::unordered_flat_map<K, int, Hash> seen;
            while (keys.size() < N) {
                uint64_t x = rng.next() % (N * 16 + 64);
                if (seen.emplace(make_key<K>(x), 0).second) { keys.push_back(make_key<K>(x)); pairs.push_back({keys.back(), make_val<V>(x)}); }
            }
        }
        std::vector<K> probes(1 << 20);  // uniform over ALL keys; large so probed lines are not all cache-hot
        for (auto& p : probes) p = keys[rng.next() % N];

        boost::container::flat_map<K, V> bf(pairs.begin(), pairs.end());
        std::flat_map<K, V> sf;    // built in O(N log N) via sort + sorted_unique
        {
            std::vector<K> ks; std::vector<V> vs;
            for (auto& p : bf) { ks.push_back(p.first); vs.push_back(p.second); }
            sf = std::flat_map<K, V>(std::sorted_unique, std::move(ks), std::move(vs));
        }
        std::map<K, V> sm(pairs.begin(), pairs.end());
        std::unordered_map<K, V, Hash> su(pairs.begin(), pairs.end());
        boost::unordered_flat_map<K, V, Hash> bu(pairs.begin(), pairs.end());
        auto& lin = pairs;  // unsorted vector, linear scan

        double a50 = 0, a99 = 0, b99, c99, d99, e99, f99;
        if (N <= 256)
            a50 = run(probes, [&](const K& k) { for (auto& p : lin) if (p.first == k) return first_word(p.second); return uint64_t(0); }, &a99);
        double b50 = run(probes, [&](const K& k) { return first_word(bf.find(k)->second); }, &b99);
        double c50 = run(probes, [&](const K& k) { return first_word(sf.find(k)->second); }, &c99);
        double d50 = run(probes, [&](const K& k) { return first_word(sm.find(k)->second); }, &d99);
        double e50 = run(probes, [&](const K& k) { return first_word(su.find(k)->second); }, &e99);
        double f50 = run(probes, [&](const K& k) { return first_word(bu.find(k)->second); }, &f99);
        auto cell = [](double p50, double p99) { static char buf[6][32]; static int i = 0; i = (i + 1) % 6;
            std::snprintf(buf[i], 32, "%6.1f|%6.1f", p50, p99); return buf[i]; };
        std::printf("%8zu %15s %15s %15s %15s %15s %15s\n", N, N <= 256 ? cell(a50, a99) : "      -|     -",
                    cell(b50, b99), cell(c50, c99), cell(d50, d99), cell(e50, e99), cell(f50, f99));
    }
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    bool quick = argc > 1 && std::string_view(argv[1]) == "quick";
    std::vector<size_t> Ns = {8, 16, 32, 64, 128, 256, 1024, 4096};
    if (!quick) { Ns.push_back(65536); Ns.push_back(1048576); }
    std::printf("[built %s]\n", lat::build_flags());
    std::printf("Apple M1 Max, Apple clang 21, -std=c++23 -O2, Boost 1.90. Successful finds, random keys.\n");
    config<uint64_t, uint64_t>("8B key, 8B value", Ns);
    config<uint64_t, V64>("8B key, 64B value", Ns);
    config<K32, uint64_t>("32B key, 8B value", Ns);
}
