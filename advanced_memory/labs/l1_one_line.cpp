// l1_one_line.cpp — move an order book from malloc to the pooled hierarchy by changing ONE line.
// The program counts every global operator new during the hot loop (a "malloc counter").
// Run:  ./build/l1_one_line          Then edit the `using Book` line, rebuild, run again.
#include "hft_mem/hft_mem.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <new>

static std::atomic<long> g_news{0};
void* operator new(std::size_t n) {
    g_news.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

struct Order { long id; long px; long qty; };

using Book = std::map<long, Order>;   // <-- THE ONE LINE. Try: hft::map<long, Order>

int main() {
    hft::thread_arena arena;          // once per thread; does nothing for std:: containers
    Book book;
    for (long i = 0; i < 10'000; ++i) book.emplace(i, Order{i, 100 + i % 7, 1});

    long before = g_news.load();
    for (long i = 10'000; i < 110'000; ++i) {   // the hot path: cancel oldest, add newest
        book.erase(i - 10'000);
        book.emplace(i, Order{i, 100 + i % 7, 1});
    }
    long hot = g_news.load() - before;
    std::printf("book size %zu, hot-path operator new calls: %ld\n", book.size(), hot);
    return 0;
}
