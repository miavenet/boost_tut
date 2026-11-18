// l2_dangling.cpp - the reference you kept is not the element you think.
// Build (ASan): clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include l2_dangling.cpp -o l2_dangling
// Run:  ./l2_dangling grow    -> PREDICT: what does AddressSanitizer report?
//       ./l2_dangling shift   -> PREDICT: does ASan fire? what limit prints?
//       ./l2_dangling fixed   -> the repair
#include <boost/container/flat_map.hpp>
#include <cstdio>
#include <cstring>

struct Limit { long max_qty; };
using RiskTable = boost::container::flat_map<int, Limit>;   // instrument id -> limit

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "grow";
    RiskTable t;
    if (std::strcmp(mode, "grow") != 0) t.reserve(64);   // shift/fixed: no reallocation
    for (int id : {100, 200, 300}) t.emplace(id, Limit{id * 10L});

    if (std::strcmp(mode, "fixed") == 0) {
        // Repair: keep the KEY, re-find after any mutation (or copy the value out).
        int key = 200;
        t.emplace(50, Limit{500});                          // a new listing arrives
        std::printf("limit for 200 = %ld (expect 2000)\n", t.find(key)->second.max_qty);
        return 0;
    }
    Limit& lim = t.find(200)->second;                       // cached "for speed"
    for (int id = 1; id <= 40; ++id) t.emplace(id, Limit{id * 1L});  // new listings, all < 100
    std::printf("limit for 200 = %ld (expect 2000)\n", lim.max_qty);  // use after mutation
}
