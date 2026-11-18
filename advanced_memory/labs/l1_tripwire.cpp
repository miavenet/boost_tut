// l1_tripwire.cpp — make an unexpected hot-path allocation FAIL LOUDLY in test.
// After warm-up we call arena.seal(): the pool may still recycle blocks it already owns, but any
// request for NEW memory throws std::bad_alloc. Then the hot loop runs.
// Run:  ./build/l1_tripwire                 (clean: reserve() was called during warm-up)
//       ./build/l1_tripwire forget-reserve  (trip it: the flat_map has to grow in the hot loop)
#include "hft_mem/hft_mem.hpp"

#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    const bool forget = argc > 1 && std::string_view(argv[1]) == "forget-reserve";

    hft::thread_arena arena;
    hft::flat_map<long, double> levels;       // price level -> size
    hft::map<long, double> orders;            // order id -> size (node container)
    if (!forget) levels.reserve(1024);        // warm-up: size the flat container up front
    for (long i = 0; i < 1000; ++i) orders.emplace(i, 1.0);  // warm-up: pool learns node size

    arena.seal();                             // ---- hot path starts: no new memory allowed ----
    try {
        for (long i = 0; i < 1000; ++i) {
            orders.erase(i);                  // frees a node back to the pool ...
            orders.emplace(1000 + i, 2.0);    // ... and reuses it: allowed, no new memory
            levels[100 + i % 300] += 1.0;     // grows the flat_map unless it was reserved
        }
        std::printf("clean: %zu levels, %zu orders, no new memory on the hot path\n",
                    levels.size(), orders.size());
        return 0;
    } catch (const std::bad_alloc&) {
        std::printf("TRIPWIRE: the hot path asked for new memory (levels.size()=%zu)\n", levels.size());
        return 3;
    }
}
