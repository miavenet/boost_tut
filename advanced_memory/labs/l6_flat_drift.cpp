// l6_flat_drift.cpp — the id index next to the intrusive book: a RESERVED boost::unordered_flat_map
// still allocates under steady churn when it is reserved for exactly the live count.
// Run: ./labs/build/l6_flat_drift
// Workload: 50,000 live ids; 200,000 x (erase a random live id, insert a new random id): the
// size never grows. We count operator new calls inside the churn.
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

static long g_news = 0;
static std::size_t g_last = 0;
void* operator new(std::size_t n) { ++g_news; g_last = n; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct mix { std::size_t operator()(long k) const noexcept {
    std::uint64_t x = static_cast<std::uint64_t>(k); x ^= x >> 33; x *= 0xff51afd7ed558ccdull; x ^= x >> 33; return x; } };

void churn(std::size_t reserve_for) {
    constexpr int LIVE = 50'000, OPS = 200'000;
    boost::unordered_flat_map<long, void*, mix> m;
    m.reserve(reserve_for);
    std::uint64_t s = 88172645463325252ull;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; };
    std::vector<long> live(LIVE);
    for (auto& k : live) { k = static_cast<long>(rnd() >> 1); m.emplace(k, nullptr); }
    std::size_t bc = m.bucket_count();
    long n0 = g_news, first = -1;
    std::size_t bytes = 0;
    for (int i = 0; i < OPS; ++i) {
        int j = static_cast<int>(rnd() % LIVE);
        long before = g_news;
        m.erase(live[j]);
        live[j] = static_cast<long>(rnd() >> 1);
        m.emplace(live[j], nullptr);
        if (g_news != before) { bytes = g_last; if (first < 0) first = i; }
    }
    std::printf("reserve(%zu): bucket_count %zu -> %zu, operator new in churn: %ld",
                reserve_for, bc, m.bucket_count(), g_news - n0);
    if (first >= 0) std::printf(" (each %zu B, first at op %ld)", bytes, first);
    std::puts("");
}

int main() {
    churn(50'000);    // reserved for exactly the live count
    churn(100'000);   // 2x headroom
}
