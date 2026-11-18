#!/bin/bash
# 02-Q4: rewrite limit_after_listing so it returns the right limit for id 200
# after the new listings are inserted. Run under AddressSanitizer.
source "$(dirname "$0")/lib.sh"
read_answer > "$WORK/answer.inc"
cat > "$WORK/t.cpp" <<'CPP'
#include <boost/container/flat_map.hpp>
#include <cstdio>
struct Limit { long max_qty; };
using RiskTable = boost::container::flat_map<int, Limit>;
inline void add_listings(RiskTable& t) { for (int id = 1; id <= 40; ++id) t.emplace(id, Limit{id * 1L}); }
#include "answer.inc"
int main() {
  // Two tables with DIFFERENT limits for id 200, so a hard-coded constant cannot pass.
  for (long limit : {2000L, 777L}) {
    RiskTable t;                                 // no reserve: growth WILL reallocate
    for (int id : {100, 200, 300}) t.emplace(id, Limit{id == 200 ? limit : id * 10L});
    long got = limit_after_listing(t);
    if (t.size() != 43) { std::puts("listings were not all added"); return 3; }
    std::printf("got %ld\n", got);
    if (got != limit) return 4;
  }
  return 0;
}
CPP
clang++ -std=c++23 -O1 -g -fsanitize=address -I/opt/homebrew/include "$WORK/t.cpp" -o "$WORK/t" 2>/dev/null || exit 1
ASAN_OPTIONS=detect_leaks=0 "$WORK/t" >/dev/null 2>&1
