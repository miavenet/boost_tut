// Capstone — a small order store, two ways, with a latency histogram per operation.
//
//   MIStore<Alloc>  : one boost::multi_index_container, 3 indices (id / book / client).
//                     The allocator is a template parameter (default std::allocator), so moving
//                     to a pooled allocator is a one-line change.
//   FastStore       : the HFT-style layout — pre-allocated slot pool, boost::intrusive lists,
//                     a price-level array (ladder) per side, boost::unordered_flat_map for id.
//
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/capstone.cpp -o build/capstone
// Run:    ./build/capstone            all variants
//         ./build/capstone mi         MIStore, modify() for price changes
//         ./build/capstone mi-replace MIStore, replace() for price changes
//         ./build/capstone mi-pmr     MIStore<std::pmr::polymorphic_allocator<Order>> on an arena
//         ./build/capstone fast       FastStore
//         ./build/capstone selftest   cross-check every variant against each other (exit 0 = agree)
//
// Timer: std::chrono::steady_clock; on Apple Silicon one tick = 41.67 ns, so single-op samples
// below ~42 ns read as 0 or 41/42. Compare tails (p99, p99.9, max), not the lowest bucket.
#include <boost/intrusive/list.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/key.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory_resource>
#include <vector>

// ----------------------------------------------------------------- the data
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

// ----------------------------------------------------------------- MIStore
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

// ----------------------------------------------------------------- FastStore
namespace bi = boost::intrusive;
struct LevelTag {};
struct ClientTag {};
struct Node : Order,
              bi::list_base_hook<bi::tag<LevelTag>, bi::link_mode<bi::normal_link>>,
              bi::list_base_hook<bi::tag<ClientTag>, bi::link_mode<bi::normal_link>> {};
using LevelList = bi::list<Node, bi::base_hook<bi::list_base_hook<bi::tag<LevelTag>, bi::link_mode<bi::normal_link>>>,
                           bi::constant_time_size<false>>;
using ClientList = bi::list<Node, bi::base_hook<bi::list_base_hook<bi::tag<ClientTag>, bi::link_mode<bi::normal_link>>>,
                            bi::constant_time_size<false>>;

class FastStore {
public:
    explicit FastStore(std::size_t capacity) : pool_(capacity) {
        free_.reserve(capacity);
        for (std::size_t i = capacity; i-- > 0;) free_.push_back(std::uint32_t(i));
        by_id_.reserve(capacity);
    }
    bool add(const Order& o) {
        if (free_.empty() || o.price < 0 || o.price >= kTicks) return false;
        auto [it, fresh] = by_id_.try_emplace(o.id, 0u);
        if (!fresh) return false;
        std::uint32_t slot = free_.back();
        free_.pop_back();
        it->second = slot;
        Node& n = pool_[slot];
        static_cast<Order&>(n) = o;
        link(n);
        clients_[o.client].push_back(n);
        return true;
    }
    bool cancel(std::uint64_t id) {
        auto it = by_id_.find(id);
        if (it == by_id_.end()) return false;
        Node& n = pool_[it->second];
        unlink(n);
        clients_[n.client].erase(ClientList::s_iterator_to(n));
        free_.push_back(it->second);
        by_id_.erase(it);
        return true;
    }
    bool modify_price(std::uint64_t id, std::int64_t px, std::uint64_t ts) {
        auto it = by_id_.find(id);
        if (it == by_id_.end() || px < 0 || px >= kTicks) return false;
        Node& n = pool_[it->second];
        unlink(n);
        n.price = px;
        n.ts = ts;           // a price change loses time priority: goes to the back of the new level
        link(n);
        return true;
    }
    int best_n(char side, int n, std::uint64_t* out) const {
        const Side& s = side == 'B' ? bids_ : asks_;
        int k = 0;
        if (s.count == 0) return 0;
        int step = side == 'B' ? -1 : 1;
        for (int px = s.best; px >= 0 && px < kTicks && k < n; px += step)
            for (const Node& o : s.level[px]) { if (k == n) break; out[k++] = o.id; }
        return k;
    }
    std::uint64_t client_qty(std::uint32_t client) const {
        std::uint64_t q = 0;
        for (const Node& o : clients_[client]) q += o.qty;
        return q;
    }
    std::size_t size() const { return by_id_.size(); }

private:
    struct Side {
        std::array<LevelList, kTicks> level;
        int best = -1;          // best non-empty level, -1 = empty side
        std::size_t count = 0;
    };
    void link(Node& n) {
        Side& s = n.side == 'B' ? bids_ : asks_;
        int px = int(n.price);
        s.level[px].push_back(n);  // FIFO within a level = time priority (ts is increasing)
        if (s.count++ == 0 || (n.side == 'B' ? px > s.best : px < s.best)) s.best = px;
    }
    void unlink(Node& n) {
        Side& s = n.side == 'B' ? bids_ : asks_;
        int px = int(n.price);
        s.level[px].erase(LevelList::s_iterator_to(n));
        if (--s.count == 0) { s.best = -1; return; }
        if (px == s.best && s.level[px].empty()) {      // walk to the next non-empty level
            int step = n.side == 'B' ? -1 : 1;
            while (s.level[s.best].empty()) s.best += step;
        }
    }
    std::vector<Node> pool_;                   // every order lives here; never reallocated
    std::vector<std::uint32_t> free_;          // free slot stack
    boost::unordered_flat_map<std::uint64_t, std::uint32_t> by_id_;
    Side bids_, asks_;
    std::array<ClientList, kClients> clients_;
};

// ----------------------------------------------------------------- latency histogram
using Clock = std::chrono::steady_clock;
struct Hist {
    std::vector<double> ns;
    std::array<std::uint64_t, 24> bucket{};     // bucket b holds samples in [2^b, 2^(b+1)) ns
    void add(double v) {
        ns.push_back(v);
        int b = 0;
        while (b < 23 && v >= double(2ull << b)) ++b;
        ++bucket[std::size_t(b)];
    }
    double pct(double p) {
        std::size_t i = std::min(ns.size() - 1, std::size_t(p / 100.0 * double(ns.size())));
        std::nth_element(ns.begin(), ns.begin() + std::ptrdiff_t(i), ns.end());
        return ns[i];
    }
    void report(const char* op) {
        if (ns.empty()) return;
        double mx = *std::max_element(ns.begin(), ns.end());
        std::printf("  %-12s n=%-7zu p50=%7.0f p99=%7.0f p99.9=%8.0f max=%9.0f ns  |", op, ns.size(), pct(50),
                    pct(99), pct(99.9), mx);
        for (int b = 0; b < 24; ++b)            // print every non-empty power-of-two bucket
            if (bucket[std::size_t(b)]) std::printf(" <%s:%llu", label(b + 1), (unsigned long long)bucket[std::size_t(b)]);
        std::printf("\n");
    }
    static const char* label(int b) {
        static char buf[24][12];
        double v = double(1ull << b);
        if (v < 1e3) std::snprintf(buf[b], 12, "%.0fns", v);
        else if (v < 1e6) std::snprintf(buf[b], 12, "%.0fus", v / 1e3);
        else std::snprintf(buf[b], 12, "%.0fms", v / 1e6);
        return buf[b];
    }
};

// ----------------------------------------------------------------- the workload (deterministic)
struct Rng {
    std::uint64_t s = 0x2545F4914F6CDD1Dull;
    std::uint64_t operator()() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
};
constexpr std::size_t kLive = 100'000;    // resting orders in steady state
constexpr std::size_t kOps = 1'000'000;

struct Result { std::uint64_t checksum = 0; std::size_t size = 0; };

template <class Store>
Result run(const char* name, Store& st, bool print) {
    Rng r;
    Hist h_add, h_cancel, h_mod, h_best, h_client;
    std::vector<std::uint64_t> live;          // ids we believe are live (workload side)
    live.reserve(kLive * 2);
    std::uint64_t next_id = 1, ts = 0, checksum = 0;
    auto new_order = [&] {
        char side = (r() & 1) ? 'B' : 'S';
        // bids rest below 512, asks at/above 512, both clustered near the touch
        std::int64_t off = std::int64_t(r() % 64);
        std::int64_t px = side == 'B' ? 511 - off : 512 + off;
        return Order{next_id++, std::uint32_t(r() % kClients), side, px, ++ts, std::uint32_t(1 + r() % 100)};
    };
    for (std::size_t i = 0; i < kLive; ++i) { Order o = new_order(); st.add(o); live.push_back(o.id); }

    std::uint64_t out[16];
    for (std::size_t i = 0; i < kOps; ++i) {
        std::uint64_t dice = r() % 100;
        auto t0 = Clock::now();
        if (dice < 45) {                                   // add
            Order o = new_order();
            t0 = Clock::now();
            st.add(o);
            h_add.add(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
            live.push_back(o.id);
        } else if (dice < 90) {                            // cancel a random live order
            std::size_t k = r() % live.size();
            std::uint64_t id = live[k];
            live[k] = live.back(); live.pop_back();
            t0 = Clock::now();
            checksum += st.cancel(id);
            h_cancel.add(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
        } else if (dice < 97) {                            // modify price of a random live order
            std::uint64_t id = live[r() % live.size()];
            std::int64_t off = std::int64_t(r() % 64);
            std::int64_t px = (r() & 1) ? 511 - off : 512 + off;   // new price anywhere near the touch
            ++ts;
            t0 = Clock::now();
            checksum += st.modify_price(id, px, ts);
            h_mod.add(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
        } else if (dice < 99) {                            // best 5 bids or asks
            char side = (r() & 1) ? 'B' : 'S';
            t0 = Clock::now();
            int k = st.best_n(side, 5, out);
            h_best.add(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
            for (int j = 0; j < k; ++j) checksum = checksum * 31 + out[j];
        } else {                                           // total open qty of one client
            std::uint32_t c = std::uint32_t(r() % kClients);
            t0 = Clock::now();
            std::uint64_t q = st.client_qty(c);
            h_client.add(std::chrono::duration<double, std::nano>(Clock::now() - t0).count());
            checksum += q;
        }
    }
    if (print) {
        std::printf("%s  (live=%zu, ops=%zu)\n", name, st.size(), kOps);
        h_add.report("add");
        h_cancel.report("cancel");
        h_mod.report("modify_price");
        h_best.report("best_5");
        h_client.report("client_qty");
    }
    return {checksum, st.size()};
}

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "all";
    bool all = std::strcmp(which, "all") == 0, self = std::strcmp(which, "selftest") == 0;
    auto is = [&](const char* w) { return all || self || std::strcmp(which, w) == 0; };
    std::size_t cap = kLive * 2;
    std::vector<Result> res;
    std::printf("M1 Max, Apple clang 21, -O2, Boost 1.90; timer tick 41.67 ns. Re-run on YOUR box.\n");
    if (is("mi"))         { MIStore<> s(cap, false); res.push_back(run("MIStore<std::allocator>, modify()", s, !self)); }
    if (is("mi-replace")) { MIStore<> s(cap, true);  res.push_back(run("MIStore<std::allocator>, replace()", s, !self)); }
    if (is("mi-pmr")) {
        std::pmr::monotonic_buffer_resource arena(std::size_t(256) << 20);   // 256 MiB upfront
        // THE one-line allocator swap: only the template argument changes.
        MIStore<std::pmr::polymorphic_allocator<Order>> s(cap, true, &arena);
        res.push_back(run("MIStore<pmr::polymorphic_allocator>, replace(), monotonic arena", s, !self));
    }
    if (is("fast"))       { auto s = std::make_unique<FastStore>(cap); res.push_back(run("FastStore (pool + intrusive + ladder + flat_map)", *s, !self)); }
    if (self) {
        bool ok = !res.empty();
        for (auto& x : res) ok = ok && x.checksum == res[0].checksum && x.size == res[0].size;
        std::printf("selftest: %zu variants, checksum %llu, size %zu -> %s\n", res.size(),
                    (unsigned long long)res[0].checksum, res[0].size, ok ? "AGREE" : "DISAGREE");
        return ok ? 0 : 1;
    }
    return 0;
}
