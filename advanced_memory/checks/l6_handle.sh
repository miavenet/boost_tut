#!/bin/bash
# Card 06-Q5: a cancel request carries the handle `h` taken when the order was created. Write
# the statement(s) that cancel it ONLY if that same order is still alive (the slot may have
# been reused by another order since). Members: hft::slab<Order> pool; hft::slab<Order>::handle h.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include "hft_mem/hft_slab.hpp"
struct Order { long id; long qty; };
static void cancel_by_handle(hft::slab<Order>& pool, hft::slab<Order>::handle h) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'

}
int main() {
    hft::thread_arena arena;
    hft::slab<Order> pool(8);
    Order* a = pool.create(Order{1, 10});
    auto ha = pool.handle_of(a);
    cancel_by_handle(pool, ha);                    // live: must cancel
    if (pool.size() != 0) return 1;
    Order* b = pool.create(Order{2, 20});          // LIFO: b takes a's slot
    if (b != a) return 1;
    cancel_by_handle(pool, ha);                    // stale handle: must NOT cancel b
    if (pool.size() != 1 || b->id != 2) return 1;
    pool.destroy(b);
    return 0;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
