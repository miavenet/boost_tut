#!/bin/bash
# Card 02-C2: the upstream argument that turns "buffer exhausted" into std::bad_alloc.
# Answer (stdin): an expression of type std::pmr::memory_resource*
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <memory_resource>
#include <new>
int main() {
    alignas(64) std::byte buf[1024];
    std::pmr::monotonic_buffer_resource mono(buf, sizeof buf,
CPP
tr -d ';' < "$WORK/answer.txt"
cat <<'CPP'
    );
    void* small = mono.allocate(512);                    // fits in the buffer
    if (small < static_cast<void*>(buf) || small >= static_cast<void*>(buf + sizeof buf)) return 1;
    try { (void)mono.allocate(4096); return 1; } catch (const std::bad_alloc&) { return 0; }
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
