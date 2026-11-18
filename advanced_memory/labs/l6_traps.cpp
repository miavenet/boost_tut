// l6_traps.cpp — break intrusive-over-a-pool on purpose, then fix it. One trap per argument.
// Two builds of this file (labs/build.sh):
//   build/l6_traps          asserts ON  (no -DNDEBUG): safe_link checks fire
//   build/l6_traps_ndebug   -DNDEBUG    : the same checks compile to nothing
// Run: ./labs/build/l6_traps <trap> ; echo "exit=$?"   (traps listed at the bottom of main)
#include "hft_mem/hft_slab.hpp"

#include <boost/intrusive/list.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace bi = boost::intrusive;

struct Order {
    long id;
    bi::list_member_hook<> level_hook;                   // safe_link
};
using Level = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::level_hook>>;

struct NOrder {
    long id;
    bi::list_member_hook<bi::link_mode<bi::normal_link>> h;
};
using NLevel = bi::list<NOrder, bi::member_hook<NOrder, bi::list_member_hook<bi::link_mode<bi::normal_link>>, &NOrder::h>>;

struct AOrder {
    long id;
    bi::list_member_hook<bi::link_mode<bi::auto_unlink>> h;
};
using ALevel = bi::list<AOrder, bi::member_hook<AOrder, bi::list_member_hook<bi::link_mode<bi::auto_unlink>>, &AOrder::h>,
                        bi::constant_time_size<false>>;

static void print_level(const char* what, Level& l) {
    std::printf("%s:", what);
    for (auto& o : l) std::printf(" %ld", o.id);
    std::printf("  (size()=%zu)\n", l.size());
    std::fflush(stdout);
}

int main(int argc, char** argv) {
    std::string_view t = argc > 1 ? argv[1] : "";
    hft::thread_arena arena;

    if (t == "linked-destroy") {          // TRAP 1: return an object to the pool while still linked
        hft::slab<Order> pool(16);
        Level lvl;
        for (long i = 1; i <= 3; ++i) lvl.push_back(*pool.create(Order{i, {}}));
        Order* two = &*std::next(lvl.begin());
        std::puts("destroy(order 2) while it is still in the level ...");
        std::fflush(stdout);
        pool.destroy(two);                 // asserts here when NDEBUG is not defined
        Order* fresh = pool.create(Order{99, {}});   // LIFO: reuses order 2's slot
        std::printf("new order 99 got the old slot? %s\n", fresh == two ? "yes" : "no");
        std::fflush(stdout);
        print_level("level walk", lvl);    // the level now 'contains' order 99, never added to it
        lvl.clear();
        pool.destroy(fresh);
        for (auto& o : lvl) pool.destroy(&o);
        // FIX: unlink first, then destroy: lvl.erase_and_dispose(it, [&](Order* o){ pool.destroy(o); })
        std::puts("(reached the end)");
        return 0;
    }
    if (t == "order-dies-first") {        // TRAP 2a: objects die before the container holding them
        Level lvl;                         // declared FIRST: destroyed LAST
        Order a{1, {}};
        lvl.push_back(a);
        std::puts("scope ends: ~Order runs while still linked ...");
        std::fflush(stdout);
        return 0;                          // a destroyed first (assert), then lvl clears a dead object
        // FIX: declare (or create) the objects' owner BEFORE the containers that link them
    }
    if (t == "container-dies") {          // TRAP 2b: the container dies while holding objects
        hft::slab<Order> pool(1'000'000);
        hft::slab<NOrder> npool(1'000'000);
        double safe_us, normal_us;
        {
            auto* lvl = new Level;
            for (int i = 0; i < 1'000'000; ++i) lvl->push_back(*pool.create(Order{i, {}}));
            auto t0 = std::chrono::steady_clock::now();
            delete lvl;                    // safe_link: ~list unlinks every element, O(n)
            safe_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        }
        NLevel* nlvl = new NLevel;
        for (int i = 0; i < 1'000'000; ++i) nlvl->push_back(*npool.create(NOrder{i, {}}));
        auto t0 = std::chrono::steady_clock::now();
        delete nlvl;                       // normal_link: ~list does nothing to the elements, O(1)
        normal_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        std::printf("~list with 1,000,000 orders: safe_link %.0f us, normal_link %.1f us\n", safe_us, normal_us);
        std::puts("normal_link: the orders' hooks were not touched (they still hold links into the dead list;\n"
                  "             there is no is_linked() to ask, so only your own bookkeeping knows)");
        std::printf("safe_link: every order is unlinked again, slab can take them back: live %u\n", pool.size());
        for (std::uint32_t i = 0; i < 1'000'000; ++i) pool.destroy(pool.at(i));
        for (std::uint32_t i = 0; i < 1'000'000; ++i) npool.destroy(npool.at(i));
        return 0;
    }
    if (t == "autounlink") {              // TRAP 3: auto_unlink surprises
        ALevel lvl;
        AOrder a{1, {}};
        lvl.push_back(a);
        AOrder copy = a;                   // hooks are not copied: the copy is NOT in the list
        std::printf("copy of a linked order is linked? %d   list size=%zu\n", copy.h.is_linked(), lvl.size());
        {
            AOrder tmp{2, {}};
            lvl.push_back(tmp);
            std::printf("pushed a temporary: size=%zu\n", lvl.size());
        }                                  // tmp's destructor quietly removes it
        std::printf("temporary went out of scope: size=%zu  <- nobody called erase\n", lvl.size());
        lvl.clear();
        return 0;
    }
    if (t == "aba") {                     // TRAP 4: stale pointer after the slot is reused
        hft::slab<Order> pool(16);
        Order* p = pool.create(Order{7, {}});
        auto h = pool.handle_of(p);        // FIX: keep {index, generation}, not a raw pointer
        pool.destroy(p);                   // order 7 cancelled
        Order* q = pool.create(Order{8, {}});   // LIFO: same slot
        std::printf("raw pointer to order 7 now reads id=%ld (same address as order 8? %s)\n",
                    p->id, p == q ? "yes" : "no");
        std::printf("handle {index %u, gen %u} -> get() = %s\n", h.index, h.gen,
                    pool.get(h) ? "an order (WRONG)" : "nullptr (stale: order 7 is gone)");
        pool.destroy(q);
        return 0;
    }
    if (t == "iterate" || t == "iterate-fixed") {   // TRAP 5: unlinking while iterating
        hft::slab<Order> pool(16);
        Level lvl;
        for (long i = 1; i <= 6; ++i) lvl.push_back(*pool.create(Order{i, {}}));
        if (t == "iterate") {
            std::puts("for (auto& o : lvl) if (o.id % 2 == 0) lvl.erase(lvl.iterator_to(o)); ...");
            std::fflush(stdout);
            long steps = 0;
            for (auto& o : lvl) {          // ++ after erase reads the erased hook
                if (++steps > 100) { std::puts("stopped after 100 steps: the loop never ended"); break; }
                if (o.id % 2 == 0) lvl.erase(lvl.iterator_to(o));
            }
            std::puts("loop finished");
        } else {                           // FIX: erase returns the next iterator
            lvl.remove_and_dispose_if([](const Order& o) { return o.id % 2 == 0; },
                                      [&](Order* o) { pool.destroy(o); });
        }
        print_level("level", lvl);
        if (t == "iterate")                // the erased ones are unlinked but still live in the slab
            for (std::uint32_t i = 0; i < 6; ++i) if (!pool.at(i)->level_hook.is_linked()) pool.destroy(pool.at(i));
        lvl.clear_and_dispose([&](Order* o) { pool.destroy(o); });
        return 0;
    }
    std::puts("traps: linked-destroy | order-dies-first | container-dies | autounlink | aba | iterate | iterate-fixed");
    return 2;
}
