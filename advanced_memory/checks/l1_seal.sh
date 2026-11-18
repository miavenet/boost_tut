#!/bin/bash
# Card 01-C1: the one statement between warm-up and hot path that makes any NEW memory request throw.
# Answer (stdin): a statement, e.g.  arena.seal();
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include "hft_mem/hft_mem.hpp"
int main() {
    hft::thread_arena arena;
    hft::map<long, long> m;
    for (long i = 0; i < 100; ++i) m[i] = i;            // warm-up
    {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
    }
    m.erase(0); m[100] = 1;                              // recycles a node: must still work
    try {
        hft::vector<long> v; v.reserve(4096);            // brand-new memory: must throw now
        return 1;
    } catch (const std::bad_alloc&) {}
    return m.size() == 100 ? 0 : 1;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
