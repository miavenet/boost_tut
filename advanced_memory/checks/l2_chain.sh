#!/bin/bash
# Card 02-Q2: declare `mono` and `pool` so that pool -> mono -> the 64 KiB `buf` -> null upstream.
# Answer (stdin): two declarations (one line), e.g.
#   std::pmr::monotonic_buffer_resource mono(buf, sizeof buf, std::pmr::null_memory_resource()); std::pmr::unsynchronized_pool_resource pool(&mono);
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <map>
#include <memory_resource>
#include <new>
alignas(64) static std::byte buf[1 << 16];
int main() {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
    std::pmr::memory_resource* top = &pool;
    std::pmr::map<int, int> m(top);
    for (int i = 0; i < 100; ++i) m[i] = i;
    const void* node = &*m.begin();
    if (node < static_cast<const void*>(buf) || node >= static_cast<const void*>(buf + sizeof buf)) return 1;
    try { for (int i = 100; i < 1'000'000; ++i) m[i] = i; return 1; }   // must run out and throw
    catch (const std::bad_alloc&) { return 0; }
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
