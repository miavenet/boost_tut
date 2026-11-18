// capstone.cpp - an order-entry hot path with two lookup tables:
//   1. instrument id -> InstrumentConfig   (ids are dense 0..N-1 -> direct index)
//   2. symbol        -> RiskLimit          (sparse 8-char symbols -> flat_map)
// Intraday "new listing" events insert into the risk table INSIDE the hot loop.
//
// Build: clang++ -std=c++23 -O2 -DNDEBUG -I/opt/homebrew/include capstone.cpp -o capstone
// Run:   ./capstone
// Your job (lesson 4): make "hot-path heap allocations" print 0 and shrink the
// max, by writing the body of prepare() below - or put it in a file and build with
//   -DPREPARE_FILE='"my_prepare.inc"'
// Stage 2 (stretch): add -DSTAGE_LISTINGS to send new listings to a small side
// table instead of shifting the big one, and compare the max again.
#include "../bench/lat.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

static long g_allocs = 0;
void* operator new(std::size_t n) { ++g_allocs; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using Sym8 = uint64_t;                       // 8 ASCII chars packed into one word
// Byte-swapped so integer order == string order: the start-of-day table is built with
// ordered_unique_range, which requires the keys already sorted.
inline Sym8 sym(const char* s) { Sym8 x = 0; std::memcpy(&x, s, std::min<size_t>(8, std::strlen(s))); return __builtin_bswap64(x); }

struct InstrumentConfig { uint32_t tick_size, lot_size; uint64_t flags; double px_band; };
struct RiskLimit { int64_t max_qty, max_notional; };

// The memory policy is ONE template argument (Boost's default shown). Swapping in
// a pooled / arena allocator - e.g. from ../advanced_memory - is a one-line change
// to the aliases below; nothing else in this file needs to know.
template <class Alloc = std::allocator<InstrumentConfig>>
using ConfigTableT = std::vector<InstrumentConfig, Alloc>;            // index = instrument id
template <class AllocOrSeq = boost::container::new_allocator<std::pair<Sym8, RiskLimit>>>
using RiskTableT = boost::container::flat_map<Sym8, RiskLimit, std::less<Sym8>, AllocOrSeq>;

#ifndef ALLOC_SWAP_DEMO
using ConfigTable = ConfigTableT<>;
using RiskTable   = RiskTableT<>;
#else   // the one-line swap, shown with the standard's polymorphic allocator
#include <memory_resource>
using ConfigTable = ConfigTableT<std::pmr::polymorphic_allocator<InstrumentConfig>>;
using RiskTable   = RiskTableT<std::pmr::polymorphic_allocator<std::pair<Sym8, RiskLimit>>>;
#endif

constexpr int kInstruments = 100000, kOrders = 400000, kListingEvery = 4000;
constexpr int kMaxNewListings = kOrders / kListingEvery;               // 100 per session

// ---- called once at startup, OFF the hot path ----------------------------------
void prepare(ConfigTable& cfg, RiskTable& risk) {
#ifdef PREPARE_FILE
#include PREPARE_FILE
#else
    (void)cfg; (void)risk;   // TODO(lesson 4): make the hot loop allocation-free
#endif
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // Start-of-day load: built in bulk, so capacity == size (tight).
    ConfigTable cfg(kInstruments);
    for (int i = 0; i < kInstruments; ++i) cfg[i] = {1u + i % 5, 100, uint64_t(i), 0.05};
    RiskTable::sequence_type seq;
    for (int i = 0; i < kInstruments; ++i) {
        char s[9]; std::snprintf(s, sizeof s, "S%06d", i * 2);       // even numbers: room for new ones
        seq.emplace_back(sym(s), RiskLimit{1000 + i, 1000000});
    }
    RiskTable risk;
    risk.adopt_sequence(boost::container::ordered_unique_range, std::move(seq));
    risk.shrink_to_fit();                    // as tight as a freshly loaded table gets
    std::vector<int> ids(kOrders);
    std::vector<Sym8> syms(kOrders);
    lat::Rng rng(2026);
    for (int i = 0; i < kOrders; ++i) {
        ids[i] = int(rng.next() % kInstruments);
        char s[9]; std::snprintf(s, sizeof s, "S%06d", ids[i] * 2);
        syms[i] = sym(s);
    }
    std::vector<double> ns;
    ns.reserve(kOrders);

    prepare(cfg, risk);

#ifdef STAGE_LISTINGS
    // Side table: at most kMaxNewListings entries, storage inside the object (no heap).
    RiskTableT<boost::container::static_vector<std::pair<Sym8, RiskLimit>, kMaxNewListings>> side;
#endif
    // ---- the hot loop -------------------------------------------------------------
    long allocs_before = g_allocs;
    int64_t checksum = 0, newly_listed = 0, misses = 0;   // misses: start-of-day symbols that must ALWAYS be found
    for (int i = 0; i < kOrders; ++i) {
        uint64_t t0 = lat::now_ns();
        if (i % kListingEvery == kListingEvery - 1) {                  // intraday new listing
            char s[9]; std::snprintf(s, sizeof s, "S%06d", 2 * int(rng.next() % kInstruments) + 1);
#ifdef STAGE_LISTINGS
            if (risk.find(sym(s)) == risk.end()) newly_listed += side.emplace(sym(s), RiskLimit{500, 500000}).second;
#else
            newly_listed += risk.emplace(sym(s), RiskLimit{500, 500000}).second;
#endif
        }
        const InstrumentConfig& c = cfg[ids[i]];                        // 1 load, no search
        auto it = risk.find(syms[i]);                                   // binary search
        int64_t q = it != risk.end() ? it->second.max_qty : -1;
#ifdef STAGE_LISTINGS
        if (q < 0) { auto st = side.find(syms[i]); if (st != side.end()) q = st->second.max_qty; }
#endif
        misses += q < 0;
        checksum += q + c.lot_size;
        ns.push_back(double(lat::now_ns() - t0));
    }
    long hot_allocs = g_allocs - allocs_before;

    // ---- report -------------------------------------------------------------------
    auto s = lat::summarize(ns);
    std::printf("[%s] orders=%d new listings=%lld\n", lat::build_flags(), kOrders, (long long)newly_listed);
    std::printf("per-order ns: p50 %.0f  p99 %.0f  p99.9 %.0f  max %.0f  mean %.1f\n", s.p50, s.p99, s.p999, s.max, s.mean);
    std::printf("histogram (ns, log2 buckets):\n");
    long bucket[32] = {};
    for (double x : ns) { int b = 0; while (b < 31 && x >= double(64L << b)) ++b; ++bucket[b]; }
    for (int b = 0; b < 32; ++b) if (bucket[b]) {
        int bar = bucket[b] > 0 ? 1 + int(40.0 * std::log2(double(bucket[b])) / std::log2(double(kOrders))) : 0;
        std::printf("  %s%7ld ns : %7ld %.*s\n", b ? "< " : "< ", 64L << b, bucket[b], bar, "########################################");
    }
    // every order's symbol existed at start of day, so any miss means the table lost data (e.g. clear()).
    bool ok = checksum > 0 && newly_listed > 0 && misses == 0 && risk.size() >= size_t(kInstruments);
    std::printf("lookups %s, misses=%lld, risk table size=%zu capacity=%zu\n", ok ? "ok" : "WRONG", (long long)misses, risk.size(), risk.capacity());
#ifdef STAGE_LISTINGS
    std::printf("side table size=%zu (merge it into the main table off the hot path)\n", side.size());
#endif
    std::printf("hot-path heap allocations: %ld\n", hot_allocs);
    return ok ? 0 : 1;
}
