// l4_hot_cold.cpp — hot/cold field splitting. The hot loop reads 3 fields; the cold fields
// (client tag, free text) ride along in every cache line unless you move them out.
// Run: ./build/l4_hot_cold     (prints ns per order scanned, p50/p99/max over 50 scans)
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

struct FatOrder {            // everything in one struct
    long id, px, qty;
    char client[40];
    char notes[64];
};
struct HotOrder { long id, px, qty; };       // what the matching loop touches
struct ColdOrder { char client[40]; char notes[64]; };  // looked up by index, rarely

constexpr int N = 1'000'000, SCANS = 50;

template <class F>
void timeit(const char* name, std::size_t bytes_per_order, F scan) {
    std::vector<double> ns;
    long sink = 0;
    for (int s = 0; s < SCANS; ++s) {
        auto t0 = std::chrono::steady_clock::now();
        sink += scan();
        auto t1 = std::chrono::steady_clock::now();
        ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / N);
    }
    std::sort(ns.begin(), ns.end());
    std::printf("%-28s %3zu B/order  p50=%.2f  p99=%.2f  max=%.2f ns/order  (sink %ld)\n", name,
                bytes_per_order, ns[SCANS / 2], ns[SCANS * 99 / 100], ns.back(), sink % 7);
}

int main() {
    std::vector<FatOrder> fat(N);
    std::vector<HotOrder> hot(N);
    std::vector<ColdOrder> cold(N);
    for (int i = 0; i < N; ++i) {
        fat[i].id = hot[i].id = i;
        fat[i].px = hot[i].px = 100 + i % 50;
        fat[i].qty = hot[i].qty = 1 + i % 3;
    }
    timeit("fat struct (hot+cold)", sizeof(FatOrder), [&] {
        long q = 0;
        for (auto& o : fat) if (o.px == 120) q += o.qty;
        return q;
    });
    timeit("hot fields split out", sizeof(HotOrder), [&] {
        long q = 0;
        for (auto& o : hot) if (o.px == 120) q += o.qty;
        return q;
    });
    (void)cold;
}
