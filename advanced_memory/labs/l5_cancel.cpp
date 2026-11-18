// l5_cancel.cpp — USE FIRST: cancel an order in O(1) with intrusive lists over a pooled slab.
// Run: ./labs/build/l5_cancel
//
// The order lives in ONE place: a slot of hft::slab<Order> (carved once from the thread arena).
// It is linked into two lists at once through two hooks it carries itself:
//   level_hook  -> the FIFO of its price level        (time priority)
//   client_hook -> the list of its client's orders    (mass-cancel per client)
// plus an id index: boost::unordered_flat_map<id, Order*> (reserved up front).
// Cancel = id lookup + two O(1) unlinks + O(1) slab release. Nothing allocates.
#include "hft_mem/hft_slab.hpp"

#include <boost/intrusive/list.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdlib>
#include <new>

static long g_news = 0;  // counts every global operator new in this program
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace bi = boost::intrusive;

struct Order {
    long id;
    int px;      // price level index (tick)
    int client;
    long qty;
    bi::list_member_hook<> level_hook;   // default link mode: safe_link (is_linked() works)
    bi::list_member_hook<> client_hook;
};
using LevelList = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::level_hook>>;
using ClientList = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::client_hook>>;

constexpr int LEVELS = 64, CLIENTS = 8, N = 10'000;

int main() {
    hft::thread_arena arena;                       // 1. the arena first: it dies last
    hft::slab<Order> orders(N);                    // 2. all Order storage, carved once
    LevelList levels[LEVELS];                      // 3. containers own NOTHING; they link
    ClientList clients[CLIENTS];
    boost::unordered_flat_map<long, Order*> by_id;
    by_id.reserve(N);

    for (long id = 0; id < N; ++id) {              // warm-up: add 10,000 orders
        Order* o = orders.create(Order{id, static_cast<int>(id % LEVELS), static_cast<int>(id % CLIENTS), 100, {}, {}});
        levels[o->px].push_back(*o);
        clients[o->client].push_back(*o);
        by_id.emplace(id, o);
    }

    long news_before = g_news;
    // ---- cancel order 4242 ----------------------------------------------------------------
    Order* o = by_id.find(4242)->second;           // id lookup: average O(1) hash probe
    std::printf("before: in level list? %d  in client list? %d  level %d size=%zu\n",
                o->level_hook.is_linked(), o->client_hook.is_linked(), o->px, levels[o->px].size());
    auto& lvl = levels[o->px];
    lvl.erase(lvl.iterator_to(*o));                // O(1): iterator FROM the object, no search
    clients[o->client].erase(clients[o->client].iterator_to(*o));   // O(1)
    std::printf("after:  in level list? %d  in client list? %d  level %d size=%zu\n",
                o->level_hook.is_linked(), o->client_hook.is_linked(), o->px, lvl.size());
    by_id.erase(4242);
    orders.destroy(o);                             // O(1): slot back on the free list
    // ----------------------------------------------------------------------------------------
    std::printf("operator new calls during the cancel: %ld\n", g_news - news_before);
    std::printf("live orders in slab: %u of %u\n", orders.size(), orders.capacity());

    // tidy shutdown: unlink everything, then give the storage back (lesson 06 shows the traps)
    for (auto& c : clients) c.clear();
    for (auto& l : levels) l.clear_and_dispose([&](Order* p) { orders.destroy(p); });
    std::printf("after shutdown: live orders %u\n", orders.size());
}
