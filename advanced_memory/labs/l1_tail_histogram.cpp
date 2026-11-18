// l1_tail_histogram.cpp — the hook. Same hot loop twice:
//   MALLOC : each message allocates a fresh 256-byte object with new (live set grows to ~51 MB)
//   ARENA  : the same objects come from an hft::thread_arena that was pre-faulted up front
// Prints a latency histogram per allocation plus the page faults taken inside the loop.
// Run:  ./build/l1_tail_histogram          (then: nowarm, then: nowarm nocap)
//   nowarm : skip arena.warm(), so the pool refills (carves new chunks) inside the timed loop
//   nocap  : pool_options::max_blocks_per_chunk = 0 (the library's default, which grows chunks)
//            instead of hft_mem's cap of 256 blocks per chunk
#include "hft_mem/hft_mem.hpp"

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <vector>

constexpr int N = 200'000;
constexpr std::size_t SZ = 256;

long minor_faults() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_minflt;
}

void histogram(const char* name, std::vector<long>& ns, long faults) {
    const long edges[] = {50, 100, 250, 500, 1000, 5000, 20000};
    long count[8] = {};
    for (long x : ns) {
        int b = 0;
        while (b < 7 && x >= edges[b]) ++b;
        ++count[b];
    }
    std::sort(ns.begin(), ns.end());
    double mean = 0;
    for (long x : ns) mean += x;
    mean /= ns.size();
    std::printf("\n%s   page faults in loop: %ld\n", name, faults);
    std::printf("  mean=%.0f ns  p50=%ld  p99=%ld  p99.9=%ld  max=%ld ns\n", mean, ns[ns.size() / 2],
                ns[ns.size() * 99 / 100], ns[ns.size() * 999 / 1000], ns.back());
    const char* label[] = {"   <50ns", "  <100ns", "  <250ns", "  <500ns", "   <1us ", "   <5us ", "  <20us ", " >=20us "};
    for (int b = 0; b < 8; ++b) {
        int bar = count[b] == 0 ? 0 : 1 + static_cast<int>(40.0 * count[b] / N);
        std::printf("  %s %7ld |", label[b], count[b]);
        for (int i = 0; i < bar && i < 41; ++i) std::putchar('#');
        std::putchar('\n');
    }
}

template <class Alloc>
void run(const char* name, Alloc alloc) {
    std::vector<void*> live(N);   // sized AND touched now, so the loop's own bookkeeping
    std::vector<long> ns(N);      // never page-faults (reserve() alone would not touch them)
    long f0 = minor_faults();
    for (int i = 0; i < N; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        void* p = alloc();
        static_cast<char*>(p)[0] = 1;  // the handler writes into its new object
        auto t1 = std::chrono::steady_clock::now();
        live[i] = p;
        ns[i] = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    }
    histogram(name, ns, minor_faults() - f0);
    if constexpr (requires { alloc.release(live); }) alloc.release(live);
}

struct malloc_route {
    void* operator()() const { return ::operator new(SZ); }
    void release(std::vector<void*>& v) const { for (void* p : v) ::operator delete(p); }
};

int main(int argc, char** argv) {
    bool warm = true, cap = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "nowarm") warm = false;
        if (std::string_view(argv[i]) == "nocap") cap = false;
    }
    std::printf("Allocating %d objects of %zu B, each written once (the 'message handler').\n", N, SZ);
    run("MALLOC (new in the hot path)", malloc_route{});

    hft::arena_config cfg;
    cfg.region_bytes = std::size_t{128} << 20;
    if (!cap) cfg.max_blocks_per_chunk = 0;  // 0 = "use the implementation's limit"
    hft::thread_arena arena(cfg);  // the region is pre-faulted HERE, before the loop
    if (warm) arena.warm(SZ, N, 1); // and the pool's free list is carved HERE (try: ./l1_tail_histogram nowarm)
    hft::allocator<std::byte> a;
    std::printf("\narena: warm()=%s, max_blocks_per_chunk=%s\n", warm ? "yes" : "no", cap ? "256 (hft_mem cap)" : "0 (library default)");
    run("ARENA  (hft::thread_arena, pre-faulted)", [&] { return static_cast<void*>(a.allocate(SZ)); });
}
