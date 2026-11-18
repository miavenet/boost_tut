// bridge_multi_index.cpp - proves the "one-line allocator swap" from boost_multi_index/labs/capstone.cpp.
// MIStore<Alloc> lives in capstone.cpp, which has a main(), so it is not includable;
// the store below is COPIED VERBATIM from boost_multi_index/labs/capstone.cpp lines 33-90
// (Order, kTicks/kClients, tags, MIStore). Only the includes are gathered at the top.
// The swap under test is the template argument:  MIStore<>  ->  MIStore<hft::allocator<Order>>.
// Thread-arena setup follows advanced_memory/labs/l1_one_line.cpp: `hft::thread_arena arena;` once per thread,
// declared BEFORE any hft container so it is destroyed AFTER them.
#include "hft_mem/hft_mem.hpp"
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <tuple>
#include <vector>

static std::atomic<long> g_news{0};
void* operator new(std::size_t n) { g_news.fetch_add(1, std::memory_order_relaxed); if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

// ---- copied from boost_multi_index/labs/capstone.cpp:33-90 -------------------------------------
struct Order {
    std::uint64_t id;
    std::uint32_t client;
    char side;             // 'B' or 'S'
    std::int64_t price;    // ticks, 0..kTicks-1
    std::uint64_t ts;      // arrival sequence = time priority
    std::uint32_t qty;
    std::int64_t rank() const { return side == 'B' ? -price : price; }  // smaller = better
};
constexpr int kTicks = 1024;       // price band the ladder covers
constexpr int kClients = 64;

namespace bmi = boost::multi_index;
struct ById {};
struct ByBook {};
struct ByClient {};

template <class Alloc = std::allocator<Order>>
class MIStore {
public:
    using Container = bmi::multi_index_container<
        Order,
        bmi::indexed_by<
            bmi::hashed_unique<bmi::tag<ById>, bmi::key<&Order::id>>,
            bmi::ordered_non_unique<bmi::tag<ByBook>, bmi::key<&Order::side, &Order::rank, &Order::ts>>,
            bmi::hashed_non_unique<bmi::tag<ByClient>, bmi::key<&Order::client>>>,
        Alloc>;

    explicit MIStore(std::size_t capacity, bool use_replace, const Alloc& a = Alloc())
        : c_(typename Container::ctor_args_list(), a), use_replace_(use_replace) {
        c_.template get<ById>().reserve(capacity);      // no rehash on the hot path
        c_.template get<ByClient>().reserve(capacity);
    }
    bool add(const Order& o) { return c_.insert(o).second; }
    bool cancel(std::uint64_t id) { return c_.template get<ById>().erase(id) == 1; }
    bool modify_price(std::uint64_t id, std::int64_t px, std::uint64_t ts) {
        auto& ix = c_.template get<ById>();
        auto it = ix.find(id);
        if (it == ix.end()) return false;
        if (use_replace_) { Order n = *it; n.price = px; n.ts = ts; return ix.replace(it, n); }
        return ix.modify(it, [&](Order& o) { o.price = px; o.ts = ts; });
    }
    // best n orders on a side, price-time priority; returns how many, writes ids
    int best_n(char side, int n, std::uint64_t* out) const {
        auto [b, e] = c_.template get<ByBook>().equal_range(std::make_tuple(side));
        int k = 0;
        for (; b != e && k < n; ++b) out[k++] = b->id;
        return k;
    }
    std::uint64_t client_qty(std::uint32_t client) const {
        std::uint64_t q = 0;
        auto [b, e] = c_.template get<ByClient>().equal_range(client);
        for (; b != e; ++b) q += b->qty;
        return q;
    }
    std::size_t size() const { return c_.size(); }

private:
    Container c_;
    bool use_replace_;
};
// ---- end copy ------------------------------------------------------------------------------------

struct Out { std::uint64_t checksum; std::size_t size; long ctor_news, work_news; };

template <class Store>
Out workload(Store& s, long ctor_news) {
    long before = g_news.load();
    std::uint64_t cs = 0;
    for (std::uint64_t i = 0; i < 5000; ++i)
        cs += s.add(Order{i, std::uint32_t(i % kClients), i % 2 ? 'B' : 'S', std::int64_t(100 + i % 50), i, std::uint32_t(10 + i % 7)});
    for (std::uint64_t i = 0; i < 5000; i += 3) cs += s.cancel(i) * 3;
    for (std::uint64_t i = 1; i < 5000; i += 5) cs += s.modify_price(i, std::int64_t(200 + i % 30), 10000 + i) * 5;
    for (std::uint64_t i = 5000; i < 8000; ++i) {            // churn: cancel oldest, add newest
        cs += s.cancel(i - 4000);
        cs += s.add(Order{i, std::uint32_t(i % kClients), 'B', std::int64_t(90 + i % 20), i, 5});
    }
    std::uint64_t ids[5];
    for (char side : {'B', 'S'}) { int k = s.best_n(side, 5, ids); cs = cs * 31 + k; for (int j = 0; j < k; ++j) cs = cs * 31 + ids[j]; }
    for (std::uint32_t c = 0; c < kClients; ++c) cs = cs * 131 + s.client_qty(c);
    return {cs, s.size(), ctor_news, g_news.load() - before};
}

int main() {
    Out a, b;
    {   // default allocator
        long n0 = g_news.load();
        MIStore<> s(10000, true);
        a = workload(s, g_news.load() - n0);
    }
    {   // THE ONE-LINE SWAP: template argument std::allocator<Order> -> hft::allocator<Order>
        hft::thread_arena arena;                       // once per thread, BEFORE the container
        long n0 = g_news.load();
        MIStore<hft::allocator<Order>> s(10000, true);
        b = workload(s, g_news.load() - n0);
    }
    std::printf("std::allocator : checksum %llu size %zu | operator new: construct %ld, workload %ld\n",
                (unsigned long long)a.checksum, a.size, a.ctor_news, a.work_news);
    std::printf("hft::allocator : checksum %llu size %zu | operator new: construct %ld, workload %ld\n",
                (unsigned long long)b.checksum, b.size, b.ctor_news, b.work_news);
    if (a.checksum != b.checksum || a.size != b.size) { std::puts("FAIL: results differ"); return 1; }
    if (a.work_news == 0 || b.work_news >= a.work_news) { std::puts("FAIL: swap did not change allocation behaviour"); return 1; }
    std::puts("OK: identical results");
    return 0;
}
