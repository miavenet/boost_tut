#!/bin/bash
# Card 03-Q2: body of do_is_equal for a resource that owns its own region.
. "$(dirname "$0")/lib.inc"
read_answer
{
cat <<'CPP'
#include <cstddef>
#include <memory_resource>
#include <new>
class region final : public std::pmr::memory_resource {
    void* do_allocate(std::size_t, std::size_t) override { throw std::bad_alloc(); }
    void do_deallocate(void*, std::size_t, std::size_t) override {}
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
CPP
cat "$WORK/answer.txt"
cat <<'CPP'
    }
};
int main() {
    region a, b;
    std::pmr::unsynchronized_pool_resource p;
    if (!a.is_equal(a)) return 1;                       // itself: equal
    if (a.is_equal(b) || b.is_equal(a)) return 1;      // another region: NOT equal
    if (a.is_equal(p)) return 1;                       // another type: NOT equal
    return 0;
}
CPP
} > "$WORK/a.cpp"
compile_run "$WORK/a.cpp"
