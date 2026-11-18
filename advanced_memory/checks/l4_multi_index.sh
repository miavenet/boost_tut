#!/bin/bash
# Card 04-Q1: the order-store type: a boost::multi_index_container of Order with the given
# `Indices`, whose nodes come from the thread's hft arena. Answer (stdin): a type.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include "hft_mem/hft_mem.hpp"
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <cstdlib>
#include <new>
static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace bmi = boost::multi_index;
struct Order { long id; long px; };
using Indices = bmi::indexed_by<bmi::hashed_unique<bmi::member<Order, long, &Order::id>>,
                                bmi::ordered_non_unique<bmi::member<Order, long, &Order::px>>>;
using Store =
CPP
tr -d ';' < "$WORK/answer.txt"
cat <<'CPP'
;
static_assert(std::is_same_v<Store::value_type, Order>);
int main() {
    hft::thread_arena arena;
    long before = g_news;
    {
        Store s;
        for (long i = 0; i < 2000; ++i) s.insert(Order{i, i % 17});
        if (s.size() != 2000 || s.get<1>().begin()->px != 0) return 1;
    }
    return g_news == before ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
