#!/bin/bash
# Card 01-Q2: type the Book alias (a NODE-based ordered map: element addresses stay valid while
# others are inserted/erased) so the hot loop makes ZERO global operator new calls.
# Answer (stdin): a type, e.g.  hft::map<long, Order>
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include "hft_mem/hft_mem.hpp"
#include <cstdlib>
#include <new>
static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
struct Order { long id; long px; long qty; };
using Book =
CPP
tr -d ';' < "$WORK/answer.txt"
cat <<'CPP'
;
int main() {
    hft::thread_arena arena;
    Book book;
    for (long i = 0; i < 1000; ++i) book.emplace(i, Order{i, 1, 1});
    long before = g_news;
    const Order* held = &book.find(999)->second;   // a node map never moves an element
    for (long i = 1000; i < 11000; ++i) {
        book.erase(i - 1000); book.emplace(i, Order{i, 1, 1});
        if (i == 1500 && &book.find(999)->second != held) return 1;   // flat_map shifts it
    }
    if (book.size() != 1000 || book.begin()->first != 10000) return 1;
    return g_news == before ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
