// l4_capstone.cpp — the library in use: a multi_index order store + a flat_map price-level table,
// both on hft_mem, warmed and sealed before the hot path. Same source, two builds:
//   build/l4_capstone            production mode: a full region falls back to malloc (counted)
//   build/l4_capstone_tripwire   -DHFT_MEM_TRIPWIRE=1: a full region throws instead
// Run each with no argument (fits) and with `tiny` (a 1 MiB region that cannot hold the book).
#include "hft_mem/hft_mem.hpp"

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <cstdio>
#include <string_view>

namespace bmi = boost::multi_index;

struct Order {
    long id;
    long px;
    long qty;
};
struct by_id {};
struct by_px {};
using OrderStore = hft::multi_index<Order, bmi::indexed_by<
    bmi::hashed_unique<bmi::tag<by_id>, bmi::member<Order, long, &Order::id>>,
    bmi::ordered_non_unique<bmi::tag<by_px>, bmi::member<Order, long, &Order::px>>>>;
using Levels = hft::flat_map<long, long>;   // px -> total qty

int main(int argc, char** argv) {
    const bool tiny = argc > 1 && std::string_view(argv[1]) == "tiny";
    hft::arena_config cfg;
    cfg.region_bytes = tiny ? std::size_t{1} << 20 : std::size_t{64} << 20;

    try {
        hft::thread_arena arena(cfg);        // 1. arena first (pin the thread before this on Linux)
        OrderStore orders;                   // 2. then the containers: they die before the arena
        Levels levels;
        orders.get<by_id>().reserve(100'000);
        levels.reserve(512);
        for (long i = 0; i < 50'000; ++i) {  // 3. warm-up: build the book
            orders.insert({i, 1000 + i % 400, 1});
            levels[1000 + i % 400] += 1;
        }
        arena.seal();                        // 4. hot path: no new memory from here on
        for (long i = 50'000; i < 250'000; ++i) {
            auto& ids = orders.get<by_id>();
            auto it = ids.find(i - 50'000);  // cancel the oldest ...
            levels[it->px] -= it->qty;
            ids.erase(it);
            orders.insert({i, 1000 + i % 400, 1});   // ... add a new one (reuses the freed node)
            levels[1000 + i % 400] += 1;
        }
        auto& by_price = orders.get<by_px>();
        std::printf("orders=%zu levels=%zu best px=%ld  region used=%zu KiB  overflow allocations=%zu\n",
                    orders.size(), levels.size(), by_price.begin()->px, arena.region().used() >> 10,
                    arena.region().overflow_count());
    } catch (const std::bad_alloc&) {
        std::puts("TRIPWIRE: region exhausted (tripwire mode) -> std::bad_alloc; size the region up");
        return 3;
    }
    return 0;
}
