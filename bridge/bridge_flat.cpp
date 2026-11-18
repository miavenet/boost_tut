// bridge_flat.cpp - proves the "one-line allocator swap" from boost_flat_containers/labs/capstone.cpp.
// capstone.cpp has a main() and includes ../bench/lat.hpp, so it is not includable; the table
// definitions are COPIED from boost_flat_containers/labs/capstone.cpp lines 27-40 (Sym8, sym,
// InstrumentConfig, RiskLimit, ConfigTableT, RiskTableT). The swap under test is the template argument:
//   ConfigTableT<>  -> ConfigTableT<hft::allocator<InstrumentConfig>>
//   RiskTableT<>    -> RiskTableT<hft::allocator<std::pair<Sym8, RiskLimit>>>
// Arena setup per advanced_memory/labs/l1_one_line.cpp: hft::thread_arena before the containers.
#include "hft_mem/hft_mem.hpp"
#include <boost/container/flat_map.hpp>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
using std::uint32_t; using std::uint64_t; using std::int64_t;

static std::atomic<long> g_news{0};
void* operator new(std::size_t n) { g_news.fetch_add(1, std::memory_order_relaxed); if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// ---- copied from boost_flat_containers/labs/capstone.cpp:27-40 ---------------------------------
using Sym8 = uint64_t;                       // 8 ASCII chars packed into one word
inline Sym8 sym(const char* s) { Sym8 x = 0; std::memcpy(&x, s, std::min<size_t>(8, std::strlen(s))); return __builtin_bswap64(x); }
struct InstrumentConfig { uint32_t tick_size, lot_size; uint64_t flags; double px_band; };
struct RiskLimit { int64_t max_qty, max_notional; };
template <class Alloc = std::allocator<InstrumentConfig>>
using ConfigTableT = std::vector<InstrumentConfig, Alloc>;            // index = instrument id
template <class AllocOrSeq = boost::container::new_allocator<std::pair<Sym8, RiskLimit>>>
using RiskTableT = boost::container::flat_map<Sym8, RiskLimit, std::less<Sym8>, AllocOrSeq>;
// ---- end copy ------------------------------------------------------------------------------------

struct Out { int64_t checksum; size_t size, cap_ok; long setup_news, work_news; };

template <class Cfg, class Risk>
Out run(long n0) {
    constexpr int N = 20000, kOrders = 100000, kEvery = 1000;
    Cfg cfg(N);
    for (int i = 0; i < N; ++i) cfg[i] = {1u + i % 5, 100, uint64_t(i), 0.05};
    typename Risk::sequence_type seq;
    for (int i = 0; i < N; ++i) { char s[9]; std::snprintf(s, sizeof s, "S%06d", i * 2); seq.emplace_back(sym(s), RiskLimit{1000 + i, 1000000}); }
    Risk risk;
    risk.adopt_sequence(boost::container::ordered_unique_range, std::move(seq));
    risk.shrink_to_fit();
    long setup = g_news.load() - n0, before = g_news.load();
    int64_t cs = 0; long listed = 0;
    for (int i = 0; i < kOrders; ++i) {
        int id = int((uint64_t(i) * 2654435761u) % N);
        if (i % kEvery == kEvery - 1) {   // new listing inside the loop: odd symbol, forces growth
            char s[9]; std::snprintf(s, sizeof s, "S%06d", 2 * id + 1);
            listed += risk.emplace(sym(s), RiskLimit{500, 500000}).second;
        }
        char s[9]; std::snprintf(s, sizeof s, "S%06d", id * 2);
        auto it = risk.find(sym(s));
        cs += (it != risk.end() ? it->second.max_qty : -1000000) + cfg[id].lot_size;
    }
    return {cs, risk.size(), size_t(listed), setup, g_news.load() - before};
}

int main() {
    long n0 = g_news.load();
    Out a = run<ConfigTableT<>, RiskTableT<>>(n0);
    Out b;
    {
        hft::thread_arena arena;      // once per thread, BEFORE the containers
        n0 = g_news.load();
        // THE ONE-LINE SWAP: allocator template arguments only
        b = run<ConfigTableT<hft::allocator<InstrumentConfig>>,
                RiskTableT<hft::allocator<std::pair<Sym8, RiskLimit>>>>(n0);
    }
    std::printf("default allocators: checksum %lld risk size %zu listed %zu | operator new: setup %ld, hot loop %ld\n",
                (long long)a.checksum, a.size, a.cap_ok, a.setup_news, a.work_news);
    std::printf("hft::allocator    : checksum %lld risk size %zu listed %zu | operator new: setup %ld, hot loop %ld\n",
                (long long)b.checksum, b.size, b.cap_ok, b.setup_news, b.work_news);
    if (a.checksum != b.checksum || a.size != b.size || a.cap_ok != b.cap_ok) { std::puts("FAIL: results differ"); return 1; }
    if (a.work_news == 0 || b.work_news >= a.work_news) { std::puts("FAIL: swap did not change allocation behaviour"); return 1; }
    std::puts("OK: identical results");
    return 0;
}
