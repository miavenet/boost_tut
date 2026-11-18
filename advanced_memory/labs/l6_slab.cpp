// l6_slab.cpp — hft::slab<T>: carved once from the thread arena, O(1) free list, exhaustion
// policy, LIFO reuse. Run: ./labs/build/l6_slab
#include "hft_mem/hft_slab.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Order { long id, px, qty, client; char pad[32]; };   // 64 B

int main() {
    hft::arena_config cfg;
    cfg.region_bytes = std::size_t{256} << 20;     // room for the 64 MB slab below, pre-faulted
    hft::thread_arena arena(cfg);
    std::size_t used0 = arena.region().used();
    long news0 = g_news;
    {
        hft::slab<Order> s(4);
        std::printf("slab of 4: region used grew by %zu B (the monotonic layer takes a 1 MiB+ chunk), operator new calls %ld\n",
                    arena.region().used() - used0, g_news - news0);
        Order* a = s.create(); Order* b = s.create();
        std::printf("first two slots adjacent? %s (b - a = %td B)\n",
                    (reinterpret_cast<char*>(b) - reinterpret_cast<char*>(a)) == sizeof(Order) ? "yes" : "no",
                    reinterpret_cast<char*>(b) - reinterpret_cast<char*>(a));
        s.destroy(a);
        Order* c = s.create();
        std::printf("LIFO: create after destroy(a) returns a's slot? %s\n", c == a ? "yes" : "no");
        s.create(); s.create();
        std::printf("full? %d   try_create -> %s\n", s.full(), s.try_create() ? "an object" : "nullptr");
        try { s.create(); } catch (const std::bad_alloc&) { std::puts("create -> std::bad_alloc (never falls back to malloc)"); }
        for (std::uint32_t i = 0; i < 4; ++i) s.destroy(s.at(i));
    }

    // LIFO vs FIFO reuse on a slab much bigger than the caches (1,000,000 x 64 B = 64 MB),
    // half live, half free. Churn: destroy one random live order, create one, write it.
    // LIFO (hft::slab) hands back the slot just freed, whose line the destroy just touched.
    // FIFO (a ring of free slot numbers, for comparison only) hands back the slot freed
    // longest ago: a second, cold line per operation.
    constexpr std::uint32_t N = 1'000'000, LIVE = N / 2, OPS = 2'000'000;
    hft::slab<Order> s(N);
    std::uint64_t x = 88172645463325252ull;
    auto rnd = [&] { x ^= x << 13; x ^= x >> 7; x ^= x << 17; return x; };
    std::vector<Order*> live(LIVE);
    for (std::uint32_t i = 0; i < LIVE; ++i) live[i] = s.create(Order{i, 0, 0, 0, {}});

    auto t0 = std::chrono::steady_clock::now();
    for (std::uint32_t k = 0; k < OPS; ++k) {
        std::uint32_t j = rnd() % LIVE;
        s.destroy(live[j]);
        live[j] = s.create(Order{k, 1, 1, 1, {}});
    }
    double lifo = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / OPS;

    std::vector<std::uint32_t> ring(N);            // FIFO free list of slot numbers
    std::vector<std::uint32_t> where(LIVE);        // live order j sits in slot where[j]
    for (std::uint32_t i = 0; i < LIVE; ++i) where[i] = s.index_of(live[i]);
    std::vector<bool> used(N, false);
    for (auto w : where) used[w] = true;
    std::uint32_t head = 0, tail = 0;
    for (std::uint32_t i = 0; i < N; ++i) if (!used[i]) ring[tail++] = i;   // the free half, in address order
    tail %= N;
    t0 = std::chrono::steady_clock::now();
    for (std::uint32_t k = 0; k < OPS; ++k) {
        std::uint32_t j = rnd() % LIVE;
        Order* old = s.at(where[j]);
        old->id = -1;                               // "destroy": touch the freed order's line
        ring[tail] = where[j]; tail = (tail + 1) % N;
        std::uint32_t slot = ring[head]; head = (head + 1) % N;
        *s.at(slot) = Order{k, 1, 1, 1, {}};        // "create": write the oldest free slot
        where[j] = slot;
    }
    double fifo = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / OPS;
    std::printf("churn on a 64 MB slab, mean per destroy+create+write: LIFO %.1f ns, FIFO %.1f ns\n", lifo, fifo);
    std::puts("(FIFO is a ring of slot numbers written for this comparison, not hft::slab; it does not run ~T)");
    for (std::uint32_t i = 0; i < LIVE; ++i) s.destroy(live[i]);   // the slab's own view of what is live
}
