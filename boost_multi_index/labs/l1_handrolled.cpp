// Lab 1a — the hand-rolled order store: three std containers kept in sync by hand.
// Build:  clang++ -std=c++23 -O2 -I/opt/homebrew/include labs/l1_handrolled.cpp -o build/l1_handrolled
// Run:    ./build/l1_handrolled            (counts heap allocations per operation)
//         ./build/l1_handrolled bug        (cancel() "forgets" one index -> dangling pointers)
//         ./build/l1_handrolled stale      (price changed via by_id only -> book key is stale)
//
// Break-it-on-purpose: rebuild with -fsanitize=address -g and run with "bug":
//   clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include labs/l1_handrolled.cpp -o build/l1_asan
//   ./build/l1_asan bug      -> AddressSanitizer: heap-use-after-free
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <tuple>
#include <unordered_map>

// ---- global allocation counter (counts every operator new in the process) ----
static long g_allocs = 0;
void* operator new(std::size_t n) {
    ++g_allocs;
    if (void* p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Order {
    std::uint64_t id;
    std::uint32_t client;
    char side;            // 'B' or 'S'
    std::int64_t price;   // ticks
    std::uint64_t ts;     // arrival sequence
    std::uint32_t qty;
};

using PriceKey = std::tuple<char, std::int64_t, std::uint64_t>;  // (side, price, time)

struct HandRolledStore {
    std::unordered_map<std::uint64_t, Order> by_id;              // owns the Order
    std::map<PriceKey, Order*> by_price;                         // points into by_id
    std::unordered_multimap<std::uint32_t, Order*> by_client;    // points into by_id

    void add(const Order& o) {
        auto [it, ok] = by_id.emplace(o.id, o);
        if (!ok) return;
        Order* p = &it->second;  // stable: unordered_map never moves its nodes
        by_price.emplace(PriceKey{o.side, o.price, o.ts}, p);
        by_client.emplace(o.client, p);
    }
    void cancel(std::uint64_t id, bool forget_client_index) {
        auto it = by_id.find(id);
        if (it == by_id.end()) return;
        Order& o = it->second;
        by_price.erase(PriceKey{o.side, o.price, o.ts});
        if (!forget_client_index) {                      // the line a tired human forgets
            auto [b, e] = by_client.equal_range(o.client);
            for (; b != e; ++b)
                if (b->second == &o) { by_client.erase(b); break; }
        }
        by_id.erase(it);                                 // Order destroyed here
    }
};

int main(int argc, char** argv) {
    bool bug = argc > 1 && std::strcmp(argv[1], "bug") == 0;
    bool stale = argc > 1 && std::strcmp(argv[1], "stale") == 0;
    HandRolledStore s;
    s.by_id.reserve(1024);
    s.by_client.reserve(1024);  // pre-size so bucket arrays do not muddy the count

    const int N = 1000;
    long before = g_allocs;
    for (int i = 0; i < N; ++i)
        s.add(Order{std::uint64_t(i), std::uint32_t(i % 7), (i & 1) ? 'S' : 'B',
                    100 + i % 50, std::uint64_t(i), 10});
    long per_insert = (g_allocs - before) / N;
    std::printf("hand-rolled: allocations per add()    = %ld\n", per_insert);

    before = g_allocs;
    for (int i = 0; i < N / 2; ++i) s.cancel(std::uint64_t(i), bug);
    std::printf("hand-rolled: allocations per cancel() = %ld\n", (g_allocs - before) / (N / 2));
    std::printf("sizes: by_id=%zu by_price=%zu by_client=%zu\n",
                s.by_id.size(), s.by_price.size(), s.by_client.size());

    if (bug) {
        // Client 0's orders: walk the client index and read each order's qty.
        long qty = 0;
        auto [b, e] = s.by_client.equal_range(0);
        for (; b != e; ++b) qty += b->second->qty;   // dangling Order* if cancel forgot us
        std::printf("client 0 open qty = %ld  (garbage or crash if any pointer dangles)\n", qty);
    }
    if (stale) {
        // Reprice order 999 (a sell at 149) to 1 tick -- but only through by_id.
        Order& o = s.by_id.at(999);
        o.price = 1;
        // The best ask should now be order 999 at price 1. Ask the book:
        auto it = s.by_price.lower_bound(PriceKey{'S', 0, 0});
        std::printf("book says best ask = order %llu, key price %lld, order's real price %lld\n",
                    (unsigned long long)it->second->id, (long long)std::get<1>(it->first),
                    (long long)it->second->price);
        auto k = s.by_price.find(PriceKey{'S', 149, 999});
        std::printf("order 999 still filed under price %lld in the book: %s\n", 149LL,
                    k != s.by_price.end() ? "yes (stale key)" : "no");
    }
    return 0;
}
