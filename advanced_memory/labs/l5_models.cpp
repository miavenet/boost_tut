// l5_models.cpp — the three ways to put an Order into containers, COUNTED on the same workload.
// Run: ./labs/build/l5_models
//
// Workload per model: insert N orders (64 price levels, id index where the model has one), then
// cancel N/10 of them. We count global operator new / delete calls and bytes, and sizeof.
// Columns: new/ins = operator new calls per insert; del/cxl = operator delete calls per cancel;
// B/ins = heap bytes per insert after one-time setup; B/total = all bytes per order, setup and
// storage included (reserved tables, the vector or slab holding the orders, list headers).
//   VALUE    the container owns the Order (std::map, std::list, boost flat_map, multi_index)
//   POINTER  the Order lives elsewhere; containers hold Order* (std::list<Order*>, unordered_map)
//   INTRUSIVE the Order carries the hooks; hft::slab owns storage; containers allocate nothing
#include "hft_mem/hft_slab.hpp"

#include <boost/container/flat_map.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <cstdio>
#include <cstdlib>
#include <list>
#include <map>
#include <new>
#include <unordered_map>
#include <vector>

static long g_news = 0, g_dels = 0, g_bytes = 0;
void* operator new(std::size_t n) {
    ++g_news; g_bytes += static_cast<long>(n);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { if (p) ++g_dels; std::free(p); }
void operator delete(void* p, std::size_t) noexcept { if (p) ++g_dels; std::free(p); }

namespace bi = boost::intrusive;
namespace bmi = boost::multi_index;

struct Order { long id; long px; long qty; long client; };   // 32 B payload
struct HOrder {                                              // same payload + 2 hooks
    long id; long px; long qty; long client;
    bi::list_member_hook<> level_hook, client_hook;
};

constexpr int N = 10'000, LEVELS = 64, CANCELS = N / 10;

struct counts { long news, dels, bytes; };
counts snap() { return {g_news, g_dels, g_bytes}; }
// a = before setup, a2 = after one-time setup (reserve, headers), b = after inserts, c = after cancels.
// extra = storage that did not come from operator new (the slab's arena memory).
void row(const char* name, counts a, counts a2, counts b, counts c, long extra, const char* chases) {
    std::printf("%-44s %7.2f %7.2f %8.1f %8.1f   %s\n", name,
                double(b.news - a2.news) / N, double(c.dels - b.dels) / CANCELS,
                double(b.bytes - a2.bytes) / N, double(b.bytes - a.bytes + extra) / N, chases);
}

int main() {
    std::printf("sizeof(Order)=%zu  sizeof(HOrder, 2 hooks)=%zu\n\n", sizeof(Order), sizeof(HOrder));
    std::printf("%-44s %7s %7s %8s %8s   %s\n", "model / container", "new/ins", "del/cxl", "B/ins", "B/total",
                "pointer chases (by structure): lookup by id | next in level");

    {   // VALUE: std::map<id, Order>
        counts a = snap();
        std::map<long, Order> m;
        for (long i = 0; i < N; ++i) m.emplace(i, Order{i, i % LEVELS, 1, i % 8});
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) m.erase(i * 10);
        counts c = snap();
        row("VALUE std::map<id,Order>", a, a, b, c, 0, "~log2(n) nodes | n/a (one order, one structure)");
    }
    {   // VALUE: std::list<Order> per level
        counts a = snap();
        std::vector<std::list<Order>> lv(LEVELS);
        counts a2 = snap();                       // exclude the vector of list headers
        for (long i = 0; i < N; ++i) lv[i % LEVELS].push_back(Order{i, i % LEVELS, 1, i % 8});
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) lv[i % LEVELS].pop_front();   // FIFO pop per level (no id index to find others)
        counts c = snap();
        row("VALUE std::list<Order> per level", a, a2, b, c, 0, "no id index | 1 (node->next)");
    }
    {   // VALUE: flat_map<id, Order>, reserved
        counts a = snap();
        boost::container::flat_map<long, Order> m;
        m.reserve(N);
        counts a2 = snap();
        for (long i = 0; i < N; ++i) m.emplace(i, Order{i, i % LEVELS, 1, i % 8});
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) m.erase(i * 10);
        counts c = snap();
        row("VALUE flat_map<id,Order> (reserved)", a, a2, b, c, 0, "binary search, contiguous | n/a");
    }
    {   // VALUE, library-owned node with 2 index headers: multi_index
        counts a = snap();
        using MI = bmi::multi_index_container<Order, bmi::indexed_by<
            bmi::hashed_unique<bmi::member<Order, long, &Order::id>>,
            bmi::ordered_non_unique<bmi::member<Order, long, &Order::px>>>>;
        MI s;
        s.reserve(N);
        counts a2 = snap();
        for (long i = 0; i < N; ++i) s.insert(Order{i, i % LEVELS, 1, i % 8});
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) s.erase(i * 10);
        counts c = snap();
        row("VALUE multi_index<hashed id, ordered px>", a, a2, b, c, 0, "bucket + node chain | 1 tree step (amortised)");
    }
    {   // POINTER: orders in a vector; std::list<Order*> per level + unordered_map<id, list iterator>
        counts a = snap();
        std::vector<Order> store;
        store.reserve(N);
        std::vector<std::list<Order*>> lv(LEVELS);
        std::unordered_map<long, std::list<Order*>::iterator> ids;
        counts a2 = snap();                       // exclude store + headers; count per-order work
        for (long i = 0; i < N; ++i) {
            store.push_back(Order{i, i % LEVELS, 1, i % 8});
            auto& l = lv[i % LEVELS];
            ids.emplace(i, l.insert(l.end(), &store.back()));
        }
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) {
            auto it = ids.find(i * 10);
            lv[(*it->second)->px].erase(it->second);
            ids.erase(it);
        }
        counts c = snap();
        row("POINTER list<Order*> + unordered_map<id,it>", a, a2, b, c, 0, "bucket+node, then node->Order | node->next->Order (2)");
    }
    {   // INTRUSIVE: slab + 2 hook lists + unordered_flat_map<id, HOrder*> reserved
        hft::thread_arena arena;
        counts a = snap();
        hft::slab<HOrder> orders(N);
        using LL = bi::list<HOrder, bi::member_hook<HOrder, bi::list_member_hook<>, &HOrder::level_hook>>;
        using CL = bi::list<HOrder, bi::member_hook<HOrder, bi::list_member_hook<>, &HOrder::client_hook>>;
        std::vector<LL> lv(LEVELS);
        std::vector<CL> cl(8);
        boost::unordered_flat_map<long, HOrder*> ids;
        ids.reserve(N);
        counts a2 = snap();                       // exclude one-time setup; it is reported below
        for (long i = 0; i < N; ++i) {
            HOrder* o = orders.create();
            o->id = i; o->px = i % LEVELS; o->qty = 1; o->client = i % 8;
            lv[o->px].push_back(*o);
            cl[o->client].push_back(*o);
            ids.emplace(i, o);
        }
        counts b = snap();
        for (long i = 0; i < CANCELS; ++i) {
            auto it = ids.find(i * 10);
            HOrder* o = it->second;
            lv[o->px].erase(LL::s_iterator_to(*o));
            cl[o->client].erase(CL::s_iterator_to(*o));
            ids.erase(it);
            orders.destroy(o);
        }
        counts c = snap();
        row("INTRUSIVE slab + 2 hook lists + flat id map", a, a2, b, c,
            long(N) * long(sizeof(HOrder) + sizeof(std::uint32_t)), "flat probe -> Order (1) | hook->next is in the Order (1)");
        std::printf("   (intrusive one-time setup: %ld operator new calls, %ld bytes; the slab, %zu B/order\n"
                    "    incl. its 4 B generation, came from the pre-faulted thread arena, not operator new)\n",
                    a2.news - a.news, a2.bytes - a.bytes, sizeof(HOrder) + sizeof(std::uint32_t));
        for (auto& l : cl) l.clear();
        for (auto& l : lv) l.clear_and_dispose([&](HOrder* p) { orders.destroy(p); });
    }
}
