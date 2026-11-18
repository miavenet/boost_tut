// l7_order_book.cpp — the full pattern: an intrusive order book over hft::slab (hft_book.hpp).
// Run: ./labs/build/l7_order_book
#include "hft_mem/hft_book.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace hft::book;

int main() {
    hft::thread_arena arena;                                  // first: dies last
    book<IdIndex::flat> b(100'000, 1024, 16);                 // exchange-style ids: hash index

    for (long id = 1; id <= 50'000; ++id) b.add(id, static_cast<int>(id % 1024), static_cast<int>(id % 16), 100);
    long news0 = g_news;

    Order* o = b.find(777);
    std::printf("order 777: pending-send? %d pending-ack? %d\n", book<>::is_pending_send(*o), book<>::is_pending_ack(*o));
    b.mark_sent(*o);
    std::printf("after mark_sent: pending-send? %d pending-ack? %d  (send queue %zu, ack queue %zu)\n",
                book<>::is_pending_send(*o), book<>::is_pending_ack(*o), b.send_queue().size(), b.ack_queue().size());

    std::printf("level %d before: %zu orders, qty %ld\n", o->px, b.level(o->px).size(), b.level_qty(o->px));
    b.reduce(777, 40);                                       // in place, keeps time priority
    std::printf("after reduce(777 -> 40): qty %ld\n", b.level_qty(o->px));
    int px = o->px;
    b.cancel(777);                                           // id lookup + 4 O(1) unlinks + release
    std::printf("after cancel(777): level %d has %zu orders, ack queue %zu, find(777)=%p\n",
                px, b.level(px).size(), b.ack_queue().size(), static_cast<void*>(b.find(777)));

    std::size_t n = b.cancel_client(3);                      // walk one client list, unlink everywhere
    std::printf("mass cancel client 3: %zu orders; book now %u orders\n", n, b.size());

    // churn: 200,000 x (cancel the oldest live, add a new one) -- the hot loop
    long next = 50'001, oldest = 1;
    for (int i = 0; i < 200'000; ++i) {
        while (!b.cancel(oldest)) ++oldest;                  // skip ids already gone
        ++oldest;
        b.add(next, static_cast<int>(next % 1024), static_cast<int>(next % 16), 100);
        ++next;
    }
    std::printf("operator new calls after warm-up (whole hot section): %ld\n", g_news - news0);

    // Variant: the id you send to the venue IS the slab handle (index + generation): no hash.
    book<IdIndex::handle> hb(1'000, 16, 2);
    Order* h = hb.add(0, 5, 1, 10);
    long hid = h->id;
    hb.cancel(hid);
    Order* reuse = hb.add(0, 5, 1, 10);                      // LIFO: same slot, new generation
    std::printf("handle ids: same slot reused? %s; cancel(old id) again -> %s\n",
                reuse == h ? "yes" : "no", hb.cancel(hid) ? "cancelled the NEW order (BUG)" : "false (stale id rejected)");
}
