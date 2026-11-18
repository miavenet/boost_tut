// b3_locality.cpp — where the nodes land, and what that does to a walk; plus false sharing.
//
// Part 1: two std::pmr::map<long,long> (A and B) filled INTERLEAVED (A, B, A, B, ...),
//   100,000 keys each. Then walk A in key order, 200 times. One sample = ns per node of one walk.
//   Also prints the median byte distance between consecutive nodes of A in walk order.
//   (1) default malloc   (2) ONE shared pool for A and B   (3) ONE pool PER container
//   Run with ascending keys (allocation order == walk order) and random keys (it is not).
// Part 2: two threads, each bumping its own atomic counter. Counters packed in one cache line
//   vs separated by alignas(std::hardware_destructive_interference_size).
//   One sample = ns for 10,000 increments on one thread.
#include "bench_common.hpp"

#include <algorithm>
#include <atomic>
#include <map>
#include <memory_resource>
#include <new>
#include <string>
#include <thread>
#include <vector>

constexpr int N = 100'000;
constexpr int WALKS = 200;

using pmap = std::pmr::map<long, long>;

long median_gap(const pmap& m) {
    std::vector<long> gaps;
    gaps.reserve(m.size());
    const void* prev = nullptr;
    for (auto& kv : m) {
        if (prev) gaps.push_back(std::labs(reinterpret_cast<const char*>(&kv) - static_cast<const char*>(prev)));
        prev = &kv;
    }
    std::nth_element(gaps.begin(), gaps.begin() + gaps.size() / 2, gaps.end());
    return gaps[gaps.size() / 2];
}

void walk_bench(const char* name, std::pmr::memory_resource* ra, std::pmr::memory_resource* rb, bool random) {
    pmap a(ra), b(rb);
    bench::xorshift rng;
    for (long i = 0; i < N; ++i) {
        long ka = random ? static_cast<long>(rng() >> 1) : i;
        long kb = random ? static_cast<long>(rng() >> 1) : i;
        a.emplace(ka, i);
        b.emplace(kb, i);
    }
    bench::recorder r(WALKS);
    for (int w = 0; w < WALKS; ++w) {
        auto t0 = bench::now_ns();
        long sum = 0;
        for (auto& kv : a) sum += kv.second;
        auto t1 = bench::now_ns();
        bench::keep(sum);
        r.add((t1 - t0) / static_cast<long>(a.size()));
    }
    r.report(name);
    std::printf("    median byte gap between consecutive nodes of A (walk order): %ld\n", median_gap(a));
}

template <class Counters>
void sharing_bench(const char* name) {
    Counters c;
    constexpr int SAMPLES = 2000, BATCH = 10'000;
    bench::recorder r0(SAMPLES), r1(SAMPLES);
    auto body = [](std::atomic<long>& x, bench::recorder& r) {
        for (int s = 0; s < SAMPLES; ++s) {
            auto t0 = bench::now_ns();
            for (int i = 0; i < BATCH; ++i) x.fetch_add(1, std::memory_order_relaxed);
            r.add(bench::now_ns() - t0);
        }
    };
    std::thread t0(body, std::ref(c.a), std::ref(r0));
    std::thread t1(body, std::ref(c.b), std::ref(r1));
    t0.join();
    t1.join();
    std::printf("  &a=%p &b=%p (distance %td B)\n", (void*)&c.a, (void*)&c.b,
                reinterpret_cast<char*>(&c.b) - reinterpret_cast<char*>(&c.a));
    r0.report(name);
}

// libc++ (Apple clang 21) defines the constant; clang 18 + libstdc++ on Linux did NOT (compile
// error, seen 2026-11-13), so guard it the way cppreference's example does.
#ifdef __cpp_lib_hardware_interference_size
constexpr std::size_t kDestructive = std::hardware_destructive_interference_size;
constexpr std::size_t kConstructive = std::hardware_constructive_interference_size;
#else
constexpr std::size_t kDestructive = 64;   // x86-64 line size; Apple M1 lines are 128 B
constexpr std::size_t kConstructive = 64;
#endif

struct packed {
    std::atomic<long> a{0};
    std::atomic<long> b{0};
};
struct apart {
    alignas(kDestructive) std::atomic<long> a{0};
    alignas(kDestructive) std::atomic<long> b{0};
};

int main() {
    std::printf("hardware_destructive_interference_size=%zu constructive=%zu\n", kDestructive, kConstructive);
    for (bool random : {false, true}) {
        std::printf("== walk map A, keys %s, ns per node ==\n", random ? "RANDOM" : "ASCENDING");
        const char* tag = random ? " [random]" : " [ascending]";
        std::string n1 = std::string("(1) default malloc") + tag, n2 = std::string("(2) one shared pool") + tag,
                    n3 = std::string("(3) pool per container") + tag;
        walk_bench(n1.c_str(), std::pmr::new_delete_resource(), std::pmr::new_delete_resource(), random);
        {
            std::pmr::monotonic_buffer_resource mono;
            std::pmr::unsynchronized_pool_resource shared(&mono);
            walk_bench(n2.c_str(), &shared, &shared, random);
        }
        {
            std::pmr::monotonic_buffer_resource mono_a, mono_b;
            std::pmr::unsynchronized_pool_resource pa(&mono_a), pb(&mono_b);
            walk_bench(n3.c_str(), &pa, &pb, random);
        }
    }
    std::printf("== false sharing: ns per 10,000 relaxed increments (thread 0) ==\n");
    sharing_bench<packed>("packed (same line)");
    sharing_bench<apart>("alignas(destructive_interference_size)");
}
