#!/bin/bash
# Card 04-Q5: define  struct Counters { ... a; ... b; };  two std::atomic<long> counters, each
# bumped by a different thread, that can NOT false-share on the machine you are on.
# Criterion: the two counters are at least one cache line apart (sysctl hw.cachelinesize on
# macOS, getconf LEVEL1_DCACHE_LINESIZE on Linux; 64 if unknown).
. "$(dirname "$0")/lib.inc"
read_answer
LINE="$(sysctl -n hw.cachelinesize 2>/dev/null || getconf LEVEL1_DCACHE_LINESIZE 2>/dev/null || echo 64)"
[ -n "$LINE" ] && [ "$LINE" -gt 0 ] 2>/dev/null || LINE=64
{
cat <<'CPP'
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
CPP
cat "$WORK/answer.txt"
cat <<CPP

int main() {
    static_assert(std::is_same_v<decltype(Counters::a), std::atomic<long>>);
    static_assert(std::is_same_v<decltype(Counters::b), std::atomic<long>>);
    Counters c[2];
    for (auto& x : c) {
        auto d = reinterpret_cast<const char*>(&x.b) - reinterpret_cast<const char*>(&x.a);
        if (d < 0) d = -d;
        if (d < $LINE) return 1;
        if (reinterpret_cast<std::uintptr_t>(&x.a) / $LINE == reinterpret_cast<std::uintptr_t>(&x.b) / $LINE) return 1;
    }
    return 0;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
