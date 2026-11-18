#!/bin/bash
# Card 05-Q2: body of `void cancel(Book& b, long id)`: remove the order from EVERY structure it
# is in and give its slot back to the slab, in O(1). Built with asserts on (safe_link checks active).
# Guard: no loop and no linear search (O(1) given the id: lookup + iterator_to, never a scan).
# Levels hold 3-4 orders and one cancel hits the MIDDLE of a level, so pop_front/back is caught.
. "$(dirname "$0")/lib.inc"
read_answer
python3 - "$WORK/answer.txt" <<'GUARD' || exit 1
import re, sys
s = open(sys.argv[1]).read()
s = re.sub(r'/\*.*?\*/', '', re.sub(r'//[^\n]*', '', s), flags=re.S)
sys.exit(1 if re.search(r'\b(for|while|do|goto)\b|remove|find_if|any_of|std::find\b|\bbegin\(', s) else 0)
GUARD
{
cat <<'CPP'
#include "hft_mem/hft_slab.hpp"
#include <boost/intrusive/list.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdlib>
#include <iterator>
#include <new>
static long g_news = 0;
void* operator new(std::size_t n) { ++g_news; if (void* p = std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
namespace bi = boost::intrusive;
struct Order { long id; int px; int client; long qty; bi::list_member_hook<> level_hook, client_hook; };
using Level = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::level_hook>>;
using Client = bi::list<Order, bi::member_hook<Order, bi::list_member_hook<>, &Order::client_hook>>;
struct Book {
    hft::slab<Order> orders{1000};
    Level levels[64];
    Client clients[8];
    boost::unordered_flat_map<long, Order*> ids;
};
void cancel(Book& b, long id) {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
}
int main() {
    hft::thread_arena arena;
    {
        Book b;
        b.ids.reserve(1000);
        for (long i = 0; i < 200; ++i) {   // level px = i % 64 (3-4 orders each), client = i % 8
            Order* o = b.orders.create(Order{i, int(i % 64), int(i % 8), 10, {}, {}});
            b.levels[o->px].push_back(*o); b.clients[o->client].push_back(*o); b.ids.emplace(i, o);
        }
        long n0 = g_news;
        cancel(b, 42);                     // front of level 42 {42,106,170}, client 2
        cancel(b, 135);                    // MIDDLE of level 7 {7,71,135,199}, client 7
        if (g_news != n0) return 1;                                  // nothing allocated
        if (b.orders.size() != 198 || b.ids.size() != 198 || b.ids.contains(42) || b.ids.contains(135)) return 1;
        long l42[] = {106, 170}, l7[] = {7, 71, 199};
        auto same = [](auto& list, auto& want) { std::size_t k = 0; for (auto& o : list) { if (k >= std::size(want) || o.id != want[k]) return false; ++k; } return k == std::size(want); };
        if (!same(b.levels[42], l42) || !same(b.levels[7], l7)) return 1;
        if (b.clients[2].size() != 24 || b.clients[7].size() != 24) return 1;
        for (auto& o : b.clients[2]) if (o.id == 42) return 1;
        for (auto& o : b.clients[7]) if (o.id == 135) return 1;
        if (!b.ids.contains(7) || b.ids.find(106)->second->id != 106) return 1;   // the others untouched
        Order* again = b.orders.create(Order{500, 1, 1, 1, {}, {}});  // reuses a freed slot
        if (again->level_hook.is_linked() || again->client_hook.is_linked()) return 1;
        b.orders.destroy(again);
        for (auto& c : b.clients) c.clear();
        for (auto& l : b.levels) l.clear_and_dispose([&](Order* o) { b.orders.destroy(o); });
    }
    return 0;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
